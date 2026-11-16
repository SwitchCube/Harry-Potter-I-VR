#pragma once
// Isolated checkpoint fixture: native equipment, timers, sound and effects.
static unsigned fluteCapture=0;
static void fluteReplayTick(bool cinematic){
 if(!fluteTestMode)return;
 static unsigned phase=0,headIndex=0;static void* boss=nullptr,*heads[3]={};
 static ULONGLONG began=0;static float before=0;static bool captured=false,slept=false,menuChecked=false,trackingChecked=false;
 static unsigned poseSamples=0;static float maxPoseError=0;
 static unsigned order[3]={0,1,2};auto now=GetTickCount64();
 if(phase==9){controls={};return;}
 if(cinematic){controls={};controls.active=true;controls.skip=true;return;}
 if(skipActive)return;
 if(phase>=3&&controls.hand&&ownedFlute()){
  auto f=ownedFlute();auto d=hpvr::sub(mem<Vec>(f,field(f,L"Location",12,L"StructProperty")),controls.wandBase);
  float error=std::sqrt(hpvr::dot(d,d));maxPoseError=std::max(maxPoseError,error);++poseSamples;
  auto r=mem<Rot>(f,field(f,L"Rotation",12,L"StructProperty"));
  need(error<.01f&&short(r.pitch-controls.wandRotation.pitch)==0&&short(r.yaw-controls.wandRotation.yaw)==0&&short(r.roll-controls.wandRotation.roll)==0,"Native flute did not follow the controller pose");
 }
 controls={};controls.active=true;controls.hand=true;controls.yaw=32768;
 if(phase==0){
  need(name(mem<void*>(player,0x24))==L"HarryL502","Flute checkpoint is not Fluffy's Harry");
  need(!ownedFlute(),"Flute fixture expects the pre-introduction checkpoint");
  boss=objectField(player,L"Fluffy");need(boss,"Native Fluffy missing");
  need(mechanicsPosition({1850,2109,44.5f},32768),"Private Fluffy approach placement blocked");
  void* scene=mechanicsFind(L"CutScene2");need(scene,"Native Fluffy introduction absent");
  callNoArgs(scene,L"StartPlaying");phase=1;began=now;return;
 }
 if(phase==1){
  if(!ownedFlute()){need(now-began<30000,"Native introduction did not equip flute");return;}
  auto offset=field(boss,L"Heads",4,L"ObjectProperty");
  for(unsigned i=0;i<3;++i){heads[i]=mem<void*>(boss,offset+i*4);need(heads[i],"Fluffy head missing");}
  std::sort(order,order+3,[&](unsigned a,unsigned b){return mem<float>(heads[a],field(heads[a],L"ListenTime",4,L"FloatProperty"))<mem<float>(heads[b],field(heads[b],L"ListenTime",4,L"FloatProperty"));});
  phase=2;
 }
 if(phase==2){
  auto* head=heads[order[headIndex]];auto body=mem<Vec>(boss,field(boss,L"Location",12,L"StructProperty"));
  auto dir=hp1::gameAxes(mem<Rot>(boss,field(boss,L"Rotation",12,L"StructProperty"))).forward;
  auto h=mem<Vec>(head,field(head,L"Location",12,L"StructProperty"));auto plane=hpvr::sub(h,hpvr::scale(dir,hpvr::dot(hpvr::sub(h,body),dir)));
  float range=mem<float>(boss,field(boss,L"AttackDist",4,L"FloatProperty"))+mem<float>(player,field(player,L"CollisionRadius",4,L"FloatProperty"));
  auto p=hpvr::add(plane,hpvr::scale(dir,range*.8f));p.z=mem<Vec>(player,field(player,L"Location",12,L"StructProperty")).z;
  need(mechanicsPosition(p,32768),"Private flute head placement blocked");
  before=mem<float>(head,field(head,L"_Timer",4,L"FloatProperty"));began=now;phase=3;
  log("{\"event\":\"hp1_flute_test_head\",\"index\":%u,\"timer\":%.4f,\"range\":%.3f}",order[headIndex],before,range);
 }
 // Stand-in hand poses use the same original mesh/pose path as live input.
 Vec p=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));float eye=mem<float>(player,field(player,L"BaseEyeHeight",4,L"FloatProperty"));
 controls.hand=true;controls.wandRotation={-3000,32768,0};
 if(headIndex==1)controls.wandRotation={-5000,30000,2500};
 auto axes=hp1::gameAxes(controls.wandRotation);controls.wandForward=axes.forward;controls.wandUp=axes.up;
 controls.wandBase=hpvr::add(p,Vec{-18,headIndex==1?-20.0f:-12.0f,eye-12});
 if(phase==3){
  auto elapsed=now-began;float duration=headIndex==0?before+.4f:1.5f;
  controls.cast=elapsed>=350&&elapsed<350+ULONGLONG(duration*1000);
  if(elapsed>750&&elapsed<1000)need(flutePlaying(),"Holding trigger did not sustain original FlutePlay");
  if(!captured&&elapsed>=1100){fluteCapture=headIndex==0?1:2;captured=true;}
  if(elapsed>=350+ULONGLONG(duration*1000)){
   auto head=heads[order[headIndex]];float remaining=mem<float>(head,field(head,L"_Timer",4,L"FloatProperty"));bool sleeping=flag(head,L"bSleeping");
   if(headIndex==0)slept=sleeping;
   bool ok=before-remaining>1&&(headIndex!=0||sleeping);
   log("{\"event\":\"hp1_flute_test_effect\",\"index\":%u,\"before\":%.4f,\"after\":%.4f,\"sleeping\":%s,\"passed\":%s}",order[headIndex],before,remaining,sleeping?"true":"false",ok?"true":"false");
   need(ok,"Native Fluffy head did not react to held flute");began=now;phase=4;
  }return;
 }
 if(phase==4&&now-began>300){
  need(state(player)==L"PlayerWalking"&&!fluteHeld,"Release did not restore native walking");
  if(headIndex==1&&!menuChecked){
   // Menu/tracking checks do not need attack range. Leave the native bite AI
   // running; move only this private fixture away instead of disabling damage.
   auto safe=mem<Vec>(boss,field(boss,L"Location",12,L"StructProperty"));safe.x+=600;safe.z=p.z;
   need(mechanicsPosition(safe,32768),"Flute UI fixture placement blocked");controls.hand=true;
   phase=5;began=now;return;
  }
  if(headIndex==1&&!trackingChecked){controls.cast=true;phase=11;began=now;return;}
  if(++headIndex<3){phase=2;captured=headIndex==2;return;}
  bool ok=fluteMeshMask==3&&slept&&fluteStarts>=5&&menuChecked&&trackingChecked&&poseSamples>20;
  log("{\"event\":\"hp1_flute_test_complete\",\"heads\":3,\"native_sleep\":%s,\"both_eye_meshes\":%s,\"release_and_menu\":%s,\"tracking_loss\":%s,\"pose_samples\":%u,\"max_pose_error\":%.6f,\"passed\":%s}",slept?"true":"false",fluteMeshMask==3?"true":"false",menuChecked?"true":"false",trackingChecked?"true":"false",poseSamples,maxPoseError,ok?"true":"false");
  need(ok,"Flute complete regression");phase=9;controls={};return;
 }
 if(phase==5){controls.cast=true;if(now-began>500){need(flutePlaying(),"Second trigger press did not restart flute");inputKey(27);phase=6;began=now;}return;}
 if(phase==6){controls.cast=true;if(now-began>300){need(menuVisible&&!flutePlaying()&&!fluteHeld,"Menu did not stop native flute");inputKey(27);phase=7;began=now;}return;}
 if(phase==7){controls.cast=true;if(now-began>300){need(!menuVisible&&!flutePlaying(),"Held menu trigger restarted flute without release");menuChecked=true;phase=4;began=now;}return;}
 if(phase==11){controls.cast=true;if(now-began>500){need(flutePlaying(),"Flute did not start before tracking check");phase=12;began=now;}return;}
 if(phase==12){controls.cast=true;controls.hand=false;if(now-began>300){need(!flutePlaying()&&!fluteHeld,"Lost hand tracking did not stop the flute");phase=13;began=now;}return;}
 if(phase==13){controls.cast=true;if(now-began>300){need(!flutePlaying(),"Tracking regain restarted a held trigger");trackingChecked=true;phase=4;began=now;}return;}
}
