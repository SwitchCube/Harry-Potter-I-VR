#pragma once
// Native checkpoint COPY only. Test placement/input cannot run in live VR.
static unsigned chessCapture=0,chessEyeSamples=0,chessPieceEyeSamples=0,chessEyeMask=0;
static float chessMaxEyeOffset=0;
static void chessTestEye(unsigned eye,Vec p,Rot r,Vec anchor,bool cinema){
 if(!chessTestMode||!chessActive())return;
 auto delta=hpvr::sub(p,anchor);float offset=std::sqrt(hpvr::dot(delta,delta));chessMaxEyeOffset=std::max(offset,chessMaxEyeOffset);
 need(!cinema&&offset<3,"Chess camera left Harry's eye position");
 ++chessEyeSamples;void* hud=objectField(player,L"myHUD");if(flag(hud,L"bCutSceneMode")){++chessPieceEyeSamples;chessEyeMask|=1u<<eye;}
}
struct ChessTestSquare{int x=-1,y=-1;bool alive=false;};
static ChessTestSquare chessTestSquare(void* b,Vec p){
 auto bp=mem<Vec>(b,field(b,L"Location",12,L"StructProperty"));
 float w=mem<float>(b,field(b,L"CollisionWidth",4,L"FloatProperty")),h=mem<float>(b,field(b,L"CollisionRadius",4,L"FloatProperty"));
 return {std::clamp(int((p.x-bp.x)/w*4+4),0,7),std::clamp(int((p.y-bp.y)/h*4+4),0,7),true};
}
static void chessTestPieces(void* b,ChessTestSquare (&pieces)[8]){
 auto off=field(b,L"ChessSet",16,L"StructProperty");auto prop=findProperty(mem<void*>(b,0x24),L"ChessSet");need(mem<DWORD>(prop,0x34)==8,"Chess set array differs");
 for(int i=0;i<8;++i){void* a=mem<void*>(b,off+i*16);pieces[i]=a?chessTestSquare(b,mem<Vec>(a,field(a,L"Location",12,L"StructProperty"))):ChessTestSquare{};}
}
static const int chessDx[]={1,1,0,-1,-1,-1,0,1},chessDy[]={0,1,1,1,0,-1,-1,-1};
static int chessTestSafeMove(void* b){
 ChessTestSquare pieces[8];chessTestPieces(b,pieces);auto hp=chessTestSquare(b,mem<Vec>(player,field(player,L"Location",12,L"StructProperty")));
 int best=-1,bestRemaining=9;
 for(int d=0;d<8;++d){int x=hp.x+chessDx[d],y=hp.y+chessDy[d];if(x<0||x>7||y<0||y>7)continue;
  ChessTestSquare copy[8];std::copy(pieces,pieces+8,copy);bool ok=true;
  for(auto a:copy)if(a.alive&&a.x==x&&a.y==y)ok=false;if(!ok)continue;
  int order[8]={0,1,2,3,4,5,6,7};
  auto rank=[&](int i){return std::make_pair(std::max(std::abs(copy[i].x-x),std::abs(copy[i].y-y)),std::min(std::abs(copy[i].x-x),std::abs(copy[i].y-y)));};
  std::sort(order,order+8,[&](int a,int c){if(copy[a].alive!=copy[c].alive)return copy[a].alive;if(rank(a)!=rank(c))return rank(a)<rank(c);return a>c;});
  for(int i:order)if(copy[i].alive){int nx=copy[i].x+(x>copy[i].x)-(x<copy[i].x),ny=copy[i].y+(y>copy[i].y)-(y<copy[i].y);int hit=-1;
   for(int j=0;j<8;++j)if(j!=i&&copy[j].alive&&copy[j].x==nx&&copy[j].y==ny)hit=j;
   if(hit>=0){copy[i].alive=copy[hit].alive=false;}else if(nx==x&&ny==y){ok=false;break;}else{copy[i].x=nx;copy[i].y=ny;}
  }
  int n=0;for(auto a:copy)if(a.alive)++n;if(ok&&n<bestRemaining){best=d;bestRemaining=n;}
 }
 need(best>=0,"Private chess fixture has no safe next move");return best;
}
static Vec chessTestPreview(void* board,int direction){
 struct {int direction;Vec result;} args={direction,{}};
 auto fn=scriptFunction(board,L"HarrysNewPosition");
 need(mem<DWORD>(findProperty(fn,L"iNewSquare"),0x48)==0&&mem<DWORD>(findProperty(fn,L"ReturnValue"),0x48)==4,"Chess preview parameters differ");
 callScript(board,L"HarrysNewPosition",&args);
 struct {Vec square,result;} loc={args.result,{}};fn=scriptFunction(board,L"GetPieceLocation");
 need(mem<DWORD>(findProperty(fn,L"PiecePosition"),0x48)==0&&mem<DWORD>(findProperty(fn,L"ReturnValue"),0x48)==12,"Chess location parameters differ");
 callScript(board,L"GetPieceLocation",&loc);return loc.result;
}
static void chessReplayTick(bool cinematic){
 if(!chessTestMode)return;
 static unsigned phase=0,preview=0,turns=0,commandsBefore=0;static ULONGLONG began=0;static int direction=0;static bool sawPieces=false,menuChecked=false,focusChecked=false;static void* board=nullptr;static Vec moveStart={};
 auto now=GetTickCount64();controls={};controls.active=true;controls.yaw=-16384;
 if(phase==99)return;
 static ULONGLONG traceAt=0;
 if(board&&now>=traceAt){
  traceAt=now+1000;auto pos=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));auto dest=mem<Vec>(player,field(player,L"ChessTargetLocation",12,L"StructProperty"));
  log("{\"event\":\"hp1_chess_test_state\",\"phase\":%u,\"player\":\"%ls\",\"board\":\"%ls\",\"ready\":%s,\"moving\":%s,\"turn\":%s,\"position\":[%.3f,%.3f,%.3f],\"target\":[%.3f,%.3f,%.3f]}",phase,state(player).c_str(),state(board).c_str(),chessReady()?"true":"false",flag(player,L"bChessMoving")?"true":"false",flag(player,L"bHarrysMove")?"true":"false",pos.x,pos.y,pos.z,dest.x,dest.y,dest.z);
 }
 auto stick=[&](int d){float angle=(d*8192-controls.yaw)*hpvr::pi/32768;controls.forward=std::cos(angle);controls.side=std::sin(angle);};
 if(phase==0){
  board=mechanicsFind(L"ChessBoard0");need(board,"Native chess board missing");
  auto p=mem<Vec>(player,field(player,L"Location",12,L"StructProperty")),bp=mem<Vec>(board,field(board,L"Location",12,L"StructProperty"));
  log("{\"event\":\"hp1_chess_inventory\",\"player_state\":\"%ls\",\"board_state\":\"%ls\",\"player\":[%.3f,%.3f,%.3f],\"board\":[%.3f,%.3f,%.3f]}",state(player).c_str(),state(board).c_str(),p.x,p.y,p.z,bp.x,bp.y,bp.z);
  if(!chessActive()){
   p.x=bp.x+mem<float>(board,field(board,L"CollisionWidth",4,L"FloatProperty"))*.125f;p.y=bp.y+mem<float>(board,field(board,L"CollisionRadius",4,L"FloatProperty"))*.875f;
   // The checkpoint stands on a lower approach floor. Place the copy above
   // the board, then let native falling/touch enter the original puzzle.
   p.z=bp.z+mem<float>(player,field(player,L"CollisionHeight",4,L"FloatProperty"))+12;
   need(mechanicsPosition(p,-16384),"Private chess approach placement blocked");
  }
  phase=1;began=now;return;
 }
 need(_wcsicmp(state(player).c_str(),L"stateDead")&&_wcsicmp(state(player).c_str(),L"ChessDeath"),"Private chess route reached a native losing square");
 if(phase==1)need(now-began<45000,"Original board did not start");
 if(cinematic){controls.skip=true;return;}if(skipActive)return;
 if(phase==1){
  if(!chessReady())return;
  ChessTestSquare pieces[8];chessTestPieces(board,pieces);for(int i=0;i<8;++i)log("{\"event\":\"hp1_chess_test_piece\",\"index\":%d,\"square\":[%d,%d],\"alive\":%s}",i,pieces[i].x,pieces[i].y,pieces[i].alive?"true":"false");
  phase=2;began=now;chessCapture=1;return;
 }
 if(phase==2){
  stick(preview);if(now-began<220)return;
  need(chessCommands==0&&chessSelection==int(preview),"Chess stick committed or chose wrong direction");
  auto square=objectField(board,L"SelectSquare");need(square,"Native chess highlight absent");auto expected=chessTestPreview(board,preview),actual=mem<Vec>(square,field(square,L"Location",12,L"StructProperty"));auto diff=hpvr::sub(expected,actual);
  need(std::sqrt(hpvr::dot(diff,diff))<.1f,"Native highlight differs from controller selection");
  log("{\"event\":\"hp1_chess_test_preview\",\"direction\":%u,\"position\":[%.3f,%.3f,%.3f],\"passed\":true}",preview,actual.x,actual.y,actual.z);
  if(++preview==8)phase=11;began=now;return;
 }
 if(phase==11){
  // Neutral stick must keep the selected square while freely looking/turning.
  controls.yaw=16384;if(now-began<350)return;
  need(chessSelection==7&&chessCommands==0,"Looking around changed the selected square");
  log("{\"event\":\"hp1_chess_test_neutral_look\",\"passed\":true}");
  // Simultaneous menu + trigger must open the menu without submitting a turn.
  controls.menu=true;controls.cast=true;phase=3;began=now;return;
 }
 if(phase==3){controls.cast=true;if(now-began<350)return;need(menuVisible&&chessCommands==0,"Chess menu input leaked");inputKey(27);phase=4;began=now;return;}
 if(phase==4){controls.cast=true;if(now-began<350)return;need(!menuVisible&&chessCommands==0,"Held menu trigger submitted chess move");menuChecked=true;phase=5;began=now;return;}
 if(phase==5){controls.active=false;controls.cast=true;stick(4);if(now-began<300)return;need(chessCommands==0,"Inactive controls submitted chess move");phase=6;began=now;return;}
 if(phase==6){controls.cast=true;if(now-began<300)return;need(chessCommands==0,"Input regain submitted held chess move");focusChecked=true;phase=7;began=now;return;}
 if(phase==7){
  if(now-began<250)return;
  direction=chessTestSafeMove(board);commandsBefore=chessCommands;moveStart=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));phase=8;began=now;return;
 }
 if(phase==8){stick(direction);if(now-began>=300){controls.cast=true;phase=9;began=now;}return;}
 if(phase==9){
  controls.cast=true;need(now-began<55000,"Original chess turn failed to finish");
  need(chessCommands<=commandsBefore+1,"Held trigger repeated chess move");
  if(!_wcsicmp(state(board).c_str(),L"ChessPiecesMove")){
   if(!sawPieces){if(turns==0)chessCapture=2;sawPieces=true;log("{\"event\":\"hp1_chess_test_native_turn\",\"cutscene_flag\":%s}",flag(objectField(player,L"myHUD"),L"bCutSceneMode")?"true":"false");}return;
  }
  if(sawPieces&&chessReady()){
   auto p=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));auto delta=hpvr::sub(p,moveStart);need(std::hypot(delta.x,delta.y)>100,"Native chess move did not change square");
   ++turns;log("{\"event\":\"hp1_chess_test_turn\",\"turn\":%u,\"direction\":%d,\"native_move\":true,\"passed\":true}",turns,direction);phase=10;began=now;
  }return;
 }
 if(phase==10){
  controls.cast=true;if(now-began<350)return;need(chessCommands==commandsBefore+1,"Held trigger carried into next turn");
  if(turns<3){sawPieces=false;phase=7;began=now;return;}
  need(chessPieceEyeSamples>20&&chessEyeMask==3&&menuChecked&&focusChecked,"Chess eye/input regression incomplete");
  log("{\"event\":\"hp1_chess_test_complete\",\"previews\":8,\"native_turns\":%u,\"menu_and_focus\":true,\"held_trigger_blocked\":true,\"eye_samples\":%u,\"piece_eye_samples\":%u,\"eye_mask\":%u,\"max_eye_anchor_distance\":%.5f,\"passed\":true}",turns,chessEyeSamples,chessPieceEyeSamples,chessEyeMask,chessMaxEyeOffset);phase=99;
 }
}
