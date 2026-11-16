#pragma once
// Opt-in private profile only; real preferences and saves are never written.
static void settingsCapture(const wchar_t* filename){
 settingsPaint();GdiFlush();auto& p=settingsPanel;BITMAPFILEHEADER f={};BITMAPINFOHEADER h={};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(h);f.bfSize=f.bfOffBits+p.width*p.height*4;h.biSize=sizeof(h);h.biWidth=p.width;h.biHeight=-LONG(p.height);h.biPlanes=1;h.biBitCount=32;h.biSizeImage=p.width*p.height*4;auto out=_wfopen((std::wstring(startup.profile)+L"\\"+filename).c_str(),L"wb");need(out,"Settings capture file");fwrite(&f,sizeof(f),1,out);fwrite(&h,sizeof(h),1,out);fwrite(p.data,h.biSizeImage,1,out);fclose(out);
}
static bool settingsTest(){
 if(GetFileAttributesW((std::wstring(startup.profile)+L"\\test-vr-settings.flag").c_str())==INVALID_FILE_ATTRIBUTES)return true;
 static unsigned stage=0;static ULONGLONG at=0;auto now=GetTickCount64();
 auto book=objectField(menuConsole,L"MenuBook");auto report=objectField(book,L"ReportPage");
 if(stage==0){need(settingsButton&&settingsOwner==report,"Native VR settings button missing");
  struct Notify{void* button;BYTE event;BYTE pad[3];} click={settingsButton,2,{}};callScript(report,L"Notify",&click);
  need(settingsOpen&&objectField(book,L"curPage")==report,"VR button replaced the native page");lobbyTestMenuCapture=21;settingsCapture(L"settings-de.bmp");stage=1;at=now;return false;}
 if(stage==1&&now-at>600){
  settingsClick(.5f,500.f/800);need(vrPreferences.language==1,"German language selection");settingsClick(.5f,500.f/800);need(vrPreferences.english,"English language selection");
  settingsClick(.5f,390.f/800);need(vrPreferences.resolution==1024,"Render resolution cycle");
  settingsClick(.5f,170.f/800);need(settingsPage==1,"Bindings page missing");
  settingsClick(.5f,150.f/800);need(settingsWaiting==0,"Cast binding capture");
  for(auto& raw:actions.raw)raw=false;settingsButtons();actions.raw[1]=true;settingsButtons();actions.raw[1]=false;
  need(vrPreferences.source[0]==1&&vrPreferences.source[1]==0,"Remapping must swap duplicate assignments");settingsCapture(L"settings-bindings-en.bmp");
  tutorialTestDriver();need(actions.mappedHandle("cast")==actions.handles.at("jump"),"Tutorial mapping bypassed the configured source");
  beginTutorial(tutorialLines[10],5);tutorialModels.resolve();need(tutorialModels.devices.size()==1&&tutorialModels.devices[0].highlights[0]=="a","Actual driver origin must highlight remapped A");
  tutorialModels.close();runtime.input=nullptr;runtime.system=nullptr;runtime.compositor=nullptr;actions.set=0;actions.handles.clear();resetTutorial();
  settingsClick(.5f,650.f/800);need(vrPreferences.swapSticks,"Stick swap missing");
  vrPreferences.load(startup.root,startup.profile);need(vrPreferences.english&&vrPreferences.resolution==1024&&vrPreferences.source[0]==1&&vrPreferences.swapSticks,"Saved settings did not survive reload");
  settingsClick(.5f,745.f/800);settingsCapture(L"settings-en.bmp");settingsClick(.5f,285.f/800);need(!vrPreferences.spatial,"Flat cinema selection");
  settingsClick(.5f,745.f/800);need(!settingsOpen,"Return to vanilla report");
  playString(book,L"ChangePageNamed",L"OPTIONS");auto options=objectField(book,L"OptionsPage");need(objectField(book,L"curPage")==options&&!settingsOpen,"Original options replaced");
  struct Click{void* button;BYTE event;BYTE pad[3];} click={mem<void*>(options,field(options,L"KeyButtons",4,L"ObjectProperty")),2,{}};callScript(options,L"Notify",&click);need(flag(options,L"bPolling"),"Original key capture did not start");
  struct Keys{BYTE key,action,pad[2];float delta;DWORD result;} keys={84,1,{},0,0};callScript(options,L"KeyEvent",&keys);need(!flag(options,L"bPolling")&&mem<int>(options,field(options,L"BoundKey1",4,L"IntProperty"))==84,"Original keyboard rebinding failed");
  lobbyTestMenuCapture=22;stage=2;at=now;return false;
 }
 if(stage==2&&now-at>700){playString(book,L"ChangePageNamed",L"REPORT");stage=3;at=now;return false;}
 if(stage==3&&now-at>700){
  log("{\"event\":\"hp1_settings_test_complete\",\"native_button\":true,\"remapped_driver_component\":true,\"persistence\":true,\"english\":true,\"resolution\":1024,\"passed\":true}");stage=4;return true;
 }return stage==4;
}
