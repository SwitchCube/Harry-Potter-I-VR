#pragma once
// HP1's original state machine remains responsible for collision, climbing,
// inventory and story triggers. Only reviewed walking/aiming states are adapted.
using Internal=void(__thiscall*)(void*,void*,void*);
static Internal originalInternal=nullptr;
static void* player=nullptr;
static unsigned movementCalls=0,castMovementCalls=0;
static bool skipActive=false,skipKeyWasDown=false;
static float savedTimeDilation=1;
static void* skipLevel=nullptr;
static ULONGLONG skipStarted=0;
static ULONGLONG replayMoveStarted=0;
static bool replayCastStarted=false,replayDone=false;
static Vec replayCastPosition={};
static unsigned replayCastSamples=0;
static unsigned replayPoseSamples=0;
static bool lastControllerCast=false,lastControllerJump=false,lastControllerMenu=false;
static void* objectField(void* o,const wchar_t* key){return mem<void*>(o,field(o,key,4,L"ObjectProperty"));}
static std::wstring state(void* o){void* frame=mem<void*>(o,0xc);void* s=frame?mem<void*>(frame,0x1c):nullptr;return s?name(s):L"None";}
static bool flag(void* o,const wchar_t* key){
    unsigned off=field(o,key,4,L"BoolProperty");void* p=findProperty(mem<void*>(o,0x24),key);
    DWORD mask=mem<DWORD>(p,0x5c);need(mask&&!(mask&(mask-1)),"HP1 boolean mask differs");return (mem<DWORD>(o,off)&mask)!=0;
}
static void setFlag(void* o,const wchar_t* key,bool value){unsigned off=field(o,key,4,L"BoolProperty");void* p=findProperty(mem<void*>(o,0x24),key);DWORD mask=mem<DWORD>(p,0x5c);need(mask&&!(mask&(mask-1)),"Invalid HP1 flag");if(value)mem<DWORD>(o,off)|=mask;else mem<DWORD>(o,off)&=~mask;}
static bool vrMovement(){return startup.mode==3||startup.mode==6;}
static bool climbFixture(){return startup.mode==6&&GetFileAttributesW((std::wstring(startup.profile)+L"\\test-climb.flag").c_str())!=INVALID_FILE_ATTRIBUTES;}
static bool stereoFixture(){static int enabled=-1;if(enabled<0)enabled=startup.mode==6&&GetFileAttributesW((std::wstring(startup.profile)+L"\\test-stereo.flag").c_str())!=INVALID_FILE_ATTRIBUTES;return enabled!=0;}
static void* stereoFixtureProjectile=nullptr;
static unsigned originalMounts=0;
static bool ownFocus(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
static void* scriptFunction(void* o,const wchar_t* key){
    using MakeName=void(__thiscall*)(void*,const wchar_t*,int);
    using FindFunction=void*(__thiscall*)(void*,DWORD,int);
    auto core=GetModuleHandleW(L"Core.dll");
    auto make=reinterpret_cast<MakeName>(GetProcAddress(core,"??0FName@@QAE@PBGW4EFindName@@@Z"));
    auto find=reinterpret_cast<FindFunction>(GetProcAddress(core,"?FindFunction@UObject@@QAEPAVUFunction@@VFName@@H@Z"));
    need(make&&find,"Missing HP1 script lookup API");DWORD token=0;make(&token,key,0);need(token!=0,"Unknown HP1 function name");
    void* fn=find(o,token,0);need(fn!=nullptr,"Missing HP1 script function");return fn;
}
static void callScript(void* o,const wchar_t* key,void* params){
    auto event=reinterpret_cast<Dispatch>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"?ProcessEvent@UObject@@UAEXPAVUFunction@@PAX1@Z"));
    need(event!=nullptr,"Missing HP1 script call API");event(o,scriptFunction(o,key),params,nullptr);
}
static void callNoArgs(void* o,const wchar_t* key){callScript(o,key,nullptr);}
static void inputKey(int key){
    if(!currentEngine||!currentViewport)return;
    using InputEvent=int(__thiscall*)(void*,void*,int,int,float);
    auto input=reinterpret_cast<InputEvent>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?InputEvent@UGameEngine@@UAEHPAVUViewport@@W4EInputKey@@W4EInputAction@@M@Z"));
    need(input!=nullptr,"Missing HP1 menu input API");input(currentEngine,currentViewport,key,1,0);input(currentEngine,currentViewport,key,3,0);
}
static void replayTravel(){
    static bool requested=false;
    if(startup.mode!=6||!replayDone||requested||GetTickCount64()-replayMoveStarted<8000)return;
    if(GetFileAttributesW((std::wstring(startup.profile)+L"\\test-travel.flag").c_str())==INVALID_FILE_ATTRIBUTES)return;
    void* fn=scriptFunction(player,L"ClientTravel");
    struct Params{DWORD url[3];BYTE kind;BYTE padding[3];DWORD items;} params={};
    for(auto item:{std::pair<const wchar_t*,unsigned>{L"URL",0},{L"TravelType",12},{L"bItems",16}}){
        void* p=findProperty(fn,item.first);need(p&&mem<DWORD>(p,0x48)==item.second,"HP1 ClientTravel parameter layout differs");
    }
    auto core=GetModuleHandleW(L"Core.dll");using StringNew=void(__thiscall*)(void*,const wchar_t*);using StringDelete=void(__thiscall*)(void*);
    auto make=reinterpret_cast<StringNew>(GetProcAddress(core,"??0FString@@QAE@PBG@Z"));auto destroy=reinterpret_cast<StringDelete>(GetProcAddress(core,"??1FString@@QAE@XZ"));
    need(make&&destroy,"Missing HP1 string API");make(params.url,L"Lev_Tut1.unr");requested=true;
    log("{\"event\":\"hp1_replay_travel_requested\",\"private_profile\":true}");
    callScript(player,L"ClientTravel",&params);destroy(params.url);
}
#include "hp1_aim.h"
#include "hp1_lesson.h"
#include "hp1_flight.h"
#include "hp1_levitation.h"
#include "hp1_troll.h"
#include "hp1_flute.h"
#include "hp1_chess.h"
#include "hp1_tutorial.h"
static void __fastcall internalHook(void* self,void*,void* frame,void* result){
    try{if(tutorialInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_tutorial",e.what());}
    try{
        if(vrMovement()&&player&&frame&&!_wcsicmp(name(mem<void*>(frame,4)).c_str(),L"doLevelSave")&&name(mem<void*>(self,0x24))==L"HPConsole"){
            Fpu fpu;void* level=objectField(player,L"Level");
            bool restore=skipActive&&skipLevel==level;float before=restore?mem<float>(level,field(level,L"TimeDilation",4,L"FloatProperty")):0;
            if(restore)mem<float>(level,field(level,L"TimeDilation",4,L"FloatProperty"))=savedTimeDilation;
            fpu.restore();originalInternal(self,frame,result);
            if(restore&&skipLevel==level)mem<float>(level,field(level,L"TimeDilation",4,L"FloatProperty"))=before;
            log("{\"event\":\"hp1_original_save_returned\",\"skip_speed_excluded\":%s}",restore?"true":"false");fpu.restore();return;
        }
    }catch(const std::exception& e){fatal("hp1_save",e.what());}
    try{if(levitationInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_levitation",e.what());}
    try{if(flightInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_flight",e.what());}
    try{if(trollInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_troll",e.what());}
    try{if(chessInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_chess",e.what());}
    try{if(fluteInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_flute",e.what());}
    try{if(lessonInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_lesson",e.what());}
    try{if(menuInternal(self,frame,result))return;}catch(const std::exception& e){log("{\"event\":\"hp1_menu_failed\",\"operation\":\"%s\"}",menuOperation());fatal("hp1_menu",e.what());}
    try{if(cinemaHudInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_cinema",e.what());}
    try{if(storyUiInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_story_ui",e.what());}
    try{if(aimInternal(self,frame,result))return;}catch(const std::exception& e){fatal("hp1_aim",e.what());}
    if(!player||self!=player||!frame||name(mem<void*>(frame,4))!=L"PlayerMove"){
        originalInternal(self,frame,result);return;
    }
    Fpu fpu;
    try{
        auto current=state(self);
        bool walking=current==L"PlayerWalking",aiming=current==L"playeraiming";
        if((!walking&&!aiming)||flag(self,L"bKeepStationary")||flag(self,L"bReverseInput")){
            fpu.restore();originalInternal(self,frame,result);return;
        }
        BYTE physics=mem<BYTE>(self,field(self,L"Physics",1,L"ByteProperty"));
        if(physics!=1&&physics!=2){fpu.restore();originalInternal(self,frame,result);return;}
        bool vrMove=vrMovement();
        if(vrMove){
            float rate=mem<float>(self,field(self,L"AccelRate",4,L"FloatProperty"));need(std::isfinite(rate)&&rate>0&&rate<100000,"Invalid HP1 acceleration");
            mem<float>(self,field(self,L"aForward",4,L"FloatProperty"))=controls.active&&!letterBusy?controls.forward*rate/.08f:0;
            mem<float>(self,field(self,L"aSideMove",4,L"FloatProperty"))=controls.active&&!letterBusy?controls.side*rate/.1f:0;
            mem<float>(self,field(self,L"aTurn",4,L"FloatProperty"))=0;
            mem<BYTE>(self,field(self,L"bAltFire",1,L"ByteProperty"))=controls.active&&controls.cast?1:0;
        }
        if(!vrMove&&inputReplay()&&replayMoveStarted&&!replayDone){
            unsigned long long elapsed=GetTickCount64()-replayMoveStarted;
            // A short sidestep avoids walking into the next tutorial trigger.
            mem<float>(self,field(self,L"aForward",4,L"FloatProperty"))=0;
            mem<float>(self,field(self,L"aSideMove",4,L"FloatProperty"))=elapsed>=1000&&elapsed<2500?300:0;
            mem<BYTE>(self,field(self,L"bAltFire",1,L"ByteProperty"))=elapsed>=1000&&elapsed<2200?1:0;
        }
        float forward=mem<float>(self,field(self,L"aForward",4,L"FloatProperty"))*.08f;
        float side=mem<float>(self,field(self,L"aSideMove",4,L"FloatProperty"))*.1f;
        unsigned vo=field(self,L"Velocity",12,L"StructProperty"),ao=field(self,L"Acceleration",12,L"StructProperty");
        Vec velocity=mem<Vec>(self,vo);bool cast=aiming||flag(self,L"bJustFired")||flag(self,L"bJustAltFired");
        Rot facing=mem<Rot>(self,field(self,L"Rotation",12,L"StructProperty"));
        if(flag(self,L"bScreenRelativeMovement")){void* cam=objectField(self,L"cam");if(cam)facing=mem<Rot>(cam,field(cam,L"Rotation",12,L"StructProperty"));}
        if(vrMove)facing.yaw=controls.yaw;
        Vec wanted=hpvr::yaw({forward,side,0},facing.yaw*hpvr::pi/32768);
        // HP1 APawn::Mount tests Rotation's forward axis against the wall
        // normal (Engine RVA ebd14..ebd60). Acceleration alone cannot fix it.
        // Supply a consistent body, view and stock forward input BEFORE the
        // original PlayerMove and its physics, also after each completed mount.
        bool screenRelative=flag(self,L"bScreenRelativeMovement");
        bool faceMove=vrMove&&controls.active&&!menuVisible&&!flag(self,L"bLockedOnTarget")&&!flag(self,L"bFixedFaceDirection");
        if(faceMove){
            float length=std::hypot(forward,side);
            int direction=length>.001f?int(std::lround(std::atan2(wanted.y,wanted.x)*32768/hpvr::pi)):controls.yaw;
            Rot body={0,direction,0};
            mem<Rot>(self,field(self,L"Rotation",12,L"StructProperty"))=body;
            mem<Rot>(self,field(self,L"ViewRotation",12,L"StructProperty"))=body;
            mem<Rot>(self,field(self,L"DesiredRotation",12,L"StructProperty"))=body;
            mem<int>(self,field(self,L"ScreenRelativeMovementYaw",4,L"IntProperty"))=direction;
            setFlag(self,L"bScreenRelativeMovement",false);
            mem<float>(self,field(self,L"aForward",4,L"FloatProperty"))=length/.08f;
            for(auto key:{L"aSideMove",L"aStrafe",L"aTurn",L"aLookUp"})mem<float>(self,field(self,key,4,L"FloatProperty"))=0;
        }
        fpu.restore();originalInternal(self,frame,result);
        if(faceMove)setFlag(self,L"bScreenRelativeMovement",screenRelative);
        // Original PlayerMove has now handled jumping and state transitions.
        // Do not override a newly entered cutscene, mount or lesson state.
        if(state(self)!=current){fpu.restore();return;}
        if(cast||vrMove){
            mem<Vec>(self,ao)=wanted;
            auto now=mem<Vec>(self,vo);
            if(now.x==0&&now.y==0&&(forward!=0||side!=0)){mem<Vec>(self,vo).x=velocity.x;mem<Vec>(self,vo).y=velocity.y;}
            if((forward!=0||side!=0)&&++castMovementCalls<=8)
                log("{\"event\":\"hp1_cast_locomotion\",\"state\":\"%ls\",\"acceleration\":[%.4f,%.4f,%.4f]}",current.c_str(),wanted.x,wanted.y,wanted.z);
            if(inputReplay()&&(forward!=0||side!=0))++replayCastSamples;
        }
        ++movementCalls;fpu.restore();
    }catch(const std::exception& e){fatal("hp1_movement",e.what());}
}
#include "hp1_climb_test.h"
#include "hp1_lesson_test.h"
#include "hp1_flight_test.h"
#include "hp1_play_test.h"
#include "hp1_mechanics_test.h"
#include "hp1_troll_test.h"
#include "hp1_flute_test.h"
#include "hp1_chess_test.h"
static void gameplayTick(){
    if(!player)return;
    lobbyTick();
    replayTravel();
    void* hud=objectField(player,L"myHUD");void* level=objectField(player,L"Level");if(!hud||!level)return;
    bool cinematic=flag(hud,L"bCutSceneMode")&&!chessActive();
    playReplayTick(cinematic);
    mechanicsReplayTick(cinematic);
    storyFixtureTick(cinematic);
    trollReplayTick(cinematic);
    fluteReplayTick(cinematic);
    chessReplayTick(cinematic);
    lobbyTestTick();
    tutorialTestTick();
    tutorialLiveTestTick(cinematic);
    bool bookHandled=menuScreenButtons();
    bool chessMode=chessButtons();
    bool fluteMode=fluteButtons();
    if(letterInput())return;
    beginLessonFixture(cinematic);
    flightReplayTick(cinematic);
    bool down=(ownFocus()&&(GetAsyncKeyState(VK_RETURN)&0x8000))||((startup.mode==3||playTestMode||mechanicsTestMode||storyTestMode||trollTestMode||fluteTestMode||chessTestMode||flightFixture())&&controls.skip)||(tutorialLiveTestMode&&cinematic&&!inLesson()&&!skipActive);
    if(inputReplay()&&replayCastStarted&&replayPoseSamples<20){
        static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+250;++replayPoseSamples;
            void* cursor=objectField(player,L"rectarget");auto anim=mem<wchar_t*>(player,field(player,L"AnimSequence",4,L"NameProperty"));
            log("{\"event\":\"hp1_cast_state\",\"state\":\"%ls\",\"target\":\"%ls\",\"animation\":\"%ls\",\"frame\":%.4f,\"held\":%u}",state(player).c_str(),cursor?name(cursor).c_str():L"None",anim?anim+6:L"None",mem<float>(player,field(player,L"AnimFrame",4,L"FloatProperty")),unsigned(mem<BYTE>(player,field(player,L"bAltFire",1,L"ByteProperty"))));
        }
    }
    if(startup.mode==3||mechanicsTestMode||trollTestMode||fluteTestMode||chessTestMode||lobbyTestMode){
        if(controls.active&&!cinematic){
            float value=1;
            if(!chessMode&&!fluteMode&&!broomPlayer()&&!menuVisible&&controls.cast&&!lastControllerCast)callScript(player,L"AltFire",&value);
            if(!broomPlayer()&&!menuVisible&&controls.jump&&!lastControllerJump){auto s=state(player);if(s==L"PlayerWalking"||s==L"playeraiming")callScript(player,L"DoJump",&value);}
        }
        if(controls.active&&!bookHandled&&(!cinematic||menuVisible)&&controls.menu&&!lastControllerMenu&&currentEngine&&currentViewport)inputKey(27);
        lastControllerCast=controls.cast;lastControllerJump=controls.jump;lastControllerMenu=controls.menu;
    }
    if(startup.mode==6&&replayDone){
        static unsigned phase=0;auto elapsed=GetTickCount64()-replayMoveStarted;
        if(phase==0&&elapsed>=4500){phase=1;inputKey(27);log("{\"event\":\"hp1_menu_replay_open\"}");}
        else if(phase==1&&elapsed>=5200){phase=2;moveMenuCursor(.5f,.805f,true);}
        else if(phase==2&&elapsed>=6000){phase=3;inputKey(27);}
        else if(phase==3&&elapsed>=6300){phase=4;inputKey(27);}
        else if(phase==4&&elapsed>=6700){phase=5;moveMenuCursor(.217f,.804f,true);}
        else if(phase==5&&elapsed>=7200){phase=6;moveMenuCursor(.5945f,.5906f,true);}
        else if(phase==6&&elapsed>=7500){phase=7;inputKey(27);log("{\"event\":\"hp1_menu_replay_close\"}");}
    }
    if(((down&&!skipKeyWasDown)||(inputReplay()&&!replayMoveStarted))&&cinematic&&!menuVisible&&!skipActive){
        savedTimeDilation=mem<float>(level,field(level,L"TimeDilation",4,L"FloatProperty"));
        need(savedTimeDilation>0&&std::isfinite(savedTimeDilation),"Invalid scene time scale");
        skipLevel=level;skipActive=true;skipStarted=GetTickCount64();
        log("{\"event\":\"hp1_scene_skip_started\",\"story_commands_preserved\":true}");
    }
    skipKeyWasDown=down;
    if(!flightFixture()&&!lessonFixture()&&!climbReplayTick(cinematic)&&inputReplay()&&!cinematic&&!replayDone){
        static int replayHeading=0;
        controls.hand=true;
        controls.wandRotation=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"));
        // Exercise a hand pitch outside HP1's original -8000 lower limit.
        if(startup.mode==6)controls.wandRotation.pitch=-12000;
        float yaw=controls.wandRotation.yaw*hpvr::pi/32768;
        float pitch=controls.wandRotation.pitch*hpvr::pi/32768;
        controls.wandForward={std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),std::sin(pitch)};
        controls.wandUp={-std::sin(pitch)*std::cos(yaw),-std::sin(pitch)*std::sin(yaw),std::cos(pitch)};
        controls.wandBase=hpvr::add(mem<Vec>(player,field(player,L"Location",12,L"StructProperty")),hpvr::add(hpvr::scale(controls.wandForward,16),Vec{0,0,30}));
        auto current=state(player);
        if(!replayMoveStarted&&current==L"PlayerWalking"&&!flag(player,L"bKeepStationary")){
            replayHeading=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty")).yaw;
            replayMoveStarted=GetTickCount64();log("{\"event\":\"hp1_movement_replay_started\"}");
        }
        if(replayMoveStarted){
            auto elapsed=GetTickCount64()-replayMoveStarted;
            // Mode 6 exercises the SAME locomotion adapter as real controllers.
            if(startup.mode==6){float rate=mem<float>(player,field(player,L"AccelRate",4,L"FloatProperty"));controls.active=true;controls.yaw=replayHeading;controls.forward=0;controls.side=elapsed>=1000&&elapsed<2500?30/rate:0;controls.cast=elapsed>=1000&&elapsed<2200;}
            if(elapsed>=1000&&!replayCastStarted&&current==L"PlayerWalking"){
                replayCastStarted=true;replayCastPosition=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));
                // AltFire has one optional float; use a valid parameter block.
                using MakeName=void(__thiscall*)(void*,const wchar_t*,int);using Find=void*(__thiscall*)(void*,DWORD,int);
                auto core=GetModuleHandleW(L"Core.dll");DWORD token=0;
                reinterpret_cast<MakeName>(GetProcAddress(core,"??0FName@@QAE@PBGW4EFindName@@@Z"))(&token,L"AltFire",0);
                void* fn=reinterpret_cast<Find>(GetProcAddress(core,"?FindFunction@UObject@@QAEPAVUFunction@@VFName@@H@Z"))(player,token,0);need(fn!=nullptr,"Replay AltFire absent");float value=1;
                reinterpret_cast<Dispatch>(GetProcAddress(core,"?ProcessEvent@UObject@@UAEXPAVUFunction@@PAX1@Z"))(player,fn,&value,nullptr);
                log("{\"event\":\"hp1_cast_replay_started\",\"state\":\"%ls\"}",state(player).c_str());
            }
            if(elapsed>=3000){
                Vec p=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));float distance=std::hypot(p.x-replayCastPosition.x,p.y-replayCastPosition.y);
                log("{\"event\":\"hp1_movement_replay_complete\",\"cast_samples\":%u,\"distance_during_cast\":%.5f,\"passed\":%s}",replayCastSamples,distance,replayCastSamples>0&&distance>5?"true":"false");
                mem<BYTE>(player,field(player,L"bAltFire",1,L"ByteProperty"))=0;replayDone=true;
            }
        }
    }
    if(!skipActive)return;
    if(level!=skipLevel){skipActive=false;skipLevel=nullptr;return;}
    unsigned dilation=field(level,L"TimeDilation",4,L"FloatProperty");
    if(!cinematic||GetTickCount64()-skipStarted>15000){
        mem<float>(level,dilation)=savedTimeDilation;skipActive=false;skipLevel=nullptr;
        log("{\"event\":\"hp1_scene_skip_stopped\",\"scene_released\":%s}",cinematic?"false":"true");return;
    }
    // HP1 ships an 8x fast-forward path, but gates it behind debug mode.
    // Use that speed without enabling cheats; CutSkip only clears timed waits.
    // Movement cues, triggers, rewards and Release commands still run normally.
    mem<float>(level,dilation)=savedTimeDilation*8;
    void* scene=objectField(hud,L"curCutScene");if(scene)callNoArgs(scene,L"CutSkip");
}
