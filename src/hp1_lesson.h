#pragma once
#include "hp1_lesson_math.h"
static void* lessonActor=nullptr;
static bool lessonDrawing=false,lessonRayValid=false,lessonDrawCaptured=false;
static hp1::LessonPlane lessonPlane={};
static hp1::LessonHit lessonCursor={};
static unsigned lessonSamples=0;
static bool inLesson(){return player&&state(player)==L"SpellLearning";}
static int lessonFixture(){static int value=-1;if(value<0){value=0;if(startup.mode==6){FILE* f=_wfopen((std::wstring(startup.profile)+L"\\test-lesson.flag").c_str(),L"r");if(f){std::fscanf(f,"%d",&value);std::fclose(f);}}}return value;}
static bool directLessonStart(){static int value=-1;if(value<0)value=startup.mode==3&&GetFileAttributesW((std::wstring(startup.profile)+L"\\start-lesson.flag").c_str())!=INVALID_FILE_ATTRIBUTES;return value!=0;}
static void resetLesson(){lessonActor=nullptr;lessonDrawing=lessonRayValid=lessonDrawCaptured=false;lessonCursor={};}
static bool lessonGeometry(){
 if(!lessonActor||!inLesson())return false;void* cam=objectField(lessonActor,L"Cam");if(!cam)return false;
 float distance=mem<float>(lessonActor,field(lessonActor,L"TemplateDist",4,L"FloatProperty")),ratio=mem<float>(lessonActor,field(lessonActor,L"FOVRatio",4,L"FloatProperty"));
 need(std::isfinite(distance)&&distance>1&&distance<10000&&ratio>.01f&&ratio<10,"Invalid HP1 lesson plane");
 auto axes=hp1::gameAxes(mem<Rot>(lessonActor,field(lessonActor,L"Rotation",12,L"StructProperty")));
 lessonPlane={hpvr::add(mem<Vec>(cam,field(cam,L"Location",12,L"StructProperty")),hpvr::scale(axes.forward,distance)),axes,distance*ratio};return true;
}
static void lessonFixtureInput(){
 if(!lessonFixture()||!lessonGeometry())return;
 static ULONGLONG start=0;static int round=-1;
 int nowRound=mem<int>(lessonActor,field(lessonActor,L"LessonLevel",4,L"IntProperty"));
 if(round!=nowRound){round=nowRound;start=GetTickCount64();}
 float elapsed=float(GetTickCount64()-start)/1000,progress=std::max(0.0f,std::min(1.0f,(elapsed-.8f)/5));
 void* gesture=objectField(lessonActor,L"SpellGesture");unsigned offset=field(gesture,L"Points",12,L"ArrayProperty");Vec* points=mem<Vec*>(gesture,offset);int count=mem<int>(gesture,offset+4);need(points&&count>=2&&count<10000,"Original lesson gesture points");
 float index=progress*(count-1);int first=std::min(count-2,int(index));Vec uv=hpvr::add(points[first],hpvr::scale(hpvr::sub(points[first+1],points[first]),index-first));
 if(lessonFixture()==2)uv={.02f,.02f+progress*.8f,0};
 auto point=hp1::lessonPoint(lessonPlane,uv.x,uv.y);
 controls.hand=true;controls.cast=elapsed>=.8f&&elapsed<5.85f;
 controls.wandBase=hpvr::add(hpvr::sub(lessonPlane.centre,hpvr::scale(lessonPlane.axes.forward,100)),hpvr::scale(lessonPlane.axes.right,12));
 controls.wandForward=hp1::unit(hpvr::sub(point,controls.wandBase),lessonPlane.axes.forward);controls.wandUp=lessonPlane.axes.up;
 lessonCursor=hp1::lessonRay(lessonPlane,controls.wandBase,controls.wandForward);
}
static bool lessonInternal(void* self,void* frame,void* result){
 if((startup.mode!=3&&startup.mode!=6)||!frame||loadingDepth)return false;
 if(name(mem<void*>(self,0x24))!=L"SpellLearnTrigger")return false;
 auto fn=name(mem<void*>(frame,4));
 if(fn==L"Trigger"){
  originalInternal(self,frame,result);
  if(objectField(self,L"Player")==player){lessonActor=self;lessonSamples=0;log("{\"event\":\"hp1_lesson_begin\",\"actor\":\"%ls\"}",name(self).c_str());}return true;
 }
 if(fn==L"Destroyed"){tutorialLessonClosed(self);originalInternal(self,frame,result);if(self==lessonActor)resetLesson();return true;}
 if(fn!=L"Tick"||objectField(self,L"Player")!=player||!inLesson())return false;
 lessonActor=self;auto phase=state(self);lessonDrawing=phase==L"Draw";
 tutorialLessonPhase(self,lessonDrawing);
 static std::wstring previous;
 if(phase!=previous){log("{\"event\":\"hp1_lesson_phase\",\"phase\":\"%ls\",\"score\":%.5f,\"round\":%d}",phase.c_str(),mem<float>(self,field(self,L"Success",4,L"FloatProperty")),mem<int>(self,field(self,L"LessonLevel",4,L"IntProperty")));previous=phase;}
 if((phase!=L"Draw"&&phase!=L"Judge")||!lessonGeometry())return false;
 RestoreFields restore;
 // Place the original trail on the template plane: the desktop uses a closer
 // WandDist, which only aligns from its fixed monocular camera.
 restore.set(self,field(self,L"WandDist",4,L"FloatProperty"),mem<float>(self,field(self,L"TemplateDist",4,L"FloatProperty")));
 bool waiting=false;
 if(lessonDrawing){
  lessonFixtureInput();
  bool valid=controls.hand&&lessonCursor.valid&&!menuVisible;
  for(auto key:{L"SmoothMouseX",L"SmoothMouseY",L"aSideMove",L"aForMove"})restore.set(player,field(player,key,4,L"FloatProperty"),0.0f);
  restore.set(player,field(player,L"bAltFire",1,L"ByteProperty"),BYTE(valid&&controls.cast));
  if(valid){mem<float>(self,field(self,L"WandX",4,L"FloatProperty"))=lessonCursor.u-.5f;mem<float>(self,field(self,L"WandY",4,L"FloatProperty"))=.5f-lessonCursor.v;}
  waiting=!flag(self,L"bStartedDrawing")&&!(valid&&controls.cast);
  if(valid&&controls.cast)++lessonSamples;
 }
 originalInternal(self,frame,result);
 // Give the wearer time to find the starting point. Once drawing starts the
 // stock timer, sampled points, CompareGesture and reward/failure flow apply.
 if(waiting&&state(self)==L"Draw")mem<float>(self,field(self,L"TimeDrawn",4,L"FloatProperty"))=0;
 return true;
}
