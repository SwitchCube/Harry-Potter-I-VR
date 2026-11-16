#pragma once
#include "hp1_levitation_math.h"
static bool levitating(){return player&&!_wcsicmp(state(player).c_str(),L"wingspell");}
static bool levitationInternal(void* self,void* frame,void* result){
 if(!vrMovement()||!levitating()||!frame||_wcsicmp(name(mem<void*>(frame,4)).c_str(),L"Tick")||self!=objectField(player,L"focusActor"))return false;
 if(!findProperty(mem<void*>(self,0x24),L"bIsLevitating")||!flag(self,L"bIsLevitating"))return false;
 // Only the reviewed baseProps.Tick has these locals. An overriding tick
 // keeps its own behavior; its eventual Super.Tick reaches this adapter.
 void* fn=mem<void*>(frame,4);void* deltaProperty=findProperty(fn,L"delta"),*directionProperty=findProperty(fn,L"dir");
 if(!deltaProperty||!directionProperty||name(mem<void*>(deltaProperty,0x24))!=L"FloatProperty"||mem<DWORD>(deltaProperty,0x38)!=4||mem<DWORD>(directionProperty,0x38)!=12)return false;
 Fpu fpu;RestoreFields restore;
 for(auto key:{L"SmoothMouseX",L"SmoothMouseY",L"aForMove",L"aSideMove"})restore.set(player,field(player,key,4,L"FloatProperty"),0.0f);
 void* camera=objectField(self,L"cam");
 Vec position=mem<Vec>(self,field(self,L"Location",12,L"StructProperty"));
 float remaining=mem<float>(self,field(self,L"curlevTime",4,L"FloatProperty"));
 bool ready=camera&&!flag(self,L"bStopLevitating")&&remaining>0&&position.z>=mem<float>(self,field(self,L"levDestZ",4,L"FloatProperty"));
 if(ready&&mem<float>(self,field(self,L"bouncetime",4,L"FloatProperty"))<=0){
  void* locals=mem<void*>(frame,0x10);float dt=mem<float>(locals,localField(frame,L"delta",4));
  bool input=controls.active&&!menuVisible&&std::isfinite(dt)&&dt>0&&dt<=.1f;
  auto target=hp1::levitationTarget(position,mem<Vec>(camera,field(camera,L"Location",12,L"StructProperty")),controls.yaw,controls.forward,controls.side,input);
  mem<float>(self,field(self,L"dist",4,L"FloatProperty"))=target.distance;
  mem<Rot>(self,field(self,L"rotv",12,L"StructProperty"))=target.rotation;
 }
 fpu.restore();originalInternal(self,frame,result);fpu.restore();
 static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+1000;
  Vec p=mem<Vec>(self,field(self,L"Location",12,L"StructProperty"));
  log("{\"event\":\"hp1_levitation_control\",\"object\":\"%ls\",\"ready\":%s,\"position\":[%.4f,%.4f,%.4f],\"input\":[%.3f,%.3f],\"remaining\":%.3f,\"native_collision\":true}",name(self).c_str(),ready?"true":"false",p.x,p.y,p.z,controls.forward,controls.side,remaining);
  if(mechanicsTestMode&&camera){Vec c=mem<Vec>(camera,field(camera,L"Location",12,L"StructProperty"));Rot r=mem<Rot>(self,field(self,L"rotv",12,L"StructProperty"));Vec dir=mem<Vec>(mem<void*>(frame,0x10),localField(frame,L"dir",12));log("{\"event\":\"hp1_levitation_native_target\",\"camera\":[%.3f,%.3f,%.3f],\"yaw\":%d,\"distance\":%.3f,\"target\":[%.3f,%.3f,%.3f]}",c.x,c.y,c.z,r.yaw,mem<float>(self,field(self,L"dist",4,L"FloatProperty")),dir.x,dir.y,dir.z);}
 }
 return true;
}
