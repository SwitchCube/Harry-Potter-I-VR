#pragma once
// The referee owns progress and the timing game. This panel only reads it;
// button presses still go through BroomHarry.PlayerInput/OnActionKeyPressed.
struct Hp1FlightUi {
 VrPanel panel;VrMenu overlay{"flight"};ULONGLONG next=0;bool visible=false;unsigned paints=0;
 std::wstring reportedPhase;
 void hide(){overlay.hide();visible=false;reportedPhase.clear();}
 void close(){hide();overlay.close();panel.close();next=0;paints=0;}
 void update(bool cinema){
  int kind=flightKind();void* hud=player?objectField(player,L"myHUD"):nullptr;
  bool live=startup.mode==3,ready=!live||(runtime.compositor&&runtime.compositor->CanRenderScene());
  if(kind<2||!hud||cinema||menuVisible||!ready||!flightInteractive()){hide();return;}
  auto now=GetTickCount64();void* ref=objectField(player,L"Referee");auto phase=state(ref);
  bool catching=phase==L"GameCatch",bumping=phase==L"GameBump";
  if(now>=next||!visible||phase!=reportedPhase){
   next=now+(catching?16:100);panel.open(768,192);panel.clear();
   const wchar_t* title=kind==2?tr(L"Erinnermich",L"Remembrall"):kind==3?L"Quidditch":tr(L"Fliegender Schl\u00fcssel",L"Flying key");
   panel.text(24,0,720,title,26,RGB(239,204,118));
   auto integer=[&](void* o,const wchar_t* key){return mem<int>(o,field(o,key,4,L"IntProperty"));};
   wchar_t line[160]={};float progress=0;
   if(catching){
    void* game=objectField(hud,L"QHUDGame");bool valid=game&&!flag(game,L"bDeleteMe");
    int pos=valid?integer(game,L"iTargetPos"):255,limit=valid?integer(game,L"iCatchPos"):0;
    bool canCatch=valid&&hp1::flightCatchReady(pos,limit);
    panel.text(24,42,720,canCatch?tr(L"JETZT X: zugreifen",L"ACTION NOW: catch"):tr(L"X dr\u00fccken, wenn die Marke im gr\u00fcnen Feld ist",L"Press Action when the marker is in the green zone"),canCatch?32:25,canCatch?RGB(133,245,164):RGB(240,239,229));
    panel.box(32,115,704,20,RGB(72,78,99));panel.box(32,115,int(704*hp1::flightCatchFraction(limit)),20,RGB(64,171,112));
    panel.box(32+int(696*hp1::flightCatchFraction(pos)),110,8,30,RGB(255,221,127));
    std::swprintf(line,160,tr(L"Versuche: %d   |   Umsehen bleibt frei",L"Attempts: %d   |   Look around freely"),integer(ref,L"CatchTriesLeft"));
   }else if(bumping){
    panel.text(24,42,720,tr(L"X: Draco jetzt anrempeln",L"ACTION: bump Draco now"),32,RGB(133,245,164));
    std::swprintf(line,160,tr(L"Der Besen folgt ihm f\u00fcr diesen letzten Sto\u00df automatisch.",L"The broom follows him automatically for this final bump."));
   }else{
    void* target=objectField(player,L"LookForTarget");std::wstring where;
    if(target&&!flag(target,L"bHidden")&&!flag(target,L"bDeleteMe")){
     auto delta=hpvr::sub(mem<Vec>(target,field(target,L"Location",12,L"StructProperty")),mem<Vec>(player,field(player,L"Location",12,L"StructProperty")));
     auto axes=hp1::gameAxes(mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty")));
     float front=hpvr::dot(delta,axes.forward),side=hpvr::dot(delta,axes.right);
     where=front<0?tr(L"hinter dir",L"behind you"):std::fabs(side)>std::fabs(front)*.35f?(side>0?tr(L"rechts",L"on your right"):tr(L"links",L"on your left")):tr(L"voraus",L"ahead");
     if(std::fabs(delta.z)>120)where+=delta.z>0?tr(L", h\u00f6her",L", higher"):tr(L", tiefer",L", lower");
    }
    if(kind==2){void* draco=objectField(ref,L"Draco");int hits=integer(draco,L"Bumps"),total=integer(draco,L"BumpsToWin");progress=total>0?float(hits)/total:0;
     std::swprintf(line,160,tr(L"Draco %ls   |   Treffer %d / %d",L"Draco %ls   |   Hits %d / %d"),where.c_str(),hits,total);
    }else{int current=integer(hud,L"CurrentNumberHoops"),total=integer(hud,L"MaxNumberHoops");progress=total>0?float(current)/total:0;
     std::swprintf(line,160,tr(L"%ls %ls   |   Fangfortschritt %d %%",L"%ls %ls   |   Catch progress %d %%"),kind==3?tr(L"Schnatz",L"Snitch"):tr(L"Schl\u00fcssel",L"Key"),where.c_str(),int(std::clamp(progress,0.0f,1.0f)*100));}
    panel.text(24,42,720,line,28);panel.box(32,115,704,12,RGB(72,78,99));panel.box(32,115,int(704*std::clamp(progress,0.0f,1.0f)),12,RGB(116,210,169));
    std::swprintf(line,160,tr(L"Trigger: schneller   |   A halten: langsam   |   X: Aktion",L"Cast: faster   |   Jump: slower   |   Action: catch"));
    if(kind==2&&flag(ref,L"bWrongWay"))std::swprintf(line,160,tr(L"Falsche Richtung: umdrehen und Draco folgen",L"Wrong way: turn around and follow Draco"));
    if(kind==3){void* rival=objectField(ref,L"Seeker");if(rival&&!flag(rival,L"bHidden")&&!flag(rival,L"bDeleteMe")){
     auto d=hpvr::sub(mem<Vec>(rival,field(rival,L"Location",12,L"StructProperty")),mem<Vec>(player,field(player,L"Location",12,L"StructProperty")));
     if(hpvr::dot(d,d)<360*360){auto axes=hp1::gameAxes(mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty")));std::swprintf(line,160,tr(L"Gegner dicht %ls neben dir!",L"An opponent is close %ls!"),hpvr::dot(d,axes.right)>0?tr(L"rechts",L"on your right"):tr(L"links",L"on your left"));}
    }}
   }
   panel.text(24,139,720,line,22);panel.upload(live?runtime.adapter:0,live?&bridge:nullptr);++paints;
   if(phase!=reportedPhase){log("{\"event\":\"hp1_flight_ui\",\"kind\":%d,\"phase\":\"%ls\",\"native_values\":true,\"gpu_verified\":%s}",kind,phase.c_str(),(panel.texture.verifiedMask&1)?"true":"false");reportedPhase=phase;}
  }
  visible=true;
  if(live){overlay.width=.90f;overlay.distance=1.4f;overlay.verticalOffset=-.32f;overlay.submit(runtime,panel.texture.textures[0],768,192,true,false);}
 }
};
static Hp1FlightUi flightUi;
static void saveFlightUiFixture(const wchar_t* file){
 if(!flightFixture()||!flightUi.panel.data)return;auto& p=flightUi.panel;GdiFlush();
 BITMAPFILEHEADER bf={};BITMAPINFOHEADER bi={};bf.bfType=0x4d42;bf.bfOffBits=sizeof(bf)+sizeof(bi);bf.bfSize=bf.bfOffBits+p.width*p.height*4;
 bi.biSize=sizeof(bi);bi.biWidth=p.width;bi.biHeight=-LONG(p.height);bi.biPlanes=1;bi.biBitCount=32;bi.biSizeImage=p.width*p.height*4;
 FILE* out=_wfopen((std::wstring(startup.profile)+L"\\"+file).c_str(),L"wb");need(out,"Flight UI capture");
 bool ok=fwrite(&bf,sizeof(bf),1,out)==1&&fwrite(&bi,sizeof(bi),1,out)==1&&fwrite(p.data,bi.biSizeImage,1,out)==1;fclose(out);need(ok,"Flight UI pixels");
}
