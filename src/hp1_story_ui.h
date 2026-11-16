#pragma once
#include "hp1_letter_panel.h"
// These paths are driven by the ORIGINAL HUD's draw calls. In particular, we
// never enumerate nearby enemies or infer a boss fight from actor health.
struct Hp1BossUi {
 VrHudCapture capture;VrBridge texture;VrMenu overlay{"boss"};unsigned seenTick=0,visiblePixels=0,draws=0;
 void* reportedTarget=nullptr;
 void hide(){if(reportedTarget)log("{\"event\":\"hp1_boss_ui\",\"visible\":false}");reportedTarget=nullptr;overlay.hide();visiblePixels=0;}
 void close(){overlay.close();texture.close();capture.close();seenTick=visiblePixels=draws=0;reportedTarget=nullptr;}
};
static Hp1BossUi bossUi;
static Hp1LetterPanel letterView;
static VrMenu letterOverlay{"letter"};
static VrHudCapture letterCapture;
static VrBridge letterTexture;
static void* letterObject=nullptr;
static bool letterArmed=false,letterSkipOwned=false,letterDirty=false,letterUsesScroll=false;
static int letterRequestedPage=0,letterStick=0;
static ULONGLONG letterOpened=0;
static std::wstring letterText;
static unsigned letterPaints=0,letterCloses=0;
static void* storyBossTarget=nullptr;
static int storyBossPhase=0,storyBossCaptured=-1;
static bool storyBossHalf=false,storyBossSuppress=false,storyFixtureBusy=false;
static bool isLetter(void* popup){
 if(!popup||flag(popup,L"bDeleteMe"))return false;
 auto type=name(mem<void*>(popup,0x24));
 return (findProperty(mem<void*>(popup,0x24),L"textName")&&findProperty(mem<void*>(popup,0x24),L"text"))||type==L"demoLetter"||type==L"masterLetter"||type==L"congratsLetter"||type==L"sorryLetter";
}
static std::wstring letterString(void* popup){
 if(!findProperty(mem<void*>(popup,0x24),L"text"))return L"";
 unsigned off=field(popup,L"text",12,L"StrProperty");int count=mem<int>(popup,off+4),capacity=mem<int>(popup,off+8);auto data=mem<wchar_t*>(popup,off);
 need(count>=0&&count<=32768&&capacity>=count,"Letter string extent differs");if(!count)return L"";need(data&&data[count-1]==0,"Letter string is not terminated");return std::wstring(data,count-1);
}
static bool letterHasScrollOwner(void* popup){
 // HP1's reviewed ULevel actor array. Match the ORIGINAL scroll's popup
 // reference, not a class-name guess or a nearby world object.
 void* level=mem<void*>(player,0x80);auto actors=mem<void**>(level,0x2c);int count=mem<int>(level,0x30);
 need(actors&&count>0&&count<100000,"Letter owner actor array");
 for(int i=0;i<count;++i)if(actors[i]&&!flag(actors[i],L"bDeleteMe")){
  void* prop=findProperty(mem<void*>(actors[i],0x24),L"popup");
  if(prop&&name(mem<void*>(prop,0x24))==L"ObjectProperty"&&objectField(actors[i],L"popup")==popup)return true;
 }
 return false;
}
static void closeStoryUi(){
 bossUi.close();letterOverlay.close();letterView.close();letterCapture.close();letterTexture.close();letterObject=nullptr;letterText.clear();letterArmed=letterSkipOwned=letterDirty=letterBusy=letterUsesScroll=false;letterRequestedPage=letterStick=0;
}
static bool letterInput(){
 if(!vrMovement()||!player)return false;
 void* hud=objectField(player,L"myHUD");void* popup=hud?objectField(hud,L"curPopup"):nullptr;
 if(!isLetter(popup)){
  bool consumed=letterSkipOwned;
  if(letterSkipOwned){setFlag(player,L"bSkipKeyPressed",false);letterSkipOwned=false;}
  if(letterObject){++letterCloses;log("{\"event\":\"hp1_letter_closed\",\"native_popup_finished\":true}");letterOverlay.close();letterView.close();letterCapture.close();letterTexture.close();letterText.clear();}
  letterObject=nullptr;letterOverlay.hide();letterArmed=letterBusy=false;return consumed;
 }
 if(popup!=letterObject){
  letterObject=popup;letterText.clear();letterOverlay.hide();letterArmed=false;letterOpened=GetTickCount64();letterRequestedPage=letterStick=0;letterDirty=true;
  letterUsesScroll=letterHasScrollOwner(popup);
  log("{\"event\":\"hp1_letter_open\",\"class\":\"%ls\"}",name(mem<void*>(popup,0x24)).c_str());
 }
 letterBusy=true;
 // Stop a previously requested cinematic fast-forward at a letter. Reading is
 // user-paced; a held casting/skip button at arrival must not dismiss the post.
 if(skipActive&&skipLevel){mem<float>(skipLevel,field(skipLevel,L"TimeDilation",4,L"FloatProperty"))=savedTimeDilation;skipActive=false;skipLevel=nullptr;}
 bool pressed=controls.skip||controls.cast;
 if(!menuVisible&&controls.active){
  if(!pressed&&GetTickCount64()-letterOpened>=600)letterArmed=true;
  if(pressed&&letterArmed){
   // A scroll must perform CutRelease BEFORE destroying its popup. Sending
   // clearMessages too could destroy it first and strand Harry in CutDoIdle.
   if(letterUsesScroll){setFlag(player,L"bSkipKeyPressed",true);letterSkipOwned=true;}
   else setFlag(player,L"clearMessages",true);
   letterArmed=false;
  }
  int stick=controls.forward>.6f?-1:controls.forward<-.6f?1:0;
  if(stick&&!letterStick){letterRequestedPage=std::max(0,std::min(letterView.pages-1,letterView.page+stick));letterDirty=true;}
  letterStick=stick;
 }
 lastControllerCast=controls.cast;lastControllerJump=controls.jump;lastControllerMenu=controls.menu;skipKeyWasDown=controls.skip;
 return true;
}
static void storyUiVisibility(){
 void* hud=player?objectField(player,L"myHUD"):nullptr;
 bool ready=startup.mode==6||(runtime.compositor&&runtime.compositor->CanRenderScene());
 if(!hud||!ready||menuVisible||flag(hud,L"bCutSceneMode")||ticks>bossUi.seenTick+1)bossUi.hide();
 if(!hud||!ready||menuVisible||!isLetter(objectField(hud,L"curPopup")))letterOverlay.hide();
}
static bool storyUiInternal(void* self,void* frame,void* result){
 if(!vrMovement()||loadingDepth||!player||!currentViewport||!frame||self!=objectField(player,L"myHUD"))return false;
 auto fn=name(mem<void*>(frame,4));bool boss=fn==L"DrawEnemyHealth";
 if(storyTestMode==1&&fn==L"PostRender"&&storyBossTarget&&!storyFixtureBusy){
  RestoreFields fixture;
  fixture.set(player,field(player,L"BossTarget",4,L"ObjectProperty"),storyBossTarget);
  auto bit=[&](void* o,const wchar_t* key,bool value){unsigned off=field(o,key,4,L"BoolProperty"),mask=mem<DWORD>(findProperty(mem<void*>(o,0x24),key),0x5c);fixture.set(o,off,value?(mem<DWORD>(o,off)|mask):(mem<DWORD>(o,off)&~mask));};
  bit(self,L"bCutSceneMode",storyBossSuppress);
  auto number=[&](const wchar_t* key,int value){fixture.set(storyBossTarget,field(storyBossTarget,key,4,L"IntProperty"),value);};
  if(findProperty(mem<void*>(storyBossTarget,0x24),L"hitCount"))fixture.set(storyBossTarget,field(storyBossTarget,L"hitCount",4,L"FloatProperty"),storyBossHalf?2.0f:4.0f);
  if(name(mem<void*>(storyBossTarget,0x24))==L"BossQuirrel")number(L"Health",storyBossHalf?40:80);
  if(findProperty(mem<void*>(storyBossTarget,0x24),L"iNumHits")){number(L"iNumHitsToBeat",4);number(L"iNumHits",storyBossHalf?2:0);}
  if(findProperty(mem<void*>(storyBossTarget,0x24),L"Bumps")){number(L"BumpsToWin",4);number(L"Bumps",storyBossHalf?2:0);}
  if(name(mem<void*>(storyBossTarget,0x24))==L"fluffy"){
   unsigned heads=field(storyBossTarget,L"Heads",4,L"ObjectProperty");
   for(unsigned i=0;i<3;++i){void* head=mem<void*>(storyBossTarget,heads+i*4);need(head,"Fluffy native head");bit(head,L"bSleeping",storyBossHalf&&i==1);
    float period=mem<float>(head,field(head,storyBossHalf&&i==1?L"SleepTime":L"ListenTime",4,L"FloatProperty"));fixture.set(head,field(head,L"_Timer",4,L"FloatProperty"),period*(storyBossHalf?(.2f+.25f*i):1.0f));}
  }
  Fpu fpu;storyFixtureBusy=true;fpu.restore();originalInternal(self,frame,result);storyFixtureBusy=false;fpu.restore();return true;
 }
 void* popup=objectField(self,L"curPopup");bool letter=fn==L"DrawPopup"&&isLetter(popup);
 if(!boss&&!letter)return false;
 if(boss&&!objectField(player,L"BossTarget")){bossUi.hide();return false;}
 if(menuVisible){if(boss)bossUi.hide();else letterOverlay.hide();return false;}
 void* locals=mem<void*>(frame,0x10),*canvas=locals?mem<void*>(locals,0):nullptr;need(canvas,"Story HUD canvas");
 Fpu fpu;
 if(letter&&findProperty(mem<void*>(popup,0x24),L"text")){
  // The original Draw resolves Localize(Pickup...) before we read the text.
  fpu.restore();originalInternal(self,frame,result);letterInput();auto text=letterString(popup);
  if(!text.empty()){
   if(text!=letterText||letterDirty||!letterView.panel.dc){letterText=text;letterView.paint(text,letterRequestedPage);letterView.panel.upload(startup.mode==3?runtime.adapter:0,startup.mode==3?&bridge:nullptr);letterDirty=false;++letterPaints;
    log("{\"event\":\"hp1_letter_rendered\",\"characters\":%u,\"page\":%d,\"pages\":%d,\"localized_original_text\":true,\"gpu_verified\":%s}",unsigned(text.size()),letterView.page+1,letterView.pages,(letterView.panel.texture.verifiedMask&1)?"true":"false");
   }
   if(startup.mode==3&&runtime.compositor&&runtime.compositor->CanRenderScene()){letterOverlay.width=.86f;letterOverlay.distance=1.05f;letterOverlay.submit(runtime,letterView.panel.texture.textures[0],1000,720);}
  }
  fpu.restore();return true;
 }
 // Native boss art includes its own GetHealth/GetSpecialHealth calls, unknown
 // boss rejection, and Fluffy's three independent awake/asleep heads.
 auto& grab=boss?bossUi.capture:letterCapture;auto& texture=boss?bossUi.texture:letterTexture;auto& overlay=boss?bossUi.overlay:letterOverlay;
 if(boss)bossUi.seenTick=ticks;
 auto device=mem<IDirect3DDevice7*>(mem<void*>(currentViewport,0x5c),0x9a4);need(device,"Story HUD device");
 unsigned w=640,h=boss?160:480;
 if(!grab.begin(device,0,w,h))return false;
 RestoreFields restore;
 for(auto key:{L"SizeX",L"SizeY"})restore.set(canvas,field(canvas,key,4,L"IntProperty"),int(key[4]==L'X'?w:h));
 for(auto key:{L"OrgX",L"OrgY",L"CurX",L"CurY"})restore.set(canvas,field(canvas,key,4,L"FloatProperty"),0.0f);
 restore.set(canvas,field(canvas,L"ClipX",4,L"FloatProperty"),float(w));restore.set(canvas,field(canvas,L"ClipY",4,L"FloatProperty"),float(h));
 fpu.restore();originalInternal(self,frame,result);hr(device->EndScene(),"Finish story HUD");
 DDSURFACEDESC2 d={};d.dwSize=sizeof(d);hr(grab.target->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT,nullptr),"Read story HUD");unsigned pixels=0;
 try{d.dwWidth=w;d.dwHeight=h;texture.open(startup.mode==3?runtime.adapter:0,w,h,true,startup.mode==3?&bridge:nullptr);pixels=texture.uploadArgb(d,!(texture.verifiedMask&1),grab.sourceAlpha);}catch(...){grab.target->Unlock(nullptr);throw;}
 hr(grab.target->Unlock(nullptr),"Unlock story HUD");texture.flush();
 if(boss){bossUi.visiblePixels=pixels;++bossUi.draws;
  void* target=pixels?objectField(player,L"BossTarget"):nullptr;
  if(target&&target!=bossUi.reportedTarget)log("{\"event\":\"hp1_boss_ui\",\"visible\":true,\"class\":\"%ls\",\"native_health_art\":true,\"pixels\":%u}",name(mem<void*>(target,0x24)).c_str(),pixels);
  if(!target&&bossUi.reportedTarget)log("{\"event\":\"hp1_boss_ui\",\"visible\":false}");bossUi.reportedTarget=target;
 }
 if(boss&&storyTestMode==1&&storyBossCaptured!=storyBossPhase){
  wchar_t file[64];std::swprintf(file,64,L"boss-%d.bmp",storyBossPhase);capture(grab.target,file);storyBossCaptured=storyBossPhase;
  log("{\"event\":\"hp1_boss_ui_sample\",\"phase\":%d,\"class\":\"%ls\",\"half\":%s,\"pixels\":%u,\"gpu_verified\":%s}",storyBossPhase,name(mem<void*>(storyBossTarget,0x24)).c_str(),storyBossHalf?"true":"false",pixels,(texture.verifiedMask&1)?"true":"false");
 }
 if(pixels&&startup.mode==3&&runtime.compositor&&runtime.compositor->CanRenderScene()){
  overlay.width=boss?.95f:.86f;overlay.distance=boss?1.2f:1.05f;overlay.verticalOffset=boss?-.45f:0;
  overlay.submit(runtime,texture.textures[0],w,h,boss,true);
 }else if(!pixels)overlay.hide();
 grab.restore(device);hr(device->BeginScene(),"Resume after story HUD");fpu.restore();return true;
}
#include "hp1_story_ui_test.h"
