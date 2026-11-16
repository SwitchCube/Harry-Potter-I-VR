#pragma once
// Explicit private --stereo-check only; never entered by a user's VR session.
static bool stereoRoomCaptured=false,stereoRoomPending=false;
static void stereoFixtureStatus(){
 auto before=readHp1Status(player);
 {
  RestoreFields restore;
  auto number=[&](const wchar_t* key,int value){restore.set(player,field(player,key,4,L"IntProperty"),value);};
  float max=mem<float>(player,field(player,L"MaxLifePotions",4,L"FloatProperty"));
  restore.set(player,field(player,L"lifePotions",4,L"FloatProperty"),max*.75f);
  number(L"numBeans",123);number(L"numStars",7);number(L"iFireSeedCount",12);
  number(L"numHousePointsGryffindor",87);number(L"numHousePointsSlytherin",93);number(L"numHousePointsRavenclaw",67);number(L"numHousePointsHufflepuff",58);
  // Reflect the FIRST HP1 card's bool, then restore it. Exercise nonzero counts.
  unsigned cards=field(player,L"WizardCards",8,L"StructProperty");
  void* property=findProperty(mem<void*>(player,0x24),L"WizardCards");
  void* structure=mem<void*>(property,0x5c),*hasCard=findProperty(structure,L"bHasCard");
  need(hasCard&&mem<DWORD>(hasCard,0x48)==4&&mem<DWORD>(hasCard,0x5c)==1,"HP1 card fixture layout differs");
  int delta=(mem<DWORD>(player,cards+4)&1)?0:1;
  restore.set(player,cards+4,mem<DWORD>(player,cards+4)|1u);
  auto sample=readHp1Status(player);
  need(sample.health==75&&sample.beans==123&&sample.stars==7&&sample.seeds==12&&sample.cards==before.cards+delta&&sample.points[0]==87,"HP1 reflected status fixture differs");
  handStatus.paint(sample);handStatus.panel.upload(0,nullptr);
  auto& panel=handStatus.panel;GdiFlush();
  BITMAPFILEHEADER file={};BITMAPINFOHEADER info={};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+panel.width*panel.height*4;
  info.biSize=sizeof(info);info.biWidth=panel.width;info.biHeight=-LONG(panel.height);info.biPlanes=1;info.biBitCount=32;info.biSizeImage=panel.width*panel.height*4;
  FILE* out=_wfopen((std::wstring(startup.profile)+L"\\hand-status.bmp").c_str(),L"wbx");need(out!=nullptr,"Status fixture output");
  bool written=fwrite(&file,sizeof(file),1,out)==1&&fwrite(&info,sizeof(info),1,out)==1&&fwrite(panel.data,info.biSizeImage,1,out)==1;fclose(out);need(written,"Write status fixture");
  log("{\"event\":\"hp1_status_fixture\",\"health\":%d,\"beans\":%d,\"stars\":%d,\"cards\":%d,\"seeds\":%d,\"gpu_readback_verified\":true}",sample.health,sample.beans,sample.stars,sample.cards,sample.seeds);
 }
 need(readHp1Status(player)==before,"Status fixture did not restore player values");handStatus.close();
 log("{\"event\":\"hp1_status_fixture_restored\",\"passed\":true}");
}
static void stereoFixtureRoom(bool cinema){
 if(!stereoFixture()||stereoRoomCaptured||stereoRoomPending||cinema||menuVisible||!replayDone||GetTickCount64()-replayMoveStarted<7400)return;
 stereoFixtureStatus();
 using GetLevel=void*(__thiscall*)(void*);using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);
 auto engine=GetModuleHandleW(L"Engine.dll");
 auto getLevel=reinterpret_cast<GetLevel>(GetProcAddress(engine,"?GetLevel@AActor@@QBEPAVULevel@@XZ"));
 auto move=reinterpret_cast<FarMove>(GetProcAddress(engine,"?FarMoveActor@ULevel@@UAEHPAVAActor@@VFVector@@HH@Z"));
 need(getLevel&&move,"Room fixture placement API");
 need(move(getLevel(player),player,Vec{-32,-4210,92.5f},0,0)!=0,"Room fixture collision-safe placement");
 // Deliberately leave the ORIGINAL third-person camera in the previous room.
 // Both stereo eyes must still see the new room, including correct BSP zone.
 mem<Vec>(player,field(player,L"Velocity",12,L"StructProperty"))={};
 mem<Vec>(player,field(player,L"Acceleration",12,L"StructProperty"))={};
 trackingOrigin.recenter(hpvr::identity(),-hpvr::pi/2);stereoRoomPending=true;
 log("{\"event\":\"hp1_room_fixture\",\"source_camera_retained\":true,\"new_position\":[-32,-4210,92.5]}");
}
static void stereoFixtureCleanup(unsigned eye){
 if(!stereoFixture()||eye!=1||!stereoFixtureProjectile)return;
 void* actor=stereoFixtureProjectile;
 need(!flag(actor,L"bDeleteMe"),"Fixture projectile expired before secondary cleanup test");
 using GetLevel=void*(__thiscall*)(void*);
 auto getLevel=reinterpret_cast<GetLevel>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?GetLevel@AActor@@QBEPAVULevel@@XZ"));need(getLevel!=nullptr,"Fixture level API");
 unsigned before=secondaryLifecycleEvents;
 int removed=destroyActorHook(getLevel(actor),nullptr,actor,0);
 need(removed&&flag(actor,L"bDeleteMe")&&secondaryLifecycleEvents==before+1,"Secondary native destruction not dispatched exactly once");
 stereoFixtureProjectile=nullptr;
 log("{\"event\":\"hp1_destroy_fixture\",\"eye\":1,\"original_cleanup_once\":true,\"passed\":true}");
}
