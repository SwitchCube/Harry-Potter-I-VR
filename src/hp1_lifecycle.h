#pragma once
// Signatures resolved from the fingerprinted HP1 exports, never HP2 addresses.
using LoadMap=void*(__thiscall*)(void*,const void*,void*,const void*,void*);
using Destroy=void(__thiscall*)(void*);
static LoadMap originalLoadMap=nullptr;
static Destroy originalDestroy=nullptr;
static void clearLevelReferences(){
    closeTutorialSounds();tutorialModels.close();
    closeStoryUi();closeCinema();
    settingsOpen=false;settingsButton=settingsOwner=nullptr;settingsWaiting=-1;settingsPanel.close();
    resetLesson();closeLessonPanel();
    player=nullptr;currentViewport=nullptr;controls={};
    menuConsole=nullptr;menuVisible=false;menuTriggerHeld=false;
    handStatus.close();
    flightUi.close();
    menuText.close();
    menuOverlay.close();menuBridge.close();menuCapture.close();
    trackingLevel=nullptr;trackingOrigin.ready=false;primaryEvents.clear();
    lobbyWasVisible=false;lobbyOrigin.ready=false;
    previousCinema=false;sceneReference=hpvr::identity();
    previousTroll=trollMoving=false;trollWanted={};
    resetChess();
    fluteHeld=fluteWasAvailable=fluteArmed=false;fluteMeshMask=0;
    previousFlight=flightRecenterHeld=flightMoving=flightCapturePending=false;flightReference=hpvr::identity();
    flightMotion={};flightMotionPlayer=nullptr;flightMotionTick=0;
    flightAction={};flightActionPlayer=nullptr;flightViewHeading={};flightViewTime=0;
    actions.turned=false;actions.recentered=false;
    lastControllerCast=lastControllerJump=lastControllerMenu=false;
    skipActive=false;skipLevel=nullptr;skipKeyWasDown=false;
    hideDirectBody=false;mirrorPlayerMask=mirrorCaptureMask=mirrorSavedMask=0;
    runtime.renderReady=false;runtime.submittedMask=0;bridge.resetVerification();
}
static void* __fastcall loadMapHook(void* self,void*,const void* url,void* pending,const void* travel,void* error){
    Fpu fpu;
    try{
        need(!pairActive,"Map travel attempted inside a stereo pair");
        if(skipActive&&skipLevel)mem<float>(skipLevel,field(skipLevel,L"TimeDilation",4,L"FloatProperty"))=savedTimeDilation;
        ++loadingDepth;++levelGeneration;clearLevelReferences();
        if(runtime.compositor)runtime.compositor->ClearLastSubmittedFrame();
        log("{\"event\":\"hp1_level_begin\",\"generation\":%u}",levelGeneration);
        fpu.restore();void* result=originalLoadMap(self,url,pending,travel,error);--loadingDepth;
        // Loading draws can have touched renderer state; reacquire on a fresh frame.
        clearLevelReferences();
        log("{\"event\":\"hp1_level_end\",\"generation\":%u,\"loaded\":%s}",levelGeneration,result?"true":"false");
        fpu.restore();return result;
    }catch(const std::exception& e){fatal("hp1_travel",e.what());}
}
static void __fastcall destroyHook(void* self,void*){
    vrProfile.print(trace);
    log("{\"event\":\"hp1_shutdown\",\"frames\":%u,\"level_changes\":%u,\"submitted_pairs\":%u}",vrFrames,levelGeneration,submittedPairs);
    closeTutorialSounds();closeLobby();clearLevelReferences();runtime.close();bridge.close();currentEngine=nullptr;
    originalDestroy(self);
}
