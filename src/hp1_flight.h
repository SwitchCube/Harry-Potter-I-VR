#pragma once
#include "hp1_flight_math.h"
static bool flightMoving=false;
static bool flightCapturePending=false;
static hp1::FlightMotion flightMotion;
static void* flightMotionPlayer=nullptr;
static unsigned flightMotionTick=0;
static float flightMaxYawRate=0,flightMaxYawAccel=0,flightMaxSpeedUp=0,flightMaxSpeedDown=0;
static bool broomPlayer(){return player&&name(mem<void*>(player,0x24))==L"BroomHarry";}
static bool practiceFlight(){if(!broomPlayer())return false;void* referee=objectField(player,L"Referee");return referee&&name(mem<void*>(referee,0x24))==L"BroomPracticeReferee";}
// All four installed HP1 flight referees share BroomHarry's native movement.
// Their scoring, paths, collisions and action handlers stay original.
static int flightKind(){
 if(!broomPlayer())return 0;void* ref=objectField(player,L"Referee");if(!ref)return 0;
 auto type=name(mem<void*>(ref,0x24));
 return type==L"BroomPracticeReferee"?1:type==L"RemembrallReferee"?2:type==L"QuidditchReferee"?3:type==L"FlyingKeyReferee"?4:0;
}
static bool flightInteractive(){int kind=flightKind();if(!kind)return false;auto s=state(objectField(player,L"Referee"));return s==L"GameTrial"||s==L"GamePlay"||s==L"GameBump"||s==L"GameCatch";}
static bool flightActionWindow(){if(!flightKind())return false;auto s=state(objectField(player,L"Referee"));return s==L"GameBump"||s==L"GameCatch";}
static hp1::FlightActionLatch flightAction;
static void* flightActionPlayer=nullptr;
static bool flightFixture(){static int enabled=-1;if(enabled<0)enabled=startup.mode==6&&GetFileAttributesW((std::wstring(startup.profile)+L"\\test-flight.flag").c_str())!=INVALID_FILE_ATTRIBUTES;return enabled!=0;}
static void flightButtons(){
 void* hud=objectField(player,L"myHUD");
 bool enabled=controls.active&&!menuVisible&&!letterBusy&&hud&&!flag(hud,L"bCutSceneMode")&&flightInteractive();
 if(flightActionPlayer!=player){flightAction={};flightActionPlayer=player;}
 for(auto key:{L"bBroomYawLeft",L"bBroomYawRight",L"bBroomPitchUp",L"bBroomPitchDown"})mem<BYTE>(player,field(player,key,1,L"ByteProperty"))=0;
 mem<BYTE>(player,field(player,L"bBroomBoost",1,L"ByteProperty"))=enabled&&controls.cast&&!controls.jump;
 mem<BYTE>(player,field(player,L"bBroomBrake",1,L"ByteProperty"))=!enabled||controls.jump;
 mem<BYTE>(player,field(player,L"bBroomAction",1,L"ByteProperty"))=flightAction.update(enabled&&flightActionWindow(),controls.skip);
 for(auto key:{L"aBroomYaw",L"aBroomPitch",L"fMouseYaw",L"fMousePitch"})mem<float>(player,field(player,key,4,L"FloatProperty"))=0;
 setFlag(player,L"bPitchUnderMouse",false);setFlag(player,L"bYawUnderMouse",false);
}
static bool flightInternal(void* self,void* frame,void* result){
 if(!vrMovement()||!frame||!flightKind())return false;
 auto fn=name(mem<void*>(frame,4));
 if(self==objectField(player,L"Referee")&&fn==L"OnActionKeyPressed"){
  Fpu fpu;auto before=state(self);fpu.restore();originalInternal(self,frame,result);
  log("{\"event\":\"hp1_flight_native_action\",\"kind\":%d,\"before\":\"%ls\",\"after\":\"%ls\",\"original_handler\":true}",flightKind(),before.c_str(),state(self).c_str());fpu.restore();return true;
 }
 if(self!=player)return false;
 // Desktop impact shake must not become an artificial headset rotation.
 if(fn==L"ViewShake")return true;
 if(fn==L"PlayerInput"){flightButtons();return false;}
 if(fn==L"PlayerMove"){
  if(state(player)!=L"PlayerWalking")return false;
  Fpu fpu;flightButtons();bool saved=flightMoving;flightMoving=true;
  void* locals=mem<void*>(frame,0x10);need(locals,"Flight movement locals missing");float dt=mem<float>(locals,localField(frame,L"DeltaTime",4));
  auto velocity=mem<Vec>(player,field(player,L"Velocity",12,L"StructProperty"));float actual=std::sqrt(hpvr::dot(velocity,velocity));
  if(flightMotionPlayer!=player||ticks-flightMotionTick>2){flightMotion.reset(actual);flightMotionPlayer=player;}
  flightMotionTick=ticks;float before=flightMotion.speed;
  fpu.restore();originalInternal(self,frame,result);flightMoving=saved;
  if(state(player)==L"PlayerWalking"){
   unsigned offset=field(player,L"AirSpeed",4,L"FloatProperty");float normal=float(mem<int>(player,field(player,L"AirSpeedNormal",4,L"IntProperty")));
   bool enabled=controls.active&&!menuVisible&&!letterBusy&&flightInteractive();
   float wanted=enabled?hp1::flightSpeed(mem<float>(player,offset),normal,controls.side,controls.forward,controls.cast||flag(player,L"bAuxBoost"),controls.jump,flightKind()!=1):0;
   float effective=flightMotion.speedStep(wanted,actual,dt);mem<float>(player,offset)=effective;
   // Stock HitWall uses Destination, so keep its look-ahead consistent with
   // the adapted speed while leaving collision and hoop detection native.
   Vec location=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));auto direction=hp1::gameAxes(mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"))).forward;
   mem<Vec>(player,field(player,L"Destination",12,L"StructProperty"))=hpvr::add(location,hpvr::scale(direction,effective*dt+.1f));
   if(dt>0&&actual+5>=before){flightMaxSpeedUp=std::max(flightMaxSpeedUp,(effective-before)/dt);flightMaxSpeedDown=std::max(flightMaxSpeedDown,(before-effective)/dt);}
   static ULONGLONG nextSample=0;auto now=GetTickCount64();
   if(startup.mode==3&&now>=nextSample){nextSample=now+1000;auto rotation=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"));void* ref=objectField(player,L"Referee");
    log("{\"event\":\"hp1_flight_live\",\"kind\":%d,\"referee_state\":\"%ls\",\"stick\":[%.3f,%.3f],\"pitch\":%d,\"yaw\":%d,\"yaw_rate\":%.2f,\"actual_speed\":%.2f,\"target_speed\":%.2f,\"dt_ms\":%.3f,\"precision_brake\":%s}",flightKind(),state(ref).c_str(),controls.side,controls.forward,hp1::signedAngle(rotation.pitch),rotation.yaw,flightMotion.yawRate,actual,effective,dt*1000,controls.jump?"true":"false");
   }
  }
  fpu.restore();return true;
 }
 if(fn==L"UpdateRotation"&&flightMoving){
  Fpu fpu;void* locals=mem<void*>(frame,0x10);need(locals,"Flight rotation locals missing");
  float dt=mem<float>(locals,localField(frame,L"DeltaTime",4));need(std::isfinite(dt)&&dt>=0&&dt<2,"Invalid flight delta time");
  bool enabled=controls.active&&!menuVisible&&!letterBusy&&flightInteractive();
  float up=float(mem<int>(player,field(player,L"PitchLimitUp",4,L"IntProperty"))),down=float(mem<int>(player,field(player,L"PitchLimitDown",4,L"IntProperty")));
  need(up>0&&up<=89&&down>0&&down<=89,"Unreviewed broom pitch limits");
  Rot body=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"));
  int desiredPitch=flightMotion.pitchStep(enabled?controls.forward:0,body.pitch,up,down,flag(player,L"bInvertBroomPitch"),dt);
  float pitchCommand=dt>0?float(desiredPitch-hp1::signedAngle(body.pitch))/(hp1::flightPitchRate*dt):0;
  // Keep original reversal statistics; our bounded analogue auto-level
  // replaces its keyboard-only centring. Commit the exact result below.
  mem<float>(player,field(player,L"fPitchControl",4,L"FloatProperty"))=std::fabs(pitchCommand)<.0005f?.00051f:pitchCommand;
  mem<float>(player,field(player,L"fYawControl",4,L"FloatProperty"))=hp1::flightYaw(enabled?controls.side:0);
  RestoreFields restore;auto rates=mem<Rot>(player,field(player,L"RotationRate",12,L"StructProperty"));rates.pitch=int(hp1::flightPitchRate);
  restore.set(player,field(player,L"RotationRate",12,L"StructProperty"),rates);
  restore.set(player,field(player,L"fRotationRateYaw",4,L"FloatProperty"),hp1::flightYawRate);
  fpu.restore();originalInternal(self,frame,result);
  auto rotated=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"));float before=flightMotion.yawRate;
  rotated.pitch=desiredPitch&65535;
  int delta=flightMotion.yawStep(dt>0?hp1::signedAngle(rotated.yaw-body.yaw)/dt:0,dt);
  rotated.yaw=body.yaw+delta;
  // Use the real reflected HP1 SetRotation API so actor/render state stays in
  // sync. This also smooths native wall avoidance rather than removing it.
  void* fn=scriptFunction(player,L"SetRotation");void* parameter=findProperty(fn,L"NewRotation"),*ret=findProperty(fn,L"ReturnValue");
  need(parameter&&ret&&mem<DWORD>(parameter,0x48)==0&&mem<DWORD>(parameter,0x38)==12&&mem<DWORD>(ret,0x48)==12&&mem<DWORD>(ret,0x38)==4,"HP1 SetRotation parameters differ");
  struct {Rot rotation;DWORD ok;} args={rotated,0};fpu.restore();callScript(player,L"SetRotation",&args);
  auto committed=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"));
  mem<Rot>(player,field(player,L"ViewRotation",12,L"StructProperty")).yaw=committed.yaw;
  mem<Rot>(player,field(player,L"DesiredRotation",12,L"StructProperty"))=committed;
  flightMaxYawRate=std::max(flightMaxYawRate,std::fabs(flightMotion.yawRate));if(dt>0)flightMaxYawAccel=std::max(flightMaxYawAccel,std::fabs(flightMotion.yawRate-before)/dt);
  fpu.restore();return true;
 }
 return false;
}
