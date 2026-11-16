#pragma once
// Explicit PC diagnostic only: a copy of the real checkpoint, never play-*.
static bool trollCapture=false;
static void trollReplayTick(bool cinematic){
 if(!trollTestMode)return;
 static ULONGLONG started=0,finished=0,next=0;
 static Vec origin={},points[5]={};static int phase=-1;
 static bool jumped=false,complete=false,captured=false;static unsigned lastTrollCalls=0,lastWalkCalls=0;
 auto now=GetTickCount64();
 if(complete){controls={};return;}
 if(cinematic){controls={};controls.active=true;controls.skip=true;return;}
 controls.skip=false;if(skipActive)return;
 if(!started){
  if(!trollChase()||state(player)!=L"PlayerWalking"){
   if(now>=next){next=now+1000;void* b=objectField(player,L"BossTarget");log("{\"event\":\"hp1_troll_test_wait\",\"state\":\"%ls\",\"boss\":\"%ls\",\"reverse\":%s}",state(player).c_str(),b?name(b).c_str():L"None",flag(player,L"bReverseInput")?"true":"false");}return;
  }
  started=now;origin=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));
  log("{\"event\":\"hp1_troll_test_start\",\"position\":[%.3f,%.3f,%.3f],\"original_chase\":true}",origin.x,origin.y,origin.z);
 }
 Vec p=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));
 Vec v=mem<Vec>(player,field(player,L"Velocity",12,L"StructProperty"));
 if(finished){
  controls.active=true;controls.forward=.6f;controls.side=0;controls.yaw=0;
  if(now-finished>=300){
   bool ok=!trollChase()&&!flag(player,L"bReverseInput")&&!flag(player,L"bConstrainYaw")&&trollMoveCalls==lastTrollCalls&&movementCalls>lastWalkCalls;
   log("{\"event\":\"hp1_troll_test_complete\",\"native_jump\":%s,\"native_stop_encounter\":true,\"walking_restored\":%s,\"passed\":%s}",jumped?"true":"false",ok?"true":"false",ok?"true":"false");
   need(ok,"Troll adapter leaked into ordinary walking");complete=true;controls={};
  }return;
 }
 need(state(player)==L"PlayerWalking"&&trollChase(),"Troll fixture left original chase before motion validation");
 auto elapsed=now-started;
 int step=elapsed<700?0:elapsed<1300?1:elapsed<1900?2:elapsed<2900?3:elapsed<3200?4:5;
 if(step!=phase){
  if(step>0&&step<=5)points[step-1]=p;
  phase=step;log("{\"event\":\"hp1_troll_test_sample\",\"step\":%d,\"position\":[%.3f,%.3f,%.3f],\"velocity\":[%.3f,%.3f,%.3f]}",step,p.x,p.y,p.z,v.x,v.y,v.z);
 }
 // Turn the diagnostic head away: course motion must still go along +X.
 controls={};controls.active=step!=4;controls.yaw=32768;controls.forward=.9f;
 if(step==1)controls.side=-.5f;if(step==2)controls.side=.5f;
 controls.jump=elapsed>=1900&&elapsed<2100;
 if(step==3&&v.z>100&&mem<BYTE>(player,field(player,L"Physics",1,L"ByteProperty"))==2)jumped=true;
 if(elapsed>=1100&&!captured){trollCapture=true;captured=true;}
 if(step==5){
  float forward=p.x-origin.x,left=points[0].y-points[1].y,right=points[2].y-points[1].y;
  Vec accel=mem<Vec>(player,field(player,L"Acceleration",12,L"StructProperty"));
  bool flags=flag(player,L"bReverseInput")&&flag(player,L"bConstrainYaw");
  bool ok=forward>300&&left>15&&right>15&&jumped&&std::hypot(accel.x,accel.y)<.01f&&flags;
  log("{\"event\":\"hp1_troll_test_motion\",\"forward\":%.3f,\"left\":%.3f,\"right\":%.3f,\"jump\":%s,\"inactive_acceleration\":%.3f,\"native_flags_preserved\":%s,\"passed\":%s}",forward,left,right,jumped?"true":"false",std::hypot(accel.x,accel.y),flags?"true":"false",ok?"true":"false");
  need(ok,"Troll stick/jump/focus regression");
  lastTrollCalls=trollMoveCalls;lastWalkCalls=movementCalls;callNoArgs(player,L"StopBossEncounter");finished=now;
 }
}
