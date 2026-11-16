#pragma once
// Explicit isolated mode-6 fixture. No synthetic input in native-vr or play-*.
static void* mechanicsFind(const wchar_t* wanted){
 void* level=mem<void*>(player,0x80);void** actors=mem<void**>(level,0x2c);int n=mem<int>(level,0x30);need(actors&&n>0&&n<100000,"Mechanics actor array");
 for(int i=0;i<n;++i)if(actors[i]&&name(actors[i])==wanted)return actors[i];return nullptr;
}
static bool mechanicsPosition(Vec p,int yaw){
 using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);auto move=reinterpret_cast<FarMove>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?FarMoveActor@ULevel@@UAEHPAVAActor@@VFVector@@HH@Z"));need(move!=nullptr,"Mechanics placement API");
 if(!move(mem<void*>(player,0x80),player,p,0,0))return false;
 mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"))={0,yaw,0};mem<Rot>(player,field(player,L"ViewRotation",12,L"StructProperty"))={0,yaw,0};
 mem<Vec>(player,field(player,L"Velocity",12,L"StructProperty"))={};mem<Vec>(player,field(player,L"Acceleration",12,L"StructProperty"))={};controls={};controls.active=true;controls.yaw=yaw;
 mechanicsRecenter=true;return true;
}
static void mechanicsReplayTick(bool cinematic){
 if(!mechanicsTestMode)return;
 static unsigned phase=0;static ULONGLONG began=0;static void* block=nullptr;
 static Vec initial={},last={},positionSamples[7]={};static int sampled=-1;static unsigned side=0;static bool cast=false;
 if(cinematic){controls.skip=true;return;}controls.skip=false;
 if(skipActive)return;
 auto now=GetTickCount64();
 if(mechanicsTestMode==1){
  if(mirrorSavedMask==3){if(phase!=9){log("{\"event\":\"hp1_mirror_test_complete\",\"both_eyes\":true,\"passed\":true}");phase=9;}return;}
  if(phase==9||state(player)!=L"PlayerWalking")return;
  if(!began||now-began>1800){
   void* mirror=mechanicsFind(L"Mover20");need(mirror,"Mirror1 mover missing");auto p=mem<Vec>(mirror,field(mirror,L"Location",12,L"StructProperty"));
   const Vec offsets[]={{0,220,-32},{220,0,-32},{0,-220,-32},{-220,0,-32}};const int yaw[]={-16384,32768,16384,0};
   need(side<4,"Mirror fixture never rendered the reflected player");auto i=side++;
   bool placed=mechanicsPosition(hpvr::add(p,offsets[i]),yaw[i]);began=now;
   log("{\"event\":\"hp1_mirror_fixture\",\"side\":%u,\"placed\":%s,\"yaw\":%d}",i,placed?"true":"false",yaw[i]);
   if(!placed)began=now-1801;
  }return;
 }
 if(phase==0){
  if(state(player)!=L"PlayerWalking")return;
  block=mechanicsFind(L"WingardiumBlock3");need(block,"Original Wingardium block missing");
  initial=mem<Vec>(block,field(block,L"Location",12,L"StructProperty"));
  need(mechanicsPosition(hpvr::add(initial,Vec{0,170,16}),-16384),"Wingardium fixture placement blocked");
  // Park only the private test player during the fixture cast, matching the
  // stock stationary wingspell state. Object collision/physics remain native.
  BYTE physics=5;callScript(player,L"SetPhysics",&physics);
  void* camera=objectField(player,L"cam");need(camera,"Fixture original camera");
  using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);auto move=reinterpret_cast<FarMove>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?FarMoveActor@ULevel@@UAEHPAVAActor@@VFVector@@HH@Z"));
  need(move(mem<void*>(player,0x80),camera,hpvr::add(initial,Vec{0,220,100}),0,1)!=0,"Fixture camera placement");
  mechanicsCapture=1;began=now;phase=1;return;
 }
 if(phase==1){
  Vec base=hpvr::add(mem<Vec>(player,field(player,L"Location",12,L"StructProperty")),Vec{0,0,28});Vec delta=hpvr::sub(mem<Vec>(block,field(block,L"Location",12,L"StructProperty")),base);
  float length=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);need(length>1,"Wingardium fixture ray");
  controls.hand=true;controls.wandBase=base;controls.wandForward=hpvr::scale(delta,1/length);controls.wandUp={0,0,1};
  controls.wandRotation={int(std::atan2(delta.z,std::hypot(delta.x,delta.y))*32768/hpvr::pi),int(std::atan2(delta.y,delta.x)*32768/hpvr::pi),0};
  if(!cast&&now-began>500){void* weapon=objectField(player,L"Weapon");callScript(weapon,L"CastSpell",&block);cast=true;log("{\"event\":\"hp1_levitation_test_cast\",\"original_spell\":true}");}
  if(levitating()&&flag(block,L"bIsLevitating")&&mem<Vec>(block,field(block,L"Location",12,L"StructProperty")).z>=mem<float>(block,field(block,L"levDestZ",4,L"FloatProperty"))){phase=2;began=now;last=mem<Vec>(block,field(block,L"Location",12,L"StructProperty"));positionSamples[0]=last;mechanicsCapture=2;}
  need(now-began<12000,"Original Wingardium spell did not engage");return;
 }
 if(phase==2){
  unsigned step=unsigned((now-began)/1000);controls.forward=controls.side=0;
  if(step==0)controls.side=.6f;else if(step==1)controls.forward=.6f;else if(step==2)controls.side=-.6f;else if(step==3)controls.forward=-.6f;
  if(int(step)>sampled){sampled=step;Vec p=mem<Vec>(block,field(block,L"Location",12,L"StructProperty"));if(step<=6)positionSamples[step]=p;
   log("{\"event\":\"hp1_levitation_test_sample\",\"step\":%u,\"position\":[%.4f,%.4f,%.4f],\"state\":\"%ls\"}",step,p.x,p.y,p.z,state(player).c_str());
  }
  if(step>=6){
   float right=positionSamples[1].x-positionSamples[0].x,forward=positionSamples[1].y-positionSamples[2].y,left=positionSamples[2].x-positionSamples[3].x,back=positionSamples[4].y-positionSamples[3].y;
   auto d=hpvr::sub(positionSamples[6],positionSamples[5]);float drift=std::hypot(d.x,d.y);
   bool ok=right>15&&forward>15&&left>15&&back>15&&drift<2;
   log("{\"event\":\"hp1_levitation_test_motion\",\"right\":%.4f,\"forward\":%.4f,\"left\":%.4f,\"back\":%.4f,\"neutral_drift\":%.4f,\"passed\":%s}",right,forward,left,back,drift,ok?"true":"false");need(ok,"Wingardium stick motion/neutral regression");
   controls.cast=true;phase=3;began=now;
  }return;
 }
 if(phase==3){controls.cast=false;
  if(!flag(block,L"bIsLevitating")&&state(player)==L"PlayerWalking"){
   log("{\"event\":\"hp1_levitation_test_complete\",\"original_drop\":true,\"passed\":true}");phase=4;controls={};
  }else need(now-began<10000,"Wingardium trigger did not restore normal play");
 }
}
