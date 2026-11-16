#pragma once
#include "hp1_flight_world_test.h"
// Private input-only integration test: no teleport, no altered ring counts,
// rewards, collision, time limit or referee state.
static void flightReplayTick(bool cinematic){
 if(flightFixture()&&flightKind()>1){flightWorldReplay(cinematic);return;}
 if(!flightFixture()||!practiceFlight()||cinematic)return;
 void* ref=objectField(player,L"Referee");if(state(player)!=L"PlayerWalking"||state(ref)!=L"GameTrial")return;
 static ULONGLONG start=0,next=0;static int phase=-1,baseYaw=0;static float right=0,left=0,up=0,down=0,boost=0,brake=100000;static bool done=false;
 if(done){controls={};static unsigned menuPhase=0;static ULONGLONG finished=GetTickCount64();auto elapsed=GetTickCount64()-finished;
  if(elapsed>=1000&&menuPhase==0){inputKey(27);menuPhase=1;}
  else if(elapsed>=2000&&menuPhase==1){need(menuVisible,"Flight menu did not open");inputKey(27);menuPhase=2;log("{\"event\":\"hp1_flight_menu_test\",\"passed\":true}");}
  return;
 }
 auto now=GetTickCount64();if(!start){start=now;replayMoveStarted=now;log("{\"event\":\"hp1_flight_test_started\",\"original_referee\":true}");}
 float t=(now-start)/1000.0f;int current=t<7?int(t):7;
 Rot rot=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"));float speed=mem<float>(player,field(player,L"AirSpeed",4,L"FloatProperty"));
 if(current!=phase){if(phase==1)right=hp1::signedAngle(rot.yaw-baseYaw);if(phase==2)left=hp1::signedAngle(rot.yaw-baseYaw);phase=current;baseYaw=rot.yaw;}
 if(phase==3)up=std::max(up,float(hp1::signedAngle(rot.pitch)));if(phase==4)down=std::min(down,float(hp1::signedAngle(rot.pitch)));
 if(phase==5)boost=std::max(boost,speed);if(phase==6)brake=std::min(brake,speed);
 controls={};controls.active=true;
 if(phase==1)controls.side=.35f;if(phase==2)controls.side=-.35f;
 if(phase==3)controls.forward=.35f;if(phase==4)controls.forward=-.35f;
 if(phase==5)controls.cast=true;if(phase==6)controls.jump=true;
 int rings=mem<int>(ref,field(ref,L"HoopsHit",4,L"IntProperty"));
 if(phase==7){
  int path=mem<int>(ref,field(ref,L"CurrentPath",4,L"IntProperty")),stage=mem<int>(ref,field(ref,L"CurrentStage",4,L"IntProperty")),ring=mem<int>(ref,field(ref,L"NextHoopToHit",4,L"IntProperty"));
  need(path>=1&&path<=2&&stage>=1&&stage<=5&&ring>=1&&ring<=25,"Flight fixture ring index");
  void* target=mem<void*>(ref,field(ref,L"Hoops",4,L"ObjectProperty")+4*((path-1)*125+(stage-1)*25+ring-1));
  if(target){auto delta=hpvr::sub(mem<Vec>(target,field(target,L"Location",12,L"StructProperty")),mem<Vec>(player,field(player,L"Location",12,L"StructProperty")));
   float yaw=std::atan2(delta.y,delta.x)*32768/hpvr::pi,diff=float(hp1::signedAngle(int(yaw)-rot.yaw));
   float command=std::clamp(diff/5000.0f,-1.0f,1.0f),lo=0,hi=1;for(int i=0;i<16;++i){float middle=(lo+hi)*.5f;if(hp1::flightYaw(middle)<std::fabs(command))lo=middle;else hi=middle;}controls.side=std::copysign((lo+hi)*.5f,command);
   float pitch=std::atan2(delta.z,std::hypot(delta.x,delta.y))*180/hpvr::pi,limit=float(mem<int>(player,field(player,pitch>=0?L"PitchLimitUp":L"PitchLimitDown",4,L"IntProperty")));
   controls.forward=std::clamp(pitch/limit,-1.0f,1.0f)*(flag(player,L"bInvertBroomPitch")?-1:1);
   float pitchError=pitch*32768/180-hp1::signedAngle(rot.pitch);
   controls.jump=std::fabs(diff)>4000||std::fabs(pitchError)>2500;
   controls.cast=!controls.jump&&std::fabs(diff)<1800&&std::fabs(pitchError)<1000&&std::sqrt(hpvr::dot(delta,delta))>350;
  }
 }
 if(now>=next){next=now+1000;auto p=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));log("{\"event\":\"hp1_flight_sample\",\"seconds\":%.2f,\"phase\":%d,\"yaw\":%d,\"pitch\":%d,\"speed\":%.2f,\"rings\":%d,\"position\":[%.2f,%.2f,%.2f]}",t,phase,rot.yaw,hp1::signedAngle(rot.pitch),speed,rings,p.x,p.y,p.z);}
 if(t>=12&&rings>=12){
  bool passed=right>100&&left< -100&&up>100&&down< -100&&boost>brake&&flag(player,L"bHasEverBoosted")&&flag(player,L"bHasEverBraked");done=true;flightCapturePending=true;
  bool comfort=flightMaxYawRate<=hp1::flightYawRate+.1f&&flightMaxYawAccel<=hp1::flightTurnBraking+2&&flightMaxSpeedUp<=221&&flightMaxSpeedDown<=401;
  passed=passed&&comfort;
  log("{\"event\":\"hp1_flight_comfort_test\",\"passed\":%s,\"max_yaw_rate\":%.2f,\"max_yaw_acceleration\":%.2f,\"max_speed_rise\":%.2f,\"max_speed_fall\":%.2f}",comfort?"true":"false",flightMaxYawRate,flightMaxYawAccel,flightMaxSpeedUp,flightMaxSpeedDown);
  log("{\"event\":\"hp1_flight_test_complete\",\"passed\":%s,\"right_delta\":%.0f,\"left_delta\":%.0f,\"up_pitch\":%.0f,\"down_pitch\":%.0f,\"boost_speed\":%.0f,\"brake_speed\":%.0f,\"original_rings\":%d}",passed?"true":"false",right,left,up,down,boost,brake,rings);
 }
}
