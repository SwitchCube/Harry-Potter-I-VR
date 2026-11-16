#pragma once
// Menu/story/ending transitions on an isolated checkpoint, never live input.
static void lobbyTestTick(){
 if(!lobbyTestMode||!menuConsole)return;
 static unsigned phase=0;static ULONGLONG at=0;static int storyPage=0;static float creditsAt=0;static unsigned captureMask=0,generation=0,reloads=0;
 auto now=GetTickCount64();if(!at)at=now;controls={};controls.active=true;
 void* book=objectField(menuConsole,L"MenuBook");if(!book)return;
 auto page=[&]{return objectField(book,L"curPage");};
 auto is=[&](const wchar_t* key){return page()&&page()==objectField(book,key);};
 auto next=[&](unsigned n){phase=n;at=now;};
 auto capturePhase=[&](unsigned n){if(captureMask&(1u<<n))return false;captureMask|=1u<<n;lobbyTestCapture=lobbyTestMenuCapture=n;return true;};
 if(phase==0&&now-at>2000&&!flag(book,L"bShowSplash")){setFlag(book,L"bGamePlaying",true);playString(book,L"OpenBook",L"REPORT");next(1);return;}
 if(phase==1&&now-at>800){if(!settingsTest())return;need(!lobbyWanted(),"Pause menu must retain real level");
  void* report=page();struct Notify{void* button;BYTE event;BYTE padding[3];} click={objectField(report,L"QuitButton"),2,{}};
  need(mem<DWORD>(findProperty(scriptFunction(report,L"Notify"),L"E"),0x48)==4,"Notify argument layout");callScript(report,L"Notify",&click);
  need(objectField(report,L"ConfirmQuit"),"Quit confirmation missing");next(101);return;}
 if(phase==101&&now-at>600){auto confirm=objectField(page(),L"ConfirmQuit");need(confirm,"Quit confirmation closed early");
  struct Notify{void* button;BYTE event;BYTE padding[3];} click={objectField(confirm,L"button1"),2,{}};callScript(confirm,L"Notify",&click);next(2);return;}
 if(phase==2&&now-at>1200){need(lobbyLevel&&lobbyWanted()&&is(L"MainPage"),"Main menu entrance missing");if(capturePhase(2))return;playString(book,L"ChangePageNamed",L"SLOT");next(3);return;}
 if(phase==3&&now-at>1200){need(lobbyWanted()&&is(L"SlotPage"),"Save menu entrance missing");if(capturePhase(3))return;
  void* story=objectField(book,L"StoryBookPage");need(story,"Story page missing");int args[2]={0,0};callScript(story,L"SetStoryAndPage",args);playString(book,L"OpenBook",L"STORYBOOKPAGE");next(4);return;}
 if(phase==4&&now-at>1200){need(lobbyWanted()&&is(L"StoryBookPage"),"Story entrance missing");if(capturePhase(4))return;auto story=page();storyPage=mem<int>(story,field(story,L"iCurrentPage",4,L"IntProperty"));controls.skip=true;next(5);return;}
 if(phase==5&&now-at>800){need(is(L"StoryBookPage")&&mem<int>(page(),field(page(),L"iCurrentPage",4,L"IntProperty"))==storyPage+1,"X did not advance original story page");
  // The real quit confirmation clears bGamePlaying. This separate ending
  // fixture requires the same active-play state as an actual end-game event.
  setFlag(book,L"bGamePlaying",true);playString(book,L"OpenBook",L"ENDGAME");next(6);return;}
 if(phase==6&&now-at>1500){need(is(L"FolioPage")&&flag(book,L"bInEndGame"),"Native ending folio missing");if(capturePhase(6))return;controls.skip=true;creditsAt=mem<float>(objectField(player,L"Level"),field(objectField(player,L"Level"),L"TimeSeconds",4,L"FloatProperty"));next(7);return;}
 if(phase==7){controls.skip=true;if(now-at>6000){need(is(L"CreditsPage"),"Held X skipped or failed to open credits");need(mem<float>(objectField(player,L"Level"),field(objectField(player,L"Level"),L"TimeSeconds",4,L"FloatProperty"))>creditsAt+1,"Native credits clock stopped");if(capturePhase(7))return;next(8);}return;}
 if(phase==8&&now-at>500){controls.menu=true;next(9);return;}
 if(phase==9&&now-at>800){need(is(L"MainPage"),"B did not leave credits");setFlag(book,L"bGamePlaying",true);playString(book,L"OpenBook",L"ENDGAME");next(10);return;}
 if(phase==10&&now-at>800){controls.menu=true;next(11);return;}
 if(phase==11&&now-at>800){need(is(L"CreditsPage")&&lobbyTestEyes>=10,"B did not advance ending / missing stereo samples");
  void* slot=objectField(book,L"SlotPage");int selected=0;callScript(slot,L"SetSelectedSlot",&selected);generation=levelGeneration;callNoArgs(slot,L"LoadSelectedSlot");next(12);return;}
 if(phase==12&&now-at>1800&&levelGeneration>generation){need(flatSceneWanted()?(lobbyWanted()&&lobbyWasVisible):(!lobbyWanted()&&!lobbyWasVisible),"Loaded scene did not select the configured world/cinema environment");playString(book,L"OpenBook",L"MAIN");next(13);return;}
 if(phase==13&&now-at>1500){need(lobbyWasVisible&&lobbyLevel&&is(L"MainPage"),"Entrance lost after native reload");
  if(capturePhase(13))return;
  if(++reloads<2){void* slot=objectField(book,L"SlotPage");int selected=0;callScript(slot,L"SetSelectedSlot",&selected);generation=levelGeneration;callNoArgs(slot,L"LoadSelectedSlot");next(12);return;}
  void* slot=objectField(book,L"SlotPage");int selected=1;callScript(slot,L"SetSelectedSlot",&selected);generation=levelGeneration;callNoArgs(slot,L"CreateSelectedSlot");next(14);return;
 }
 if(phase==14){
  if(levelGeneration<=generation){if(is(L"StoryBookPage"))controls.skip=((now-at)/400)%2==1;need(now-at<40000,"New game story did not travel");return;}
  if(now-at<1000)return;
  void* realLevel=mem<void*>(player,0x80);need(realLevel!=lobbyLevel&&mechanicsFind(L"PlayerStart0"),"New game did not load a separate native tutorial map");
  playString(book,L"OpenBook",L"MAIN");next(15);return;
 }
 if(phase==15&&now-at>1200){
  if(GetFileAttributesW((std::wstring(startup.profile)+L"\\test-vr-settings.flag").c_str())!=INVALID_FILE_ATTRIBUTES)need(cinemaSamples>0,"Flat cinema did not capture the original HUD in the entrance hall");
  need(lobbyWanted()&&lobbyWasVisible&&lobbyLevel!=mem<void*>(player,0x80),"New-game entrance aliases gameplay map");
  if(capturePhase(15))return;
  log("{\"event\":\"hp1_lobby_test_complete\",\"main_save_story\":true,\"pause_keeps_world\":true,\"story_x\":true,\"cards_x_and_b\":true,\"held_x_blocked\":true,\"credits_clock\":true,\"new_game_distinct_map\":true,\"reloads\":%u,\"eye_samples\":%u,\"passed\":true}",reloads,lobbyTestEyes);next(99);
 }
}
