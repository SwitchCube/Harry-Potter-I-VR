#pragma once
// Explicit private native-gameplay fixtures only. They never run in native-vr
// or a normal play profile, and do not activate or replace real progress.
static void saveLetterFixture(unsigned index){
 auto& p=letterView.panel;GdiFlush();BITMAPFILEHEADER file={};BITMAPINFOHEADER info={};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+p.width*p.height*4;
 info.biSize=sizeof(info);info.biWidth=p.width;info.biHeight=-LONG(p.height);info.biPlanes=1;info.biBitCount=32;info.biSizeImage=p.width*p.height*4;
 wchar_t suffix[64];std::swprintf(suffix,64,L"\\letter-%u.bmp",index);FILE* out=_wfopen((std::wstring(startup.profile)+suffix).c_str(),L"wbx");need(out,"Letter fixture output");
 bool ok=fwrite(&file,sizeof(file),1,out)==1&&fwrite(&info,sizeof(info),1,out)==1&&fwrite(p.data,info.biSizeImage,1,out)==1;fclose(out);need(ok,"Letter fixture image");
}
static void* spawnStoryFixture(const wchar_t* path){
 auto core=GetModuleHandleW(L"Core.dll");using Find=void*(__cdecl*)(void*,void*,const wchar_t*,int);using Load=void*(__cdecl*)(void*,void*,const wchar_t*,const wchar_t*,DWORD,void*);
 auto find=reinterpret_cast<Find>(GetProcAddress(core,"?StaticFindObject@UObject@@SAPAV1@PAVUClass@@PAV1@PBGH@Z"));auto load=reinterpret_cast<Load>(GetProcAddress(core,"?StaticLoadClass@UObject@@SAPAVUClass@@PAV2@PAV1@PBG2KPAVUPackageMap@@@Z"));need(find&&load,"Fixture class API");
 void* actorClass=find(nullptr,nullptr,L"Engine.Actor",0);need(actorClass,"Native Actor class");void* klass=load(actorClass,nullptr,path,nullptr,0,nullptr);need(klass,"Native scroll class");
 // HP1 execSpawn at Engine RVA10c700 independently confirms the exported ABI:
 // Class, Name, Owner, Instigator, Location, Rotation, Template, no-collision,
 // remote-owned. Only this private setup bypasses spawn-position collision.
 using Spawn=void*(__thiscall*)(void*,void*,DWORD,void*,void*,Vec,Rot,void*,int,int);
 auto spawn=reinterpret_cast<Spawn>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?SpawnActor@ULevel@@UAEPAVAActor@@PAVUClass@@VFName@@PAV2@PAVAPawn@@VFVector@@VFRotator@@2HH@Z"));need(spawn,"Native scroll spawn API");
 auto location=hpvr::add(mem<Vec>(player,field(player,L"Location",12,L"StructProperty")),Vec{80,0,36});
 void* scroll=spawn(mem<void*>(player,0x80),klass,0,player,player,location,Rot{},nullptr,1,0);need(scroll,"Original scroll spawn failed");return scroll;
}
static void storyFixtureTick(bool cinematic){
 if(!storyTestMode)return;
 static unsigned phase=0,index=0;static ULONGLONG next=0,began=0;static std::vector<void*> targets;static unsigned draws=0;static std::wstring firstLetter;
 auto now=GetTickCount64();controls={};controls.active=true;
 if(menuVisible){inputKey(27);return;}
 if(storyTestMode==1){
  if(phase==0){
   for(auto candidate:{L"tut3peeves2",L"BossRailMove0",L"fluffy0",L"BossQuirrel0",L"BroomDraco0"})if(void* actor=mechanicsFind(candidate))targets.push_back(actor);
   need(!targets.empty(),"No original boss in fixture map");phase=1;next=now+1500;return;
  }
  if(phase==1&&now>=next){
   if(index<targets.size()*2+1){storyBossPhase=int(index);storyBossTarget=index<targets.size()*2?targets[index/2]:player;storyBossHalf=index%2!=0;phase=2;next=now+6000;}
   else {storyBossSuppress=true;draws=bossUi.draws;phase=3;next=now+700;}
  }else if(phase==2){
   if(storyBossCaptured==storyBossPhase){need((bossUi.visiblePixels>100)==(index<targets.size()*2),"Native boss visibility condition differs");++index;phase=1;next=now+100;}
   else need(now<next,"Original HUD never rendered boss fixture");
  }else if(phase==3&&now>=next){
   need(bossUi.draws==draws,"Boss bar rendered in original cinematic suppression");storyBossTarget=nullptr;bossUi.hide();phase=4;
   log("{\"event\":\"hp1_boss_ui_test_complete\",\"bosses\":%u,\"samples\":%u,\"non_boss_hidden\":true,\"cinematic_hidden\":true,\"passed\":true}",unsigned(targets.size()),index);
  }
  return;
 }
 void* hud=objectField(player,L"myHUD");
 if(phase==0){
  if(cinematic){controls.skip=true;return;}if(skipActive)return;
  void* scroll=spawnStoryFixture(index?L"HarryPotter.demoScroll":L"HarryPotter.HedwigsScroll");callScript(scroll,L"Touch",&player);need(isLetter(objectField(hud,L"curPopup")),"Original scroll Touch did not show letter");
  controls.cast=true;phase=1;began=now;return;
 }
 if(phase==1){
  controls.cast=true;need(isLetter(objectField(hud,L"curPopup")),"Held button dismissed arriving letter");
  if(now-began>=2000){need(!letterText.empty()&&letterView.panel.texture.verifiedMask==1,"Readable localized letter was not uploaded");saveLetterFixture(index);
   if(!index)firstLetter=letterText;else need(firstLetter!=letterText,"Two native letters resolved to identical text");
   log("{\"event\":\"hp1_letter_ui_sample\",\"letter\":%u,\"characters\":%u,\"held_arrival_preserved\":true,\"gpu_verified\":true}",index,unsigned(letterText.size()));phase=2;began=now;controls.cast=false;
  }
 }else if(phase==2){if(now-began>800){controls.skip=true;phase=3;began=now;}}
 else if(phase==3){
  if(!isLetter(objectField(hud,L"curPopup"))&&!flag(hud,L"bCutSceneMode")&&state(player)==L"PlayerWalking"){
   if(++index==2){phase=4;log("{\"event\":\"hp1_mail_ui_test_complete\",\"letters\":2,\"original_scroll_release\":true,\"passed\":true}");}else phase=0;
  }else need(now-began<5000,"Letter confirmation did not restore original play");
 }
}
