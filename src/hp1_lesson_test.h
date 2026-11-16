#pragma once
static void beginLessonFixture(bool cinematic){
 static bool started=false;if((!lessonFixture()&&!directLessonStart())||started||cinematic||inLesson()||state(player)!=L"PlayerWalking")return;
 if(skipActive&&skipLevel){mem<float>(skipLevel,field(skipLevel,L"TimeDilation",4,L"FloatProperty"))=savedTimeDilation;skipActive=false;skipLevel=nullptr;}
 void* level=mem<void*>(player,0x80);void** actors=mem<void**>(level,0x2c);int count=mem<int>(level,0x30);need(actors&&count>0&&count<100000,"HP1 fixture actor array");void* trigger=nullptr;
 for(int i=0;i<count;++i)if(actors[i]&&name(mem<void*>(actors[i],0x24))==L"SpellLearnTrigger"){trigger=actors[i];break;}
 need(trigger!=nullptr,"No original lesson in fixture level");
 using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);auto move=reinterpret_cast<FarMove>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?FarMoveActor@ULevel@@UAEHPAVAActor@@VFVector@@HH@Z"));need(move!=nullptr,"Fixture move API");
 Vec location=mem<Vec>(trigger,field(trigger,L"Location",12,L"StructProperty"));need(move(level,player,location,0,0)!=0,"Lesson fixture placement collision");
 void* fn=scriptFunction(trigger,L"Trigger");for(auto pair:{std::pair<const wchar_t*,unsigned>{L"Other",0},{L"EventInstigator",4}}){void* p=findProperty(fn,pair.first);need(p&&mem<DWORD>(p,0x48)==pair.second&&mem<DWORD>(p,0x38)==4,"Lesson Trigger fixture ABI");}
 struct {void* other;void* instigator;} args={player,player};started=true;callScript(trigger,L"Trigger",&args);
 log("{\"event\":\"hp1_lesson_fixture_started\",\"variant\":%d,\"original_trigger\":true}",lessonFixture());
}
