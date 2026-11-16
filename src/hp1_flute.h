#pragma once
// HarryL502 owns the native Flute only after the Fluffy introduction. Do not
// infer availability from inventory bHasFlute or replace ordinary spell input.
static bool fluteHeld=false,fluteWasAvailable=false,fluteArmed=false;
static unsigned fluteMeshMask=0,fluteStarts=0,fluteStops=0;
static void* ownedFlute(){
 if(!player||name(mem<void*>(player,0x24))!=L"HarryL502")return nullptr;
 void* flute=objectField(player,L"Flute"),*weapon=objectField(player,L"Weapon");
 return flute&&weapon&&name(mem<void*>(flute,0x24))==L"Flute"&&!flag(flute,L"bDeleteMe")&&objectField(flute,L"Owner")==player&&flag(weapon,L"bHidden")?flute:nullptr;
}
static bool flutePlaying(){return player&&!_wcsicmp(state(player).c_str(),L"FlutePlay");}
static bool fluteInputAllowed(){
 void* hud=player?objectField(player,L"myHUD"):nullptr;
 auto s=player?state(player):L"None";
 return controls.active&&controls.hand&&!menuVisible&&!letterBusy&&hud&&!flag(hud,L"bCutSceneMode")&&(s==L"PlayerWalking"||!_wcsicmp(s.c_str(),L"FlutePlay"));
}
static void stopFlute(){
 if(!flutePlaying())return;
 // Native state transition calls HarryL502.FlutePlay.EndState, which stops
 // the sound/notes and notifies all heads. Never write state/timers ourselves.
 auto core=GetModuleHandleW(L"Core.dll");using Make=void(__thiscall*)(void*,const wchar_t*,int);using Go=int(__thiscall*)(void*,DWORD);
 auto make=reinterpret_cast<Make>(GetProcAddress(core,"??0FName@@QAE@PBGW4EFindName@@@Z"));
 auto go=reinterpret_cast<Go>(GetProcAddress(core,"?GotoState@UObject@@UAE?AW4EGotoState@@VFName@@@Z"));
 need(make&&go,"Missing HP1 flute state API");DWORD token=0;make(&token,L"PlayerWalking",0);need(token,"Missing HP1 walking state");
 Fpu fpu;fpu.restore();go(player,token);++fluteStops;
 log("{\"event\":\"hp1_flute_stop\",\"native_end_state\":true,\"state\":\"%ls\"}",state(player).c_str());fpu.restore();
}
static bool fluteButtons(){
 if(!vrMovement())return false;
 bool available=ownedFlute()!=nullptr;
 if(!available){
  if(fluteWasAvailable&&player){mem<BYTE>(player,field(player,L"bAltFire",1,L"ByteProperty"))=0;stopFlute();}
  fluteHeld=fluteArmed=fluteWasAvailable=false;return false;
 }
 bool enabled=fluteInputAllowed();
 if(!fluteWasAvailable){fluteWasAvailable=true;fluteArmed=false;log("{\"event\":\"hp1_flute_available\",\"original_owned_flute\":true,\"wand_replaced\":true}");}
 if(!enabled){fluteArmed=false;}else if(!controls.cast)fluteArmed=true;
 bool held=enabled&&fluteArmed&&controls.cast;
 if(fluteHeld&&!enabled)log("{\"event\":\"hp1_flute_input_stopped\",\"state\":\"%ls\",\"hand_valid\":%s,\"menu\":%s}",state(player).c_str(),controls.hand?"true":"false",menuVisible?"true":"false");
 mem<BYTE>(player,field(player,L"bAltFire",1,L"ByteProperty"))=held?1:0;
 if(!held&&flutePlaying())stopFlute();
 if(held&&!fluteHeld&&state(player)==L"PlayerWalking"){
  float value=1;callScript(player,L"AltFire",&value);++fluteStarts;
  log("{\"event\":\"hp1_flute_start\",\"native_altfire\":true,\"state\":\"%ls\"}",state(player).c_str());
 }
 fluteHeld=held;return true;
}
static void setFlutePose(void* actor,Vec position,Rot rotation){
 for(auto key:{L"SetLocation",L"SetRotation"}){
  void* fn=scriptFunction(actor,key);const wchar_t* param=key[3]==L'L'?L"NewLocation":L"NewRotation";
  void* p=findProperty(fn,param),*ret=findProperty(fn,L"ReturnValue");
  need(p&&ret&&mem<DWORD>(p,0x48)==0&&mem<DWORD>(p,0x38)==12&&mem<DWORD>(ret,0x48)==12&&mem<DWORD>(ret,0x38)==4,"HP1 flute pose parameters differ");
  if(key[3]==L'L'){struct{Vec p;DWORD ok;} args={position,0};callScript(actor,key,&args);}
  else{struct{Rot r;DWORD ok;} args={rotation,0};callScript(actor,key,&args);}
 }
}
static bool fluteTracked(){void* hud=player?objectField(player,L"myHUD"):nullptr;return controls.hand&&hud&&!flag(hud,L"bCutSceneMode");}
static void syncFlutePose(){
 void* flute=ownedFlute();if(!flute||!fluteTracked())return;
 Fpu fpu;fpu.restore();setFlutePose(flute,controls.wandBase,controls.wandRotation);fpu.restore();
}
static bool fluteInternal(void* self,void* frame,void* result){
 if(!vrMovement()||!frame||!player)return false;
 auto fn=name(mem<void*>(frame,4));
 if(self!=player&&(fn!=L"Tick"||name(mem<void*>(self,0x24))!=L"Flute"))return false;
 void* flute=ownedFlute();if(!flute)return false;
 if(self==player&&(fn==L"PlayerTick"||fn==L"PlayerInput")){
  Fpu fpu;bool held=fluteHeld&&controls.cast&&fluteInputAllowed();
  mem<BYTE>(player,field(player,L"bAltFire",1,L"ByteProperty"))=held?1:0;
  fpu.restore();originalInternal(self,frame,result);
  if(fn==L"PlayerInput")mem<BYTE>(player,field(player,L"bAltFire",1,L"ByteProperty"))=held?1:0;
  fpu.restore();return true;
 }
 if(self==flute&&fn==L"Tick"&&fluteTracked()){
  Fpu fpu;RestoreFields restore;
  restore.set(player,field(player,L"weaponLoc",12,L"StructProperty"),controls.wandBase);
  restore.set(player,field(player,L"weaponRot",12,L"StructProperty"),controls.wandRotation);
  fpu.restore();originalInternal(self,frame,result);fpu.restore();return true;
 }
 return false;
}
