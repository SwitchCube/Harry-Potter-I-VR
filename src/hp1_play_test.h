#pragma once
// Private original-menu/save fixture. Never runs in a real headset session.
static void playString(void* actor,const wchar_t* function,const wchar_t* argument){
 DWORD value[3]={};auto core=GetModuleHandleW(L"Core.dll");
 using Make=void(__thiscall*)(void*,const wchar_t*);using Delete=void(__thiscall*)(void*);
 auto make=reinterpret_cast<Make>(GetProcAddress(core,"??0FString@@QAE@PBG@Z"));auto destroy=reinterpret_cast<Delete>(GetProcAddress(core,"??1FString@@QAEXXZ"));
 // This engine exports the private destructor spelling used by ClientTravel.
 if(!destroy)destroy=reinterpret_cast<Delete>(GetProcAddress(core,"??1FString@@UAE@XZ"));
 if(!destroy)destroy=reinterpret_cast<Delete>(GetProcAddress(core,"??1FString@@QAE@XZ"));
 need(make&&destroy,"Play fixture FString API");make(value,argument);callScript(actor,function,value);destroy(value);
}
static void playReplayTick(bool cinematic){
 if(!playTestMode||!menuConsole)return;
 if(playTestMode==3){
  // Isolated COPY of a completed checkpoint. Exercise the actual menu GPU
  // transfer repeatedly across original slot reloads, with no save/score writes.
  static unsigned step=0,cycles=0,reloads=0,generation=0;
  static ULONGLONG due=GetTickCount64()+2000;auto now=GetTickCount64();controls={};
  if(step==4||now<due)return;
  if(step==3){if(levelGeneration<=generation)return;step=0;due=now+1000;return;}
  if(cinematic){controls.active=true;controls.skip=true;return;}
  if(step==0){need(!menuVisible,"Saved-game menu fixture starts closed");inputKey(27);step=1;}
  else if(step==1){need(menuVisible,"Saved-game book did not open");inputKey(27);step=2;}
  else if(step==2){
   need(!menuVisible,"Saved-game book did not close");++cycles;
   if(cycles==15){step=4;log("{\"event\":\"hp1_saved_menu_test_complete\",\"cycles\":%u,\"reloads\":%u,\"passed\":true}",cycles,reloads);return;}
   step=0;
   if(cycles%3==0){void* book=objectField(menuConsole,L"MenuBook");void* slot=objectField(book,L"SlotPage");need(slot,"Saved-game slot page");
    int selected=0;callScript(slot,L"SetSelectedSlot",&selected);generation=levelGeneration;++reloads;step=3;
    callNoArgs(slot,L"LoadSelectedSlot");log("{\"event\":\"hp1_saved_menu_reload\",\"number\":%u}",reloads);
   }
  }
  due=now+1000;return;
 }
 static unsigned phase=0;static ULONGLONG start=GetTickCount64(),next=0;auto now=GetTickCount64();if(now<next)return;next=now+500;
 void* book=objectField(menuConsole,L"MenuBook");if(!book)return;void* slot=objectField(book,L"SlotPage");if(!slot)return;
 if(phase==0&&now-start>2000&&!flag(book,L"bShowSplash")){
  playString(book,L"ChangePageNamed",L"SLOT");int index=0;callScript(slot,L"SetSelectedSlot",&index);
  if(playTestMode==1){callNoArgs(slot,L"CreateSelectedSlot");log("{\"event\":\"hp1_play_new_game\",\"original_slot\":0,\"storybook\":true}");}
  else {callNoArgs(slot,L"LoadSelectedSlot");log("{\"event\":\"hp1_play_load_requested\",\"original_slot\":0}");}
  phase=1;return;
 }
 if(phase!=1)return;
 if(objectField(book,L"curPage")==objectField(book,L"StoryBookPage")){callNoArgs(objectField(book,L"StoryBookPage"),L"GotoNextPage");return;}
 if(cinematic){controls.active=true;controls.skip=!controls.skip;return;}
 if(!levelGeneration||state(player)!=L"PlayerWalking"||skipActive)return;
 if(playTestMode==1){
  mem<int>(player,field(player,L"numBeans",4,L"IntProperty"))=137;
  int index=0;callScript(menuConsole,L"DoLevelSave",&index);
  log("{\"event\":\"hp1_play_saved\",\"original_save_api\":true,\"beans\":137,\"slot\":0}");
 }else{
  int beans=mem<int>(player,field(player,L"numBeans",4,L"IntProperty"));
  log("{\"event\":\"hp1_play_loaded\",\"beans\":%d,\"passed\":%s}",beans,beans==137?"true":"false");need(beans==137,"Loaded play save lost inventory");
 }
 controls={};phase=2;
}
