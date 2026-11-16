#pragma once
struct Hp1Controls {
    bool active=false,cast=false,jump=false,menu=false,skip=false,recenter=false,hand=false;
    float forward=0,side=0,turn=0;int yaw=0;
    Vec wandBase={},wandForward={},wandUp={};Rot wandRotation={};
    hpvr::Mat physicalWand={};
    hpvr::Mat pointer={};bool pointerValid=false;
    hpvr::Mat leftGrip={};bool leftValid=false,showStatus=false;
};
static Hp1Controls controls;
static void* currentEngine=nullptr;
static void* currentViewport=nullptr;
static void* menuConsole=nullptr;
static bool menuVisible=false;
static bool letterBusy=false;
static bool menuInternal(void*,void*,void*);
static const char* menuOperation();
static void updateMenuInput();
static void moveMenuCursor(float,float,bool);
static bool letterInput();
static bool storyUiInternal(void*,void*,void*);
static void storyUiVisibility();
static void storyFixtureTick(bool);
static bool lobbyRendering=false;
static void lobbyTick();
static bool lobbyWanted();
static bool menuScreenButtons();

static void lobbyTestTick();
static unsigned lobbyTestCapture=0;
static unsigned lobbyTestEyes=0;
static unsigned lobbyTestMenuCapture=0;
static void tutorialTestTick();
static void tutorialLiveTestTick(bool);
static void tutorialLessonPhase(void*,bool);
static void tutorialLessonClosed(void*);
static void tutorialTestRender();
static unsigned tutorialTestCapture=0;

static bool cinemaHudInternal(void*,void*,void*);
