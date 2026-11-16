#pragma once
#include "hp1_tutorial_catalogue.h"
#include "hp1_tutorial_catalogue_en.h"
// The native FindDialog out-parameters feed BOTH subtitle and sound duration.
// No separate Windows audio playback, artificial sleep or cinematic speed change.
static bool tutorialSpecific(const Hp1TutorialLine&);
static const Hp1TutorialLine* tutorialLine=nullptr;
static ULONGLONG tutorialStarted=0,tutorialEnds=0,tutorialFade=0;
static bool tutorialArmed[2]={};
static unsigned tutorialSerial=0,tutorialReplacements=0;
static std::vector<void*> tutorialSounds;
static void* tutorialLessonOwner=nullptr;
static bool tutorialLessonLine(const Hp1TutorialLine& line){
 std::wstring id=line.id;return id.find(L"lesson")!=std::wstring::npos||!_wcsicmp(line.id,L"sprout_new_72");
}
static void resetTutorial(){tutorialLine=nullptr;tutorialStarted=tutorialEnds=tutorialFade=0;tutorialArmed[0]=tutorialArmed[1]=false;}
static void beginTutorial(const Hp1TutorialLine& line,float seconds){
 tutorialLine=&line;tutorialStarted=GetTickCount64();tutorialEnds=tutorialStarted+ULONGLONG(std::max(7.f,std::min(35.f,seconds+7))*1000);tutorialFade=0;
 tutorialArmed[0]=tutorialArmed[1]=false;++tutorialSerial;
 log("{\"event\":\"hp1_tutorial_hint\",\"id\":\"%ls\",\"serial\":%u,\"action\":\"%s\",\"second_action\":\"%s\"}",line.id,tutorialSerial,line.actions[0],line.actions[1]);
}
static bool tutorialActionDown(const char* key){
 if(!std::strcmp(key,"jump"))return controls.jump;
 if(!std::strcmp(key,"cast"))return controls.cast;
 if(!std::strcmp(key,"move"))return std::hypot(controls.forward,controls.side)>.35f;
 if(!std::strcmp(key,"show_health"))return controls.showStatus;
 return false;
}
static void tutorialLessonClosed(void* actor){
 if(actor!=tutorialLessonOwner)return;
 tutorialLessonOwner=nullptr;resetTutorial();
}
static void tutorialLessonPhase(void* actor,bool drawing){
 tutorialLessonOwner=actor;
 // The native SayIntro/FindDialog starts the cue once. Draw and subsequent
 // teacher repetitions must not rearm it, including failed first attempts.
 if(drawing&&tutorialLine&&tutorialLessonLine(*tutorialLine)){
  log("{\"event\":\"hp1_tutorial_lesson_instruction_finished\",\"id\":\"%ls\",\"rearm_on_round\":false}",tutorialLine->id);resetTutorial();
 }
}
static float tutorialAlpha(bool ready,bool canAct){
 if(!tutorialLine)return 0;auto now=GetTickCount64();
 if(now>=tutorialEnds){resetTutorial();return 0;}
 if(!ready)return 0;
 for(unsigned i=0;i<2;++i)if(*tutorialLine->actions[i]){
  bool down=tutorialActionDown(tutorialLine->actions[i]);if(!canAct)tutorialArmed[i]=false;else if(!down)tutorialArmed[i]=true;
  if(canAct&&tutorialArmed[i]&&down&&now>tutorialStarted+1800&&!tutorialFade){tutorialFade=now;log("{\"event\":\"hp1_tutorial_hint_used\",\"id\":\"%ls\",\"action\":\"%s\"}",tutorialLine->id,tutorialLine->actions[i]);}
 }
 float fade=std::min(1.f,float(now-tutorialStarted)/250.f);
 if(tutorialFade)fade*=std::max(0.f,1.f-float(now-tutorialFade)/550.f);
 else fade*=std::min(1.f,float(tutorialEnds-now)/550.f);
 if(tutorialFade&&now>=tutorialFade+550){resetTutorial();return 0;}
 return fade;
}
static std::wstring tutorialString(void* locals,unsigned off){
 auto text=mem<wchar_t*>(locals,off);int count=mem<int>(locals,off+4),capacity=mem<int>(locals,off+8);
 need(count>=0&&count<4096&&capacity>=count,"Tutorial FString extent differs");
 if(!count)return L"";need(text&&text[count-1]==0,"Tutorial FString termination differs");return std::wstring(text,count-1);
}
static void* tutorialSound(const wchar_t* symbol){
 using Class=void*(__cdecl*)();using Load=void*(__cdecl*)(void*,void*,const wchar_t*,const wchar_t*,DWORD,void*);using Root=void(__thiscall*)(void*);
 auto core=GetModuleHandleW(L"Core.dll");
 auto cls=reinterpret_cast<Class>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?StaticClass@USound@@SAPAVUClass@@XZ"));
 auto load=reinterpret_cast<Load>(GetProcAddress(core,"?StaticLoadObject@UObject@@SAPAV1@PAVUClass@@PAV1@PBG2KPAVUPackageMap@@@Z"));
 auto root=reinterpret_cast<Root>(GetProcAddress(core,"?AddToRoot@UObject@@QAEXXZ"));
 if(!cls||!load||!root)return nullptr;
 auto path=std::wstring(startup.root)+L"\\assets\\tutorial\\"+(std::wcsncmp(symbol,L"HP1VRTutorialEN.",16)==0?L"HP1VRTutorialEN.uax":L"HP1VRTutorial.uax");
 if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)return nullptr;
 void* sound=load(cls(),nullptr,symbol,path.c_str(),0,nullptr); // Same reviewed loading flags as the independent entrance map.
 if(sound&&std::find(tutorialSounds.begin(),tutorialSounds.end(),sound)==tutorialSounds.end()){root(sound);tutorialSounds.push_back(sound);}
 return sound;
}
static void closeTutorialSounds(){
 using Root=void(__thiscall*)(void*);auto remove=reinterpret_cast<Root>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"?RemoveFromRoot@UObject@@QAEXXZ"));
 if(remove)for(void* sound:tutorialSounds)remove(sound);tutorialSounds.clear();resetTutorial();tutorialLessonOwner=nullptr;
}
static bool tutorialInternal(void* self,void* frame,void* result){
 if(!vrMovement()||!player||!frame||lobbyRendering||name(mem<void*>(frame,4))!=L"FindDialog")return false;
 void* narrator=objectField(player,L"theNarrator");if(!narrator||self!=objectField(narrator,L"CurDialog"))return false;
 void* locals=mem<void*>(frame,0x10);if(!locals)return false;
 auto id=tutorialString(locals,localField(frame,L"dialogID",12));const Hp1TutorialLine* line=nullptr;
 auto catalogue=vrPreferences.english?tutorialLinesEn:tutorialLines;
 for(unsigned i=0;i<sizeof(tutorialLines)/sizeof(tutorialLines[0]);++i)if(!_wcsicmp(id.c_str(),catalogue[i].id)){line=&catalogue[i];break;}
 if(!line)return false;

 Fpu fpu;fpu.restore();originalInternal(self,frame,result);
 unsigned variant=tutorialSpecific(*line)?0:1;
 void* sound=tutorialSound(line->sound[variant]);
 if(!sound){log("{\"event\":\"hp1_tutorial_fallback\",\"id\":\"%ls\",\"original_audio_and_text\":true}",line->id);return true;}
 using Assign=void*(__thiscall*)(void*,const wchar_t*);auto assign=reinterpret_cast<Assign>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"??4FString@@QAEAAV0@PBG@Z"));
 need(assign,"Tutorial string assignment API missing");
 mem<void*>(locals,localField(frame,L"dlgSound",4))=sound;assign(static_cast<BYTE*>(locals)+localField(frame,L"dlgText",12),line->text[variant]);
 beginTutorial(*line,line->seconds[variant]);++tutorialReplacements;
 log("{\"event\":\"hp1_tutorial_replaced\",\"id\":\"%ls\",\"variant\":%u,\"seconds\":%.6f,\"native_sound\":true,\"synthetic_voice\":true}",line->id,variant,line->seconds[variant]);
 fpu.restore();return true;
}
