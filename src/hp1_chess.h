#pragma once
// HP1 ChessMode uses forward as a single-square command, not locomotion.
// Feed that original command; ChessBoard still validates, moves and resolves it.
static int chessSelection=0;
static bool chessSelected=false,chessArmed=false,chessPending=false,chessWasReady=false;
static unsigned chessCommands=0,chessSelections=0;
static void* chessBoard(){
 if(!player)return nullptr;
 auto s=state(player);if(_wcsicmp(s.c_str(),L"ChessMode")&&_wcsicmp(s.c_str(),L"ChessDeath"))return nullptr;
 void* level=mem<void*>(player,0x80);void** actors=mem<void**>(level,0x2c);int n=mem<int>(level,0x30);
 need(actors&&n>0&&n<100000,"HP1 chess actor array differs");
 for(int i=0;i<n;++i){auto a=actors[i];if(a&&name(mem<void*>(a,0x24))==L"ChessBoard"&&!flag(a,L"bDeleteMe")&&objectField(a,L"Player")==player)return a;}
 return nullptr;
}
static bool chessActive(){return chessBoard()!=nullptr;}
static bool chessReady(){
 void* board=chessBoard();return board&&!_wcsicmp(state(board).c_str(),L"HarrysMove")&&!_wcsicmp(state(player).c_str(),L"ChessMode")&&flag(player,L"bHarrysMove")&&!flag(player,L"bChessMoving");
}
static void resetChess(){chessSelected=chessArmed=chessPending=chessWasReady=false;}
static void chessFacing(){
 Rot facing={0,chessSelection*8192,0};
 for(auto key:{L"Rotation",L"ViewRotation",L"DesiredRotation"})mem<Rot>(player,field(player,key,12,L"StructProperty"))=facing;
}
static bool chessButtons(){
 if(!vrMovement()||!chessActive()){resetChess();return false;}
 if(!chessSelected){
  int yaw=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty")).yaw;
  chessSelection=((yaw+4096)&65535)/8192;chessSelected=true;
  log("{\"event\":\"hp1_chess_controls\",\"eight_directions\":true,\"stick_select_trigger_confirm\":true,\"native_rules\":true}");
 }
 bool ready=chessReady()&&controls.active&&!controls.menu&&!menuVisible&&!letterBusy;
 if(!ready||!chessWasReady){chessArmed=chessPending=false;}
 if(ready){
  float strength=std::hypot(controls.forward,controls.side);
  if(strength>.35f){
   float angle=std::atan2(controls.side,controls.forward)+controls.yaw*hpvr::pi/32768;
   int yaw=int(std::lround(angle*32768/hpvr::pi));
   int candidate=((yaw+4096)&65535)/8192;
   // A small angular hysteresis prevents a borderline diagonal flickering.
   if(candidate!=chessSelection&&std::abs(int(short(yaw-chessSelection*8192)))>4700){
    chessSelection=candidate;++chessSelections;
    log("{\"event\":\"hp1_chess_selection\",\"direction\":%d}",chessSelection);
   }
  }
  chessFacing();
  if(!controls.cast)chessArmed=true;
  else if(chessArmed){chessPending=true;chessArmed=false;}
 }
 chessWasReady=ready;return true;
}
static bool chessInternal(void* self,void* frame,void* result){
 if(!vrMovement()||!player||!frame||self!=player||name(mem<void*>(frame,4))!=L"PlayerMove"||_wcsicmp(state(player).c_str(),L"ChessMode"))return false;
 if(!chessActive())return false;
 Fpu fpu;
 bool enabled=chessReady()&&controls.active&&!controls.menu&&!menuVisible&&!letterBusy;
 bool commit=enabled&&chessSelected&&chessPending;chessPending=false;
 // Never pass continuous stick motion, desktop mouse rotation or stale fire.
 for(auto key:{L"aForward",L"aSideMove",L"aStrafe",L"aTurn",L"aLookUp"})mem<float>(player,field(player,key,4,L"FloatProperty"))=0;
 mem<BYTE>(player,field(player,L"bAltFire",1,L"ByteProperty"))=0;
 if(enabled&&chessSelected)chessFacing();
 if(commit){mem<float>(player,field(player,L"aForward",4,L"FloatProperty"))=1;++chessCommands;}
 fpu.restore();originalInternal(self,frame,result);
 mem<float>(player,field(player,L"aForward",4,L"FloatProperty"))=0;
 if(commit)log("{\"event\":\"hp1_chess_confirm\",\"direction\":%d,\"native_move_requested\":%s}",chessSelection,flag(player,L"bChessMoving")?"true":"false");
 fpu.restore();return true;
}
