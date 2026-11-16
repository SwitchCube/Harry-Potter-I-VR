#pragma once
#include "vr_menu.h"
#include "vr_panel.h"
struct Hp1StatusValues {
 int health=0,beans=0,stars=0,cards=0,seeds=0,points[4]={};
 bool flight=false,levitation=false,flute=false,chess=false,chessTurn=false;int seconds=0,rings=0,maxRings=0,flightType=0;
 bool operator==(const Hp1StatusValues& b)const{return health==b.health&&beans==b.beans&&stars==b.stars&&cards==b.cards&&seeds==b.seeds&&std::equal(points,points+4,b.points)&&flight==b.flight&&levitation==b.levitation&&chess==b.chess&&chessTurn==b.chessTurn&&flute==b.flute&&seconds==b.seconds&&rings==b.rings&&maxRings==b.maxRings&&flightType==b.flightType;}
};
static Hp1StatusValues readHp1Status(void* actor){
 Hp1StatusValues s;
 s.levitation=actor==player&&levitating();
 s.flute=actor==player&&ownedFlute();
 s.chess=actor==player&&chessActive();s.chessTurn=s.chess&&chessReady();
 s.health=hp1::healthPercent(mem<float>(actor,field(actor,L"lifePotions",4,L"FloatProperty")),mem<float>(actor,field(actor,L"MaxLifePotions",4,L"FloatProperty")));
 auto count=[&](const wchar_t* key){return mem<int>(actor,field(actor,key,4,L"IntProperty"));};
 s.beans=count(L"numBeans");s.stars=count(L"numStars");s.seeds=count(L"iFireSeedCount");
 const wchar_t* house[]={L"numHousePointsGryffindor",L"numHousePointsSlytherin",L"numHousePointsRavenclaw",L"numHousePointsHufflepuff"};
 for(unsigned i=0;i<4;++i)s.points[i]=count(house[i]);
 // Let HP1 count its own travel WizardCards array; no HP2 inventory layout.
 void* fn=scriptFunction(actor,L"getNumCards"),*ret=findProperty(fn,L"ReturnValue");
 need(ret&&name(mem<void*>(ret,0x24))==L"IntProperty"&&mem<DWORD>(ret,0x38)==4&&mem<DWORD>(ret,0x48)==0,"HP1 card-count return layout differs");
 callScript(actor,L"getNumCards",&s.cards);need(s.cards>=0&&s.cards<=25,"Invalid HP1 card count");
 if(actor==player&&flightKind()){void* hud=objectField(actor,L"myHUD");if(hud){s.flight=true;s.flightType=flightKind();s.seconds=int(std::ceil(std::max(0.0f,mem<float>(hud,field(hud,L"fCountdownTime",4,L"FloatProperty")))));s.rings=mem<int>(hud,field(hud,L"CurrentNumberHoops",4,L"IntProperty"));s.maxRings=mem<int>(hud,field(hud,L"MaxNumberHoops",4,L"IntProperty"));}}
 return s;
}
struct Hp1Status {
 VrPanel panel;VrMenu overlay{"status"};Hp1StatusValues last={};
 bool painted=false,reported=false;ULONGLONG next=0;
 void hide(){if(overlay.visible)log("{\"event\":\"hp1_hand_status\",\"visible\":false}");overlay.hide();}
 void close(){hide();overlay.close();panel.close();painted=reported=false;next=0;}
 void paint(const Hp1StatusValues& s){
  panel.open(640,s.flight?744:(s.levitation||s.flute||s.chess)?650:568);panel.clear();
  panel.text(28,12,580,L"Status",38,RGB(239,204,118));
  wchar_t buffer[96];std::swprintf(buffer,96,tr(L"Energie  %d %%",L"Energy  %d %%"),s.health);panel.text(28,68,580,buffer,32);
  panel.box(28,128,584,14,RGB(50,58,72));panel.box(28,128,584*s.health/100,14,RGB(109,211,146));
  auto item=[&](int x,int y,const wchar_t* title,int value,int max=0){panel.text(x,y,264,title,24,RGB(182,189,205));if(max)std::swprintf(buffer,96,L"%d / %d",value,max);else std::swprintf(buffer,96,L"%d",value);panel.text(x,y+37,264,buffer,34);};
  item(28,155,tr(L"Bohnen",L"Beans"),s.beans);item(340,155,tr(L"Sterne",L"Stars"),s.stars);
  item(28,245,tr(L"Zaubererkarten",L"Wizard cards"),s.cards,25);item(340,245,tr(L"Feuersamen",L"Fire seeds"),s.seeds);
  panel.box(28,344,584,2,RGB(100,88,63));
  if(s.flight){
   const wchar_t* title=s.flightType==1?tr(L"Flugstunde",L"Flying lesson"):s.flightType==2?tr(L"Erinnermich",L"Remembrall"):s.flightType==3?L"Quidditch":tr(L"Fliegender Schl\u00fcssel",L"Flying key");
   panel.text(28,351,584,title,27,RGB(239,204,118));
   if(s.flightType==1){item(28,405,tr(L"Zeit (Sekunden)",L"Time (seconds)"),s.seconds);item(340,405,tr(L"Ringe",L"Rings"),s.rings,s.maxRings);}
   else{panel.text(28,402,584,s.flightType==2?tr(L"Draco ber\u00fchren; letzter Sto\u00df mit X",L"Touch Draco; use the action button for the final bump"):tr(L"Ziel verfolgen; mit X im gr\u00fcnen Feld fangen",L"Follow the target; catch in the green zone"),24);}
   panel.text(28,500,584,tr(L"Linker Stick: lenken / steigen / sinken",L"Movement stick: steer / climb / descend"),25);
   panel.text(28,545,584,tr(L"Stick loslassen: wieder waagerecht",L"Release the stick: return to level flight"),25);
   panel.text(28,590,584,tr(L"Trigger: schneller   |   A halten: langsam",L"Cast: accelerate   |   Jump: fly slowly"),25);
   panel.text(28,635,584,tr(L"Flugmarkierung zeigt die Flugrichtung",L"The flight marker shows your heading"),25);
   panel.text(28,685,584,tr(L"X: Szene abkuerzen   |   B: Menue",L"Action: skip scene   |   Menu: open menu"),25);
  }else{panel.text(28,351,584,tr(L"Hauspunkte",L"House points"),27,RGB(239,204,118));
  const wchar_t* names[]={L"Gryffindor",L"Slytherin",L"Ravenclaw",L"Hufflepuff"};
  for(unsigned i=0;i<4;++i){int x=i%2?340:28,y=408+int(i/2)*54;std::swprintf(buffer,96,L"%ls  %d",names[i],s.points[i]);panel.text(x,y,280,buffer,26);}
  if(s.levitation){panel.text(28,520,584,tr(L"Schweben: linker Stick bewegt das Objekt",L"Levitation: movement stick moves the object"),24,RGB(239,204,118));panel.text(28,562,584,tr(L"Richtung folgt dem Blick; loslassen: Halt",L"Direction follows your view; release to stop"),24);panel.text(28,604,584,tr(L"Rechter Trigger: Objekt absetzen",L"Cast button: release the object"),24);}
  if(s.chess){panel.text(28,520,584,s.chessTurn?tr(L"Schach: linker Stick w\u00e4hlt das Feld",L"Chess: movement stick selects a square"):tr(L"Schach: Figuren ziehen - bitte warten",L"Chess: pieces are moving - please wait"),24,RGB(239,204,118));panel.text(28,562,584,tr(L"Rechter Trigger: ausgew\u00e4hlten Zug best\u00e4tigen",L"Cast button: confirm your move"),23);panel.text(28,604,584,tr(L"Stick loslassen: Auswahl bleibt beim Umsehen",L"Release the stick: look around without moving selection"),22);}
  if(s.flute){panel.text(28,520,584,tr(L"Fl\u00f6te: rechten Trigger halten zum Spielen",L"Flute: hold the cast button to play"),24,RGB(239,204,118));panel.text(28,562,584,tr(L"Vor den jeweiligen Kopf stellen",L"Stand in front of the head you want to soothe"),24);panel.text(28,604,584,tr(L"Loslassen: stoppen und wieder laufen",L"Release: stop playing and walk again"),24);}
  }
  last=s;painted=true;
 }
 void update(bool cinema){
  bool ready=runtime.compositor&&runtime.compositor->CanRenderScene();
  if(!player||!hp1::showStatus(controls.showStatus,controls.leftValid,ready,menuVisible,cinema)){hide();return;}
  bool newTexture=false;
  if(!painted||GetTickCount64()>=next){
   next=GetTickCount64()+100;auto values=readHp1Status(player);
   if(!painted||!(last==values)){paint(values);panel.upload(runtime.adapter,&bridge);newTexture=true;}
  }
  hpvr::Mat head;std::memcpy(&head,&runtime.poses[0].mDeviceToAbsoluteTracking,sizeof(head));auto plane=hp1::statusPlane(controls.leftGrip,head);
  bool wasVisible=overlay.visible;overlay.width=.34f;overlay.submit(runtime,panel.texture.textures[0],panel.width,panel.height,false,false,&plane);
  if(!wasVisible||newTexture)log("{\"event\":\"hp1_hand_status\",\"visible\":true,\"health_percent\":%d,\"beans\":%d,\"stars\":%d,\"cards\":%d,\"seeds\":%d,\"tracked_left_grip\":true}",last.health,last.beans,last.stars,last.cards,last.seeds);
 }
};
static Hp1Status handStatus;
