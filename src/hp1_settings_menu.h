#pragma once
// Additional native button on FEReportPage; no stock pages are replaced.
static void* settingsButton=nullptr;
static void* settingsOwner=nullptr;
static VrPanel settingsPanel;
static int settingsPage=0,settingsWaiting=-1;
static bool settingsReleased=false,settingsLastRaw[6]={};
static unsigned settingsButtonLanguage=0;
static float settingsPointerX=-1,settingsPointerY=-1;
static const wchar_t* settingsAction(int i){
 const wchar_t* de[]={L"Zaubern / Beschleunigen",L"Springen / Bremsen",L"Men\u00fc",L"Aktion / Szene abk\u00fcrzen",L"Neu zentrieren",L"Inventar anzeigen"};
 const wchar_t* en[]={L"Cast / Accelerate",L"Jump / Brake",L"Menu",L"Action / Skip scene",L"Recenter",L"Show inventory"};return vrPreferences.english?en[i]:de[i];
}
static std::wstring settingsSource(int i){
 if(runtime.input&&actions.set){VRInputValueHandle_t origins[16]={};auto handle=actions.handles.at(Hp1Preferences::keys[i]);
  if(runtime.input->GetActionOrigins(actions.set,handle,origins,16)==EVRInputError_VRInputError_None)for(auto o:origins)if(o){char text[256]={};if(runtime.input->GetOriginLocalizedName(o,text,sizeof(text),EVRInputStringBits_VRInputString_Hand|EVRInputStringBits_VRInputString_InputSource)==EVRInputError_VRInputError_None&&*text)return Hp1TutorialModels::wide(text);}}
 const wchar_t* de[]={L"Rechter Trigger",L"A",L"B",L"X",L"Linker Stickklick",L"Linke untere Grifftaste"};
 const wchar_t* en[]={L"Right trigger",L"A",L"B",L"X",L"Left stick click",L"Left lower grip"};return vrPreferences.english?en[i]:de[i];
}
static unsigned settingsParam(void* fn,const wchar_t* key){auto p=findProperty(fn,key);need(p,"HP1 settings function parameter missing");auto off=mem<DWORD>(p,0x48);need(off<500,"HP1 settings parameter extent differs");return off;}
static void settingsNativeButton(void* book){
 void* page=objectField(book,L"curPage");if(!page||page!=objectField(book,L"ReportPage"))return;
 if(settingsOwner!=page){
  settingsButton=nullptr;settingsOwner=page;void* reference=objectField(page,L"QuitLabel");need(reference,"HP1 report button missing");
  for(void* child=objectField(page,L"FirstChildWindow");child;child=objectField(child,L"NextSiblingWindow")){
   if(findProperty(mem<void*>(child,0x24),L"HelpText")&&tutorialString(child,field(child,L"HelpText",12,L"StrProperty"))==L"HP1VR.Settings"){settingsButton=child;break;}
  }
  if(!settingsButton){auto fn=scriptFunction(page,L"CreateControl");BYTE params[512]={};
  mem<void*>(params,settingsParam(fn,L"ControlClass"))=mem<void*>(reference,0x24);
  for(auto kv:{std::pair<const wchar_t*,float>{L"X",375},{L"Y",18},{L"W",200},{L"H",28}})mem<float>(params,settingsParam(fn,kv.first))=kv.second;
  callScript(page,L"CreateControl",params);settingsButton=mem<void*>(params,settingsParam(fn,L"ReturnValue"));need(settingsButton,"HP1 VR button creation failed");
   using Assign=void*(__thiscall*)(void*,const wchar_t*);auto assign=reinterpret_cast<Assign>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"??4FString@@QAEAAV0@PBG@Z"));need(assign,"Settings button tag API");assign(static_cast<BYTE*>(settingsButton)+field(settingsButton,L"HelpText",12,L"StrProperty"),L"HP1VR.Settings");
  }
  mem<int>(settingsButton,field(settingsButton,L"Font",4,L"IntProperty"))=mem<int>(reference,field(reference,L"Font",4,L"IntProperty"));
  mem<BYTE>(settingsButton,field(settingsButton,L"Align",1,L"ByteProperty"))=2;
  mem<DWORD>(settingsButton,field(settingsButton,L"TextColor",4,L"StructProperty"))=0xff86ddff;
  setFlag(settingsButton,L"bShadowText",true);settingsButtonLanguage=~0u;
  log("{\"event\":\"hp1_vr_settings_button\",\"page\":\"FEReportPage\",\"original_controls_preserved\":true}");
 }
 if(settingsButtonLanguage!=vrPreferences.revision){
  using Assign=void*(__thiscall*)(void*,const wchar_t*);auto assign=reinterpret_cast<Assign>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"??4FString@@QAEAAV0@PBG@Z"));need(assign,"HP1 button text API missing");
  assign(static_cast<BYTE*>(settingsButton)+field(settingsButton,L"Text",12,L"StrProperty"),tr(L"VR-Einstellungen",L"VR Settings"));settingsButtonLanguage=vrPreferences.revision;
 }
}
static void settingsClick(float,float);
static bool settingsInternal(void* self,void* frame,void* result){
 if(!frame||!vrMovement())return false;auto fn=name(mem<void*>(frame,4));void* locals=mem<void*>(frame,0x10);
 if(settingsButton&&self==settingsOwner&&fn==L"Notify"&&locals&&mem<void*>(locals,0)==settingsButton&&mem<BYTE>(locals,4)==2){
  settingsOpen=true;settingsPage=0;settingsWaiting=-1;settingsReleased=false;log("{\"event\":\"hp1_vr_settings_open\"}");return true;
 }
 if(settingsOpen&&menuConsole&&fn==L"KeyEvent"&&self==objectField(menuConsole,L"MenuBook")&&locals){
  BYTE key=mem<BYTE>(locals,0),action=mem<BYTE>(locals,1);if(key==27&&action==1){settingsOpen=false;settingsWaiting=-1;}
  if(key==1&&action==1){auto root=objectField(menuConsole,L"Root");float x=mem<float>(menuConsole,field(menuConsole,L"MouseX",4,L"FloatProperty")),y=mem<float>(menuConsole,field(menuConsole,L"MouseY",4,L"FloatProperty"));settingsClick(x/mem<float>(root,field(root,L"WinWidth",4,L"FloatProperty")),y/mem<float>(root,field(root,L"WinHeight",4,L"FloatProperty")));}if(result)*static_cast<DWORD*>(result)=1;return true;
 }return false;
}
static void settingsClick(float u,float v){
 if(!settingsOpen||u<.045f||u>.955f)return;int y=int(v*800);
 if(y>=706&&y<780){if(settingsWaiting>=0)settingsWaiting=-1;else if(settingsPage)settingsPage=0;else settingsOpen=false;return;}
 if(settingsWaiting>=0)return;
 if(settingsPage==1){
  if(y>=130&&y<610){settingsWaiting=(y-130)/80;settingsReleased=false;return;}
  if(y>=620&&y<690){vrPreferences.swapSticks=!vrPreferences.swapSticks;vrPreferences.save();}return;
 }
 if(y>=130&&y<225)settingsPage=1;
 else if(y>=240&&y<335){vrPreferences.spatial=!vrPreferences.spatial;vrPreferences.save();}
 else if(y>=350&&y<445){const int sizes[]={1024,1280,1536,2048};int i=0;while(i<3&&sizes[i]!=vrPreferences.resolution)++i;vrPreferences.resolution=sizes[(i+1)%4];vrPreferences.save();}
 else if(y>=460&&y<555){vrPreferences.language=(vrPreferences.language+1)%3;vrPreferences.locale();vrPreferences.save();handStatus.painted=false;}
}
static bool settingsButtons(){
 if(!settingsOpen)return false;
 if(settingsWaiting>=0){bool any=false;for(bool b:actions.raw)any|=b;if(!any)settingsReleased=true;
  if(settingsReleased)for(int i=0;i<6;++i)if(actions.raw[i]&&!settingsLastRaw[i]){vrPreferences.assign(settingsWaiting,i);vrPreferences.save();settingsWaiting=-1;settingsReleased=false;break;}
 }else if(controls.menu&&!settingsLastRaw[2]){if(settingsPage)settingsPage=0;else settingsOpen=false;}
 for(int i=0;i<6;++i)settingsLastRaw[i]=actions.raw[i];return true;
}
static void settingsPaint(){
 auto& p=settingsPanel;p.open(1000,800);p.clear();p.text(36,20,920,tr(L"VR-Einstellungen",L"VR Settings"),42,RGB(246,212,134));
 auto row=[&](int y,int h,const std::wstring& title,const std::wstring& value){bool over=settingsPointerX>=.035f&&settingsPointerX<=.965f&&settingsPointerY*800>=y&&settingsPointerY*800<y+h;p.box(35,y,930,h,over?RGB(87,76,46):RGB(40,47,71));p.text(52,y+((h<90&&!value.empty())?-7:2),894,title.c_str(),29);if(!value.empty())p.text(52,y+(h<90?25:37),894,value.c_str(),h<90?24:25,RGB(251,216,128));};
 if(settingsPage==1){
  for(int i=0;i<6;++i)row(130+i*80,72,settingsAction(i),settingsWaiting==i?tr(L"Alle Tasten loslassen, dann neue Taste dr\u00fccken",L"Release all buttons, then press the new button"):settingsSource(vrPreferences.source[i]));
  row(620,70,tr(L"Bewegen / Drehen: Sticks tauschen",L"Movement / Turning: swap sticks"),vrPreferences.swapSticks?tr(L"Getauscht",L"Swapped"):tr(L"Standard",L"Default"));
 }else{
  row(130,95,tr(L"Controller-Tastenbelegung",L"Controller bindings"),tr(L"Aktion w\u00e4hlen und neue Taste dr\u00fccken",L"Choose an action, then press its new button"));
  row(240,95,tr(L"Zwischensequenzen",L"Cinematics"),vrPreferences.spatial?L"3D VR":tr(L"Kino / Flat",L"Cinema / Flat"));
  row(350,95,tr(L"VR-Renderaufl\u00f6sung (ab n\u00e4chstem Spielstart)",L"VR render resolution (next game launch)"),std::to_wstring(vrPreferences.resolution)+L" x "+std::to_wstring(vrPreferences.resolution));
  row(460,95,tr(L"Sprache der VR-Mod",L"VR mod language"),vrPreferences.language==0?tr(L"Automatisch (Spielsprache)",L"Automatic (game language)"):vrPreferences.language==1?L"Deutsch":L"English");
  p.text(38,575,920,tr(L"Originale Maus-/Tastaturoptionen bleiben im Spielmen\u00fc.",L"Original mouse/keyboard settings remain in the game menu."),24);
  p.text(38,617,920,tr(L"Gesprochene Hinweise beschreiben die Standardbelegung.",L"Spoken hints describe the default bindings."),24);
 }
 row(706,74,settingsWaiting>=0?tr(L"Abbrechen",L"Cancel"):tr(L"Zur\u00fcck",L"Back"),L"");
 if(settingsPointerX>=0){int x=int(settingsPointerX*1000),y=int(settingsPointerY*800);p.box(x-10,y-2,20,4,RGB(255,255,255));p.box(x-2,y-10,4,20,RGB(255,255,255));}
}
