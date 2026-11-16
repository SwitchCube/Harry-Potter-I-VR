#pragma once
#include "hp1_troll_math.h"
static bool trollMoving=false;
static Vec trollWanted={};
static unsigned trollMoveCalls=0;
static bool trollChase(){
 if(!player||!flag(player,L"bReverseInput")||!flag(player,L"bConstrainYaw"))return false;
 void* boss=objectField(player,L"BossTarget");
 return boss&&name(mem<void*>(boss,0x24))==L"BossTrollChase";
}
static bool trollInternal(void* self,void* frame,void* result){
 if(!vrMovement()||!player||self!=player||!frame)return false;
 auto fn=name(mem<void*>(frame,4));
 if(fn==L"ProcessMove"&&trollMoving){
  // Adapt the original walking function's input, BEFORE it handles jumping,
  // animation and physics. No teleport, velocity replacement or AI changes.
  Fpu fpu;void* locals=mem<void*>(frame,0x10);need(locals,"Troll ProcessMove locals missing");
  mem<Vec>(locals,localField(frame,L"NewAccel",12))=trollWanted;
  fpu.restore();originalInternal(self,frame,result);fpu.restore();return true;
 }
 if(fn!=L"PlayerMove"||!trollChase()||state(player)!=L"PlayerWalking"||flag(player,L"bKeepStationary"))return false;
 BYTE physics=mem<BYTE>(player,field(player,L"Physics",1,L"ByteProperty"));
 if(physics!=1&&physics!=2)return false;
 Fpu fpu;void* hud=objectField(player,L"myHUD");
 if(!hud||flag(hud,L"bCutSceneMode")){fpu.restore();return false;}
 float rate=mem<float>(player,field(player,L"AccelRate",4,L"FloatProperty"));
 need(std::isfinite(rate)&&rate>0&&rate<100000,"Invalid HP1 troll acceleration");
 auto wanted=hp1::trollAcceleration(controls.forward,controls.side,rate,controls.active&&!menuVisible&&!letterBusy);
 // The stock chase doubles/absolutizes forward and negates strafe for its
 // rear-facing desktop camera. Compensate, retaining its native yaw bounds.
 mem<float>(player,field(player,L"aForward",4,L"FloatProperty"))=wanted.x/.16f;
 mem<float>(player,field(player,L"aStrafe",4,L"FloatProperty"))=-wanted.y/.16f;
 for(auto key:{L"aSideMove",L"aTurn",L"aLookUp"})mem<float>(player,field(player,key,4,L"FloatProperty"))=0;
 bool screenRelative=flag(player,L"bScreenRelativeMovement");setFlag(player,L"bScreenRelativeMovement",false);
 bool before=trollMoving;Vec oldWanted=trollWanted;trollMoving=true;trollWanted=wanted;
 fpu.restore();originalInternal(self,frame,result);
 trollMoving=before;trollWanted=oldWanted;setFlag(player,L"bScreenRelativeMovement",screenRelative);
 ++trollMoveCalls;
 static ULONGLONG next=0;auto now=GetTickCount64();
 if(now>=next){next=now+1000;auto p=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));
  log("{\"event\":\"hp1_troll_control\",\"stick\":[%.3f,%.3f],\"acceleration\":[%.3f,%.3f,%.3f],\"position\":[%.3f,%.3f,%.3f],\"native_physics\":true}",controls.forward,controls.side,wanted.x,wanted.y,wanted.z,p.x,p.y,p.z);
 }
 fpu.restore();return true;
}
