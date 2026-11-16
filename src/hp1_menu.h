#pragma once
#include "vr_hud_capture.h"
#include "vr_menu.h"
#include "hp1_menu_text.h"
static VrHudCapture menuCapture;
static VrBridge menuBridge;
static VrMenu menuOverlay;
static bool menuBusy=false,menuCaptured=false,menuTriggerHeld=false,optionsCaptured=false;
static unsigned menuCaptures=0;
static bool modalCaptured=false;
static bool menuOriginalTextFallback=false;
static const char* menuStage="entry";
static const char* menuOperation(){return menuStage;}
#include "hp1_settings_menu.h"
static void moveMenuCursor(float u,float v,bool click){
    if(!menuConsole||!menuVisible)return;
    if(settingsOpen){settingsPointerX=u;settingsPointerY=v;if(click)settingsClick(u,v);return;}
    void* root=objectField(menuConsole,L"Root");if(!root)return;
    float w=mem<float>(root,field(root,L"WinWidth",4,L"FloatProperty")),h=mem<float>(root,field(root,L"WinHeight",4,L"FloatProperty"));
    struct Mouse{float x,y;} mouse={u*w,v*h};
    mem<float>(menuConsole,field(menuConsole,L"MouseX",4,L"FloatProperty"))=mouse.x;
    mem<float>(menuConsole,field(menuConsole,L"MouseY",4,L"FloatProperty"))=mouse.y;
    callScript(root,L"MoveMouse",&mouse);
    if(click)inputKey(1);
}
// Native book actions are independent of the gameplay cutscene lock. In the
// ending HP1 deliberately waits for Escape on its folio before showing credits.
static bool menuScreenButtons(){
    if(settingsButtons())return true;
    static bool skipHeld=false,backHeld=false;
    bool skip=controls.skip&&!skipHeld,back=controls.menu&&!backHeld;
    skipHeld=controls.skip;backHeld=controls.menu;
    if(!menuConsole)return false;
    void* book=objectField(menuConsole,L"MenuBook");if(!book||!flag(book,L"bIsOpen"))return false;
    void* page=objectField(book,L"curPage");
    bool ending=flag(book,L"bInEndGame")&&page==objectField(book,L"FolioPage");
    bool credits=page&&page==objectField(book,L"CreditsPage");
    // UWindow retains ModalWindow until its next WaitModal call. A closed,
    // hidden confirmation must not block X/B when no mouse event follows.
    if(!controls.active)return ending||credits;
    void* root=objectField(menuConsole,L"Root"),*modal=objectField(root,L"ModalWindow");
    if(modal){
        DWORD waiting=0;need(mem<DWORD>(findProperty(scriptFunction(root,L"WaitModal"),L"ReturnValue"),0x48)==0,"WaitModal result layout differs");
        callScript(root,L"WaitModal",&waiting);if(waiting)return ending||credits;
    }
    if((ending||credits)&&(skip||back)){
        struct Keys{BYTE key,action,padding[2];float delta;DWORD result;} args={27,1,{},0,0};
        void* fn=scriptFunction(book,L"KeyEvent");
        for(auto v:{std::pair<const wchar_t*,unsigned>{L"Key",0},{L"Action",1},{L"Delta",4},{L"ReturnValue",8}})
            need(mem<DWORD>(findProperty(fn,v.first),0x48)==v.second,"HP1 book input layout differs");
        callScript(book,L"KeyEvent",&args);
        log("{\"event\":\"hp1_ending_continue\",\"from\":\"%s\",\"handled\":%s}",ending?"cards":"credits",args.result?"true":"false");
    }else if(page&&page==objectField(book,L"StoryBookPage")&&skip){callNoArgs(page,L"GotoNextPage");}
    return ending||credits;
}
static void updateMenuInput(){
    bool click=controls.cast&&!menuTriggerHeld;menuTriggerHeld=controls.cast;
    if(!controls.active||!menuVisible||!menuConsole||!menuOverlay.visible||!controls.pointerValid)return;
    if(menuOverlay.point(controls.pointer))moveMenuCursor(menuOverlay.cursorX,menuOverlay.cursorY,click);
}
static bool menuInternal(void* self,void* frame,void* result){
    if(settingsInternal(self,frame,result))return true;
    if(menuText.active&&menuBusy&&frame&&name(mem<void*>(frame,4))==L"Paint"){
        auto type=name(mem<void*>(self,0x24));void* locals=mem<void*>(frame,0x10);void* canvas=locals?mem<void*>(locals,0):nullptr;
        if(canvas&&type==L"HPMessageBox")menuText.cover(self,canvas);
        if(canvas&&(type==L"HPMenuOptionCombo"||type==L"HPMenuOptionHSlider")){
            Fpu fpu;float saved=menuText.labelEnd;
            float edge=type==L"HPMenuOptionCombo"?mem<float>(self,field(self,L"WinWidth",4,L"FloatProperty"))-mem<float>(self,field(self,L"EditBoxWidth",4,L"FloatProperty"))-20:mem<float>(self,field(self,L"SliderDrawX",4,L"FloatProperty"))-23;
            menuText.labelEnd=mem<float>(canvas,field(canvas,L"OrgX",4,L"FloatProperty"))+edge*menuText.scale;
            fpu.restore();originalInternal(self,frame,result);menuText.labelEnd=saved;return true;
        }
    }
    if((startup.mode!=3&&startup.mode!=6)||!frame||loadingDepth||menuBusy)return false;
    auto fn=name(mem<void*>(frame,4));
    if(_wcsicmp(fn.c_str(),L"PostRender")||name(mem<void*>(self,0x24))!=L"HPConsole")return false;
    menuStage="read-book";
    menuConsole=self;void* book=objectField(self,L"MenuBook");
    bool open=book&&flag(book,L"bIsOpen");
    if(open!=menuVisible){menuVisible=open;log("{\"event\":\"hp1_menu_visibility\",\"visible\":%s}",open?"true":"false");}
    if(!open){
        settingsOpen=false;settingsWaiting=-1;settingsPanel.close();
        // Do not retain a full-resolution menu target, CPU copy and GDI bitmap
        // throughout the next flight/level. HP1 has a limited x86 address space.
        if(menuCapture.target||menuText.dc)log("{\"event\":\"hp1_menu_resources_released\",\"closed\":true}");
        menuOverlay.close();menuBridge.close();menuCapture.close();menuText.close();menuOriginalTextFallback=false;return false;
    }
    if(!currentViewport||!vrFrames)return false;
    Fpu fpu;
    void* locals=mem<void*>(frame,0x10);void* canvas=locals?mem<void*>(locals,0):nullptr;need(canvas!=nullptr,"HP1 menu canvas missing");
    auto rd=mem<void*>(currentViewport,0x5c);auto device=mem<IDirect3DDevice7*>(rd,0x9a4);need(device!=nullptr,"HP1 menu device missing");
    // HP1's book is a 640x480 layout. Render it at the full pixel width, with
    // its original aspect ratio, separately from the surrounding stereo scene.
    unsigned sx=field(canvas,L"SizeX",4,L"IntProperty"),sy=field(canvas,L"SizeY",4,L"IntProperty");
    int w=mem<int>(canvas,sx),h=w*3/4;
    need(w>=64&&w<=4096&&h<=mem<int>(canvas,sy),"HP1 menu extent invalid");
    // Allocate text before touching the active D3D7 scene. Allocation failure
    // degrades this menu opening to stock bitmap glyphs, never fatal(). Keep
    // canvas/viewport scale: the engine's frame projection still uses eye size.
    unsigned failure=startup.mode==6&&GetFileAttributesW((std::wstring(startup.profile)+L"\\test-menu-memory.flag").c_str())!=INVALID_FILE_ATTRIBUTES?2:0;
    bool hadText=menuText.dc!=nullptr;
    menuStage="text-scratch";
    if(!menuOriginalTextFallback&&!menuText.begin(w,h,failure)){
        menuOriginalTextFallback=true;
        log("{\"event\":\"hp1_menu_text_fallback\",\"win32_error\":%lu,\"gdi_objects\":%lu,\"width\":%d,\"height\":%d,\"scratch_rows\":%u,\"simulated\":%s,\"original_text\":true}",menuText.allocationError,GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS),w,h,Hp1MenuRaster::stripRows,failure?"true":"false");
    }
    if(menuOriginalTextFallback)menuText.active=false;
    else if(!hadText)log("{\"event\":\"hp1_menu_text_raster\",\"width\":%d,\"height\":%d,\"scratch_bytes\":%u}",w,h,w*menuText.rows*4);
    RestoreFields restore;restore.set(canvas,sx,w);restore.set(canvas,sy,h);
    restore.set(canvas,field(canvas,L"ClipX",4,L"FloatProperty"),float(w));restore.set(canvas,field(canvas,L"ClipY",4,L"FloatProperty"),float(h));
    bool windowsMouse=flag(currentViewport,L"bWindowsMouseAvailable");if(startup.mode==3)setFlag(currentViewport,L"bWindowsMouseAvailable",false);
    // The book occupies w by 3w/4, not the square eye surface. Preserve only
    // that region in the borrowed-target fallback; keep all text pixels.
    menuStage="capture-target";
    unsigned captureFailure=startup.mode==6&&GetFileAttributesW((std::wstring(startup.profile)+L"\\test-menu-borrow.flag").c_str())!=INVALID_FILE_ATTRIBUTES?2:0;
    if(!menuCapture.begin(device,captureFailure,unsigned(w),unsigned(h))){menuText.active=false;setFlag(currentViewport,L"bWindowsMouseAvailable",windowsMouse);fpu.restore();return true;}
    settingsNativeButton(book);
    menuStage="original-paint";
    menuBusy=true;fpu.restore();originalInternal(self,frame,result);menuBusy=false;menuText.active=false;
    setFlag(currentViewport,L"bWindowsMouseAvailable",windowsMouse);
    void* page=objectField(book,L"curPage");
    const wchar_t* hint=flag(book,L"bInEndGame")&&page==objectField(book,L"FolioPage")?tr(L"X / B: Zum Abspann",L"X / B: Continue to credits"):page&&page==objectField(book,L"CreditsPage")?tr(L"X / B: Hauptmen\u00fc",L"X / B: Main menu"):nullptr;
    if(hint&&!menuOriginalTextFallback){int size=int(14*menuText.scale);menuText.commands.push_back({hint,w/10,h-int(25*menuText.scale),w*8/10,int(22*menuText.scale),size,{0,0,w,h},RGB(255,235,165),DT_CENTER|DT_SINGLELINE|DT_NOPREFIX});}
    hr(device->EndScene(),"Finish HP1 menu capture");
    menuStage="text-composite";
    DDSURFACEDESC2 textSurface={};textSurface.dwSize=sizeof(textSurface);hr(menuCapture.target->Lock(nullptr,&textSurface,DDLOCK_WAIT,nullptr),"Menu text pixels");
    try{menuText.composite(textSurface);}catch(...){menuCapture.target->Unlock(nullptr);throw;}hr(menuCapture.target->Unlock(nullptr),"Unlock menu text");
    if(!menuCaptured)log("{\"event\":\"hp1_menu_text\",\"commands\":%u,\"scale\":%.3f,\"native_glyphs\":true}",unsigned(menuText.commands.size()),menuText.scale);
    menuStage="diagnostic-capture";
    if(!menuCaptured)capture(menuCapture.target,L"menu-capture.bmp");
    if(lobbyTestMode&&lobbyTestMenuCapture){wchar_t file[80];swprintf(file,80,L"lobby-%u-menu.bmp",lobbyTestMenuCapture);capture(menuCapture.target,file);log("{\"event\":\"hp1_lobby_menu_capture\",\"stage\":%u,\"page\":\"%ls\",\"text_commands\":%u}",lobbyTestMenuCapture,name(page).c_str(),unsigned(menuText.commands.size()));lobbyTestMenuCapture=0;}
    if(startup.mode==6&&!optionsCaptured&&objectField(book,L"curPage")==objectField(book,L"OptionsPage")){
        capture(menuCapture.target,L"menu-options.bmp");optionsCaptured=true;
        log("{\"event\":\"hp1_menu_pointer_click\",\"options_opened\":true}");
    }
    if(startup.mode==6&&!modalCaptured&&objectField(objectField(menuConsole,L"Root"),L"ModalWindow")){
        capture(menuCapture.target,L"menu-confirm.bmp");modalCaptured=true;log("{\"event\":\"hp1_menu_confirmation\",\"captured\":true,\"text_occluders\":%u}",unsigned(menuText.occluders.size()));
    }
    // PC diagnostics exercise the SAME D3D11 menu transfer as live VR. They
    // omit only headset overlay submission, not allocation/upload/readback.
    if(startup.mode==3||startup.mode==6){
        menuStage="gpu-open";bool opened=!menuBridge.width;
        DDSURFACEDESC2 d={};d.dwSize=sizeof(d);hr(menuCapture.target->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT,nullptr),"Read HP1 menu");
        try{d.dwWidth=w;d.dwHeight=h;menuBridge.open(startup.mode==3?runtime.adapter:0,w,h,true,&bridge);menuStage="gpu-upload";menuBridge.uploadArgb(d,!(menuBridge.verifiedMask&1),menuCapture.sourceAlpha);}catch(...){menuCapture.target->Unlock(nullptr);throw;}
        hr(menuCapture.target->Unlock(nullptr),"Unlock HP1 menu");menuBridge.flush();
        if(opened)log("{\"event\":\"hp1_menu_transfer\",\"width\":%d,\"height\":%d,\"cpu_bytes\":%u,\"background_bytes\":%u,\"verified\":%s,\"live\":%s}",w,h,unsigned(menuBridge.pixels.size()),unsigned(menuCapture.backgroundBytes()),menuBridge.verifiedMask&1?"true":"false",startup.mode==3?"true":"false");
        menuStage="overlay-submit";
        if(settingsOpen){settingsPaint();settingsPanel.upload(startup.mode==3?runtime.adapter:0,&bridge);}
        if(startup.mode==3){if(runtime.compositor->CanRenderScene()){menuOverlay.width=1.65f;menuOverlay.distance=1.3f;auto fixed=lobbyScreenPose(2.4f);menuOverlay.submit(runtime,settingsOpen?settingsPanel.texture.textures[0]:menuBridge.textures[0],settingsOpen?1000:w,settingsOpen?800:h,false,!settingsOpen,lobbyWasVisible?&fixed:nullptr);}else menuOverlay.hide();}
    }
    menuStage="restore-scene";
    menuCapture.restore(device);hr(device->BeginScene(),"Resume HP1 after menu");
    if(!menuCaptured)log("{\"event\":\"hp1_menu_captured\",\"width\":%d,\"height\":%d,\"world_separate\":true}",w,h);
    menuCaptured=true;++menuCaptures;fpu.restore();return true;
}
