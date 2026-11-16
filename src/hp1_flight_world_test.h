#pragma once
// Private input replay for the actual chase/referee, not a replacement game.
// No writes to targets, progress, hit counts, collision or rewards. A separate
// explicit action-fixture flag enters the original final action state after
// the steering checks; it does not claim to have completed the entire chase.
static void saveFlightUiFixture(const wchar_t*);
static bool flightActionFixture(){return flightFixture()&&GetFileAttributesW((std::wstring(startup.profile)+L"\\test-flight-action.flag").c_str())!=INVALID_FILE_ATTRIBUTES;}
static void flightFixtureState(void* actor,const wchar_t* stateName){
 auto core=GetModuleHandleW(L"Core.dll");using Name=void(__thiscall*)(void*,const wchar_t*,int);using Go=int(__thiscall*)(void*,DWORD);
 auto make=reinterpret_cast<Name>(GetProcAddress(core,"??0FName@@QAE@PBGW4EFindName@@@Z"));
 auto go=reinterpret_cast<Go>(GetProcAddress(core,"?GotoState@UObject@@UAE?AW4EGotoState@@VFName@@@Z"));need(make&&go,"HP1 native state fixture API");
 DWORD token=0;make(&token,stateName,0);need(token,"Native flight state name");Fpu fpu;fpu.restore();go(actor,token);fpu.restore();need(state(actor)==stateName,"Native flight state entry");
}
static void flightWorldReplay(bool cinematic){
 if(!flightFixture()||flightKind()<2)return;
 static ULONGLONG start=0,next=0;static int phase=-1,baseYaw=0;static float right=0,left=0,up=0,down=0,boost=0,brake=100000;
 static bool actionSent=false,actionPassed=false,done=false;static int actionCalls=0,phaseAtAction=0;
 static ULONGLONG actionTime=0,finish=0;static unsigned menuPhase=0;
 void* ref=objectField(player,L"Referee");auto refState=state(ref);auto now=GetTickCount64();
 // Reopen the real book repeatedly during Quidditch in explicit diagnostics.
 // No score/save/state writes; allocation failure is injected in menuInternal.
 if(start&&now-start>=21000&&GetFileAttributesW((std::wstring(startup.profile)+L"\\test-menu-cycles.flag").c_str())!=INVALID_FILE_ATTRIBUTES){
  static unsigned step=0;static ULONGLONG nextMenu=0;controls={};controls.active=true;
  if(step<=24&&now>=nextMenu){
   if(step==24){need(!menuVisible,"Menu stress did not return to flight");log("{\"event\":\"hp1_menu_cycles_complete\",\"cycles\":12,\"passed\":true}");}
   else{need(menuVisible==bool(step%2),"Menu stress visibility differs");inputKey(27);}
   ++step;nextMenu=now+1000;
  }
  return;
 }
 if(done){controls={};if(now-finish>=1000&&menuPhase==0){inputKey(27);menuPhase=1;}else if(now-finish>=2000&&menuPhase==1){need(menuVisible,"World flight menu did not open");inputKey(27);menuPhase=2;log("{\"event\":\"hp1_flight_menu_test\",\"passed\":true}");}return;}
 if(actionSent&&refState==L"GameWon"&&!actionPassed){actionPassed=true;done=true;finish=now;log("{\"event\":\"hp1_flight_action_test_complete\",\"kind\":%d,\"passed\":true,\"original_win\":true,\"actions\":%d}",flightKind(),actionCalls);return;}
 if(cinematic||!flightInteractive()){controls={};controls.active=true;controls.skip=true;return;}
 if(!start){start=now;replayMoveStarted=now;log("{\"event\":\"hp1_flight_test_started\",\"kind\":%d,\"original_referee\":true}",flightKind());}
 float t=(now-start)/1000.0f;int current=t<7?int(t):7;
 Rot rot=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"));float speed=mem<float>(player,field(player,L"AirSpeed",4,L"FloatProperty"));
 if(current!=phase){if(phase==1)right=hp1::signedAngle(rot.yaw-baseYaw);if(phase==2)left=hp1::signedAngle(rot.yaw-baseYaw);phase=current;baseYaw=rot.yaw;}
 if(phase==3)up=std::max(up,float(hp1::signedAngle(rot.pitch)));if(phase==4)down=std::min(down,float(hp1::signedAngle(rot.pitch)));
 if(phase==5)boost=std::max(boost,speed);if(phase==6)brake=std::min(brake,speed);
 controls={};controls.active=true;
 if(phase==1)controls.side=.35f;if(phase==2)controls.side=-.35f;if(phase==3)controls.forward=.35f;if(phase==4)controls.forward=-.35f;
 if(phase==5)controls.cast=true;if(phase==6)controls.jump=true;
 if(phase==7&&state(player)==L"PlayerWalking"){
  void* target=objectField(player,L"LookForTarget");if(target&&!flag(target,L"bDeleteMe")){
   auto delta=hpvr::sub(mem<Vec>(target,field(target,L"Location",12,L"StructProperty")),mem<Vec>(player,field(player,L"Location",12,L"StructProperty")));
   float desired=std::atan2(delta.y,delta.x)*32768/hpvr::pi,error=float(hp1::signedAngle(int(desired)-rot.yaw));
   float command=std::clamp(error/4000.0f,-1.0f,1.0f),lo=0,hi=1;for(int i=0;i<16;++i){float m=(lo+hi)*.5f;if(hp1::flightYaw(m)<std::fabs(command))lo=m;else hi=m;}controls.side=std::copysign((lo+hi)*.5f,command);
   float pitch=std::atan2(delta.z,std::hypot(delta.x,delta.y))*180/hpvr::pi,limit=float(mem<int>(player,field(player,pitch>=0?L"PitchLimitUp":L"PitchLimitDown",4,L"IntProperty")));
   controls.forward=std::clamp(pitch/limit,-1.0f,1.0f)*(flag(player,L"bInvertBroomPitch")?-1:1);
   float pitchError=pitch*32768/180-hp1::signedAngle(rot.pitch);
   controls.jump=std::fabs(error)>6000||std::fabs(pitchError)>3500;controls.cast=!controls.jump;
  }
 }
 static ULONGLONG setupAt=0;static bool heldChecked=false;
 if(phaseAtAction==3&&t>=23&&flightActionFixture()&&!setupAt){
  setupAt=now;controls.skip=true;flightFixtureState(ref,flightKind()==2?L"GameBump":L"GameCatch");
  log("{\"event\":\"hp1_flight_action_fixture\",\"private_original_state_boundary\":true}");return;
 }
 if(setupAt&&now-setupAt<600){controls.skip=true;need(!flag(player,L"bActioned"),"Held scene button consumed flight action");return;}
 if(setupAt&&!heldChecked){heldChecked=true;controls.skip=false;log("{\"event\":\"hp1_flight_action_arrival\",\"held_button_preserved\":true}");return;}
 if(flightActionWindow()){
  void* hud=objectField(player,L"myHUD");void* game=objectField(hud,L"QHUDGame");bool act=refState==L"GameBump";
  if(game){int pos=mem<int>(game,field(game,L"iTargetPos",4,L"IntProperty")),limit=mem<int>(game,field(game,L"iCatchPos",4,L"IntProperty"));act=hp1::flightCatchReady(pos,limit);}
  if(act&&(!actionTime||now-actionTime>400)){controls.skip=true;actionSent=true;actionTime=now;++actionCalls;saveFlightUiFixture(L"flight-action.bmp");}
 }
 if(now>=next){next=now+1000;log("{\"event\":\"hp1_flight_world_sample\",\"seconds\":%.2f,\"kind\":%d,\"state\":\"%ls\",\"player_state\":\"%ls\",\"pitch\":%d,\"yaw\":%d,\"speed\":%.2f,\"actions\":%d}",t,flightKind(),refState.c_str(),state(player).c_str(),hp1::signedAngle(rot.pitch),rot.yaw,speed,actionCalls);}
 // Store steering evidence independently; action success is checked separately.
 if(t>=12&&phaseAtAction==0){phaseAtAction=1;flightCapturePending=true;saveFlightUiFixture(L"flight-progress.bmp");
  bool passed=right>100&&left< -100&&up>100&&down< -100&&boost>brake;
  log("{\"event\":\"hp1_flight_test_complete\",\"passed\":%s,\"kind\":%d,\"right_delta\":%.0f,\"left_delta\":%.0f,\"up_pitch\":%.0f,\"down_pitch\":%.0f,\"boost_speed\":%.0f,\"brake_speed\":%.0f}",passed?"true":"false",flightKind(),right,left,up,down,boost,brake);
 }
 if(t>=18&&phaseAtAction==1){phaseAtAction=2;inputKey(27);}
 if(t>=19&&phaseAtAction==2){need(menuVisible,"Flight replay menu");inputKey(27);phaseAtAction=3;log("{\"event\":\"hp1_flight_menu_test\",\"passed\":true}");}
}
