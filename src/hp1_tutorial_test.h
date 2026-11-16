#pragma once
// Explicit private headset fixture observes the ORIGINAL spell tutorial only.
// No injected speech; start-lesson.flag enters the original SpellLearnTrigger.
static void tutorialLiveTestTick(bool cinematic){
 if(!tutorialLiveTestMode||!player||!runtime.input)return;
 static ULONGLONG statusAt=0;auto tick=GetTickCount64();
 if(tick>=statusAt){statusAt=tick+5000;log("{\"event\":\"hp1_tutorial_live_status\",\"cinematic\":%s,\"in_lesson\":%s,\"drawing\":%s,\"input_active\":%s,\"menu\":%s}",cinematic?"true":"false",inLesson()?"true":"false",lessonDrawing?"true":"false",controls.active?"true":"false",menuVisible?"true":"false");}
}
// Private game integration + deterministic OpenVR driver contract fixture.
// No real headset/controller acceptance is implied by these fake device meshes.
static bool tutorialRemapped=false;
static VR_IVRInput_FnTable tutorialFakeInput={};
static VR_IVRCompositor_FnTable tutorialFakeCompositor={};
static VR_IVRSystem_FnTable tutorialFakeSystem={};
static VR_IVRRenderModels_FnTable tutorialFakeModels={};
static RenderModel_Vertex_t tutorialFakeVertices[8]={};
static uint16_t tutorialFakeIndices[]={0,1,2,0,2,3,4,6,5,4,7,6,0,4,5,0,5,1,2,6,7,2,7,3,0,3,7,0,7,4,1,5,6,1,6,2};
static RenderModel_t tutorialFakeMesh={tutorialFakeVertices,8,tutorialFakeIndices,12,0};
static bool OPENVR_FNTABLE_CALLTYPE tutorialFakeReady(){return true;}
static void OPENVR_FNTABLE_CALLTYPE tutorialFakeClear(){}
static EVRInputError OPENVR_FNTABLE_CALLTYPE tutorialFakeBinding(VRActionHandle_t action,InputBindingInfo_t* out,uint32_t size,uint32_t count,uint32_t* returned){
 need(size==sizeof(*out)&&count>=1,"Binding ABI");*out={};*returned=1;
 bool inventory=action==4;
 strcpy(out->rchDevicePathName,inventory?(tutorialRemapped?"/user/hand/right":"/user/hand/left"):action==3||tutorialRemapped?"/user/hand/left":"/user/hand/right");
 strcpy(out->rchInputPathName,inventory?(tutorialRemapped?"/input/trigger":"/input/grip"):tutorialRemapped?"/input/grip":action==1?"/input/a":action==2?"/input/trigger":"/input/joystick");strcpy(out->rchSlotName,action==3?"position":"click");return EVRInputError_VRInputError_None;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE tutorialFakeOrigins(VRActionSetHandle_t,VRActionHandle_t action,VRInputValueHandle_t* out,uint32_t count){need(count>0,"Origin count");out[0]=action;return EVRInputError_VRInputError_None;}
static EVRInputError OPENVR_FNTABLE_CALLTYPE tutorialFakeInfo(VRInputValueHandle_t origin,InputOriginInfo_t* out,uint32_t size){
 need(size==sizeof(*out),"Origin ABI");*out={};out->trackedDeviceIndex=origin==4?(tutorialRemapped?1:2):origin==3||tutorialRemapped?2:1;out->devicePath=out->trackedDeviceIndex;
 strcpy(out->rchRenderModelComponentName,origin==4?(tutorialRemapped?"trigger":"grip"):tutorialRemapped?"grip":origin==1?"a":origin==2?"trigger":"joystick");return EVRInputError_VRInputError_None;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE tutorialFakeLabel(VRInputValueHandle_t,char* text,uint32_t size,int32_t){strncpy(text,"Fixture button",size);return EVRInputError_VRInputError_None;}
static uint32_t OPENVR_FNTABLE_CALLTYPE tutorialFakeProperty(TrackedDeviceIndex_t,ETrackedDeviceProperty,char* text,uint32_t size,ETrackedPropertyError* error){strncpy(text,"hp1_fixture_controller",size);*error=ETrackedPropertyError_TrackedProp_Success;return 23;}
static uint32_t OPENVR_FNTABLE_CALLTYPE tutorialFakeCount(char*){return 5;}
static const char* tutorialFakeNames[]={"body","a","trigger","joystick","grip"};
static uint32_t OPENVR_FNTABLE_CALLTYPE tutorialFakeComponent(char*,uint32_t index,char* text,uint32_t size){strncpy(text,tutorialFakeNames[index],size);return strlen(text)+1;}
static uint32_t OPENVR_FNTABLE_CALLTYPE tutorialFakePart(char*,char* name,char* text,uint32_t size){strncpy(text,name,size);return strlen(text)+1;}
static EVRRenderModelError OPENVR_FNTABLE_CALLTYPE tutorialFakeLoad(char*,RenderModel_t** model){*model=&tutorialFakeMesh;return EVRRenderModelError_VRRenderModelError_None;}
static void OPENVR_FNTABLE_CALLTYPE tutorialFakeFree(RenderModel_t*){}
static bool OPENVR_FNTABLE_CALLTYPE tutorialFakePose(char*,char* component,VRInputValueHandle_t,RenderModel_ControllerMode_State_t*,RenderModel_ComponentState_t* out){
 *out={};auto p=hpvr::identity();int n=0;for(int i=0;i<5;++i)if(!std::strcmp(component,tutorialFakeNames[i]))n=i;
 float scale=n?.28f:1.f;for(int i=0;i<3;++i)p.m[i][i]=scale;
 if(n){p.m[0][3]=(n-2)*.024f;p.m[1][3]=.040f;p.m[2][3]=-.025f;}
 std::memcpy(&out->mTrackingToComponentRenderModel,&p,sizeof(p));out->uProperties=EVRComponentProperty_VRComponentProperty_IsVisible;return true;
}
static void tutorialTestDriver(){
 for(unsigned i=0;i<8;++i){auto& v=tutorialFakeVertices[i];v.vPosition.v[0]=((i==1||i==2||i==5||i==6)?1:-1)*.03f;v.vPosition.v[1]=(i>=4?1:-1)*.035f;v.vPosition.v[2]=((i==2||i==3||i==6||i==7)?1:-1)*.07f;v.vNormal.v[1]=1;}
 tutorialFakeInput.GetActionBindingInfo=tutorialFakeBinding;tutorialFakeInput.GetActionOrigins=tutorialFakeOrigins;tutorialFakeInput.GetOriginTrackedDeviceInfo=tutorialFakeInfo;tutorialFakeInput.GetOriginLocalizedName=tutorialFakeLabel;
 tutorialFakeSystem.GetStringTrackedDeviceProperty=tutorialFakeProperty;tutorialFakeCompositor.CanRenderScene=tutorialFakeReady;tutorialFakeCompositor.ClearLastSubmittedFrame=tutorialFakeClear;
 tutorialFakeModels.GetComponentCount=tutorialFakeCount;tutorialFakeModels.GetComponentName=tutorialFakeComponent;tutorialFakeModels.GetComponentRenderModelName=tutorialFakePart;tutorialFakeModels.LoadRenderModel_Async=tutorialFakeLoad;tutorialFakeModels.FreeRenderModel=tutorialFakeFree;tutorialFakeModels.GetComponentStateForDevicePath=tutorialFakePose;
 runtime.input=&tutorialFakeInput;runtime.system=&tutorialFakeSystem;runtime.compositor=&tutorialFakeCompositor;tutorialModels.api=&tutorialFakeModels;
 actions.set=1;actions.handles["jump"]=1;actions.handles["cast"]=2;actions.handles["move"]=3;actions.handles["show_health"]=4;
 for(unsigned i=0;i<3;++i){auto p=hpvr::identity();if(i){p.m[0][3]=i==1?.20f:-.20f;p.m[1][3]=-.15f;p.m[2][3]=-.40f;}std::memcpy(&runtime.poses[i].mDeviceToAbsoluteTracking,&p,sizeof(p));runtime.poses[i].bPoseIsValid=runtime.poses[i].bDeviceIsConnected=true;}
}
static float tutorialTestLookup(const Hp1TutorialLine& entry,bool adapted=true){
 void* narrator=objectField(player,L"theNarrator");need(narrator,"Tutorial narrator absent");void* fn=scriptFunction(narrator,L"FindDialog");
 for(auto item:{std::pair<const wchar_t*,unsigned>{L"dialogID",0},{L"dlgSound",12},{L"dlgText",16},{L"ReturnValue",28}}){auto p=findProperty(fn,item.first);need(p&&mem<DWORD>(p,0x48)==item.second,"Tutorial FindDialog parameter ABI");}
 struct Params{DWORD id[3];void* sound;DWORD text[3];DWORD result;} args={};
 using Make=void(__thiscall*)(void*,const wchar_t*);using Delete=void(__thiscall*)(void*);auto core=GetModuleHandleW(L"Core.dll");
 auto make=reinterpret_cast<Make>(GetProcAddress(core,"??0FString@@QAE@PBG@Z"));auto destroy=reinterpret_cast<Delete>(GetProcAddress(core,"??1FString@@QAE@XZ"));need(make&&destroy,"Tutorial fixture strings");
 make(args.id,entry.id);unsigned before=tutorialReplacements;callScript(narrator,L"FindDialog",&args);
 need(args.sound&&args.result,"Native dialogue lookup failed");auto text=tutorialString(&args,16);unsigned variant=tutorialRemapped?1:0;
 if(adapted){need(tutorialReplacements==before+1,"Tutorial sound was not replaced once");need(text==entry.text[variant],"Tutorial subtitle differs");need(name(args.sound)==std::wstring(entry.sound[variant]).substr(std::wstring(entry.sound[variant]).find(L'.')+1),"Tutorial sound identity differs");}
 else need(tutorialReplacements==before&&text!=entry.text[variant],"Unrelated language/line replaced");
 void* durationFn=scriptFunction(player,L"GetSoundDuration");need(mem<DWORD>(findProperty(durationFn,L"Sound"),0x48)==0&&mem<DWORD>(findProperty(durationFn,L"ReturnValue"),0x48)==4,"Sound duration ABI");
 struct Duration{void* sound;float value;} duration={args.sound,0};callScript(player,L"GetSoundDuration",&duration);
 if(adapted)need(std::fabs(duration.value-entry.seconds[variant])<.06f&&duration.value>1,"Native sound duration differs from audio");
 destroy(args.id);destroy(args.text);
 log("{\"event\":\"hp1_tutorial_lookup_verified\",\"id\":\"%ls\",\"variant\":%u,\"duration\":%.6f,\"adapted\":%s}",entry.id,variant,duration.value,adapted?"true":"false");return duration.value;
}
static void tutorialTestRender(){
 tutorialTestDriver();tutorialModels.update(true,false);
}
static void tutorialTestTick(){
 if(!tutorialTestMode||!menuConsole)return;
 static unsigned phase=0,checks=0,generation=0,captured=0;static ULONGLONG at=0;auto now=GetTickCount64();if(!at)at=now;controls={};controls.active=true;
 // Capture the fully visible CURRENT cue before starting the next cue's fade-in.
 if(phase>=1&&phase<=4&&now-at>=1100&&!(captured&(1u<<phase))){tutorialTestCapture=phase;captured|=1u<<phase;return;}
 if(phase==9&&now-at>=1100&&!(captured&(1u<<5))){tutorialTestCapture=5;captured|=1u<<5;return;}
 if(now-at<1500)return;
 tutorialTestDriver();
 if(phase==0){
  vrPreferences.english=false;for(int i=0;i<6;++i)vrPreferences.source[i]=i;vrPreferences.swapSticks=false;
  for(const auto& row:tutorialLines){tutorialRemapped=false;tutorialTestLookup(row);tutorialRemapped=true;tutorialTestLookup(row);checks+=2;}
  vrPreferences.english=true;for(const auto& row:tutorialLinesEn){tutorialRemapped=false;tutorialTestLookup(row);tutorialRemapped=true;tutorialTestLookup(row);checks+=2;}vrPreferences.english=false;
  tutorialRemapped=false;tutorialTestLookup(tutorialLines[3]);phase=1;at=GetTickCount64();return;
 }
 if(phase==1){need(tutorialModels.drawnMask==3,"Controller was not drawn in both eyes");tutorialTestLookup(tutorialLines[10]);phase=2;at=now;return;}
 if(phase==2){tutorialTestLookup(tutorialLines[0]);phase=3;at=now;return;}
 if(phase==3){tutorialRemapped=true;tutorialTestLookup(tutorialLines[3]);phase=4;at=now;return;}
 if(phase==4){if(now-at<2100)return;need(tutorialModels.devices.size()==1&&tutorialModels.devices[0].index==2&&tutorialModels.devices[0].highlights[0]=="grip","Remapped action highlighted wrong hand/button");
  controls.jump=true;tutorialAlpha(true,true);need(tutorialFade!=0,"Tutorial success did not fade");phase=5;at=now;return;}
 if(phase==5){need(!tutorialLine,"Tutorial failed to fade out");tutorialRemapped=false;
  vrPreferences.english=true;tutorialTestLookup(tutorialLinesEn[3]);vrPreferences.english=false;
  Hp1TutorialLine unrelated={L"FRED_GEORGE_007",{L"unchanged",L"unchanged"},{L"",L""},{0,0},{"jump",""}};tutorialTestLookup(unrelated,false);
  tutorialTestLookup(tutorialLines[3]);controls.skip=true;tutorialModels.update(true,false);need(!tutorialLine&&!tutorialModels.alpha,"Skip retained tutorial");controls.skip=false;
  tutorialTestLookup(tutorialLines[3]);tutorialModels.update(true,false);runtime.poses[1].bPoseIsValid=false;
  // Listener still uses native PlaySound and its measured native duration.
  struct Speech{DWORD id[3];float seconds;} speech={};using Make=void(__thiscall*)(void*,const wchar_t*);using Delete=void(__thiscall*)(void*);auto core=GetModuleHandleW(L"Core.dll");
  reinterpret_cast<Make>(GetProcAddress(core,"??0FString@@QAE@PBG@Z"))(speech.id,tutorialLines[3].id);callScript(objectField(player,L"theNarrator"),L"DeliverDialog",&speech);reinterpret_cast<Delete>(GetProcAddress(core,"??1FString@@QAE@XZ"))(speech.id);
  need(speech.seconds>tutorialLines[3].seconds[0],"Native dialogue wait duration missing");phase=6;at=now;return;
 }
 if(phase==6){
  if(now-at<18000)return;
  generation=levelGeneration;void* book=objectField(menuConsole,L"MenuBook"),*slot=objectField(book,L"SlotPage");int selected=0;callScript(slot,L"SetSelectedSlot",&selected);callNoArgs(slot,L"LoadSelectedSlot");phase=7;at=now;return;
 }
 if(phase==7&&levelGeneration>generation){need(!tutorialLine,"Tutorial survived level transition");tutorialTestLookup(tutorialLines[3]);phase=8;at=now;return;}
 if(phase==8){tutorialRemapped=false;tutorialTestLookup(tutorialLines[15]);phase=9;at=now;return;}
 if(phase==9){if(now-at<2100)return;need(tutorialModels.drawnMask==3&&tutorialModels.devices.size()==1&&tutorialModels.devices[0].index==2&&tutorialModels.devices[0].highlights[0]=="grip","Inventory left grip not rendered in both eyes");controls.showStatus=true;tutorialAlpha(true,true);need(tutorialFade!=0,"Inventory hint did not acknowledge grip");phase=10;at=now;return;}
 if(phase==10){need(!tutorialLine,"Inventory cue did not fade");log("{\"event\":\"hp1_tutorial_test_complete\",\"lookups\":%u,\"native_duration\":true,\"native_dialogue_playback\":true,\"inventory_grip\":true,\"remapped_hand_component\":true,\"both_eyes\":true,\"success_fade\":true,\"unrelated_unchanged\":true,\"skip_cleanup\":true,\"reload\":true,\"mock_controller_driver\":true,\"passed\":true}",checks);phase=99;}
}
