// Exercise the actual production hint and SteamVR-origin resolver against the
// deterministic private driver. Does not start HP1 or SteamVR.
#include "../src/hp1vr_native.cpp"
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(){
 startup.mode=6;tutorialTestDriver();
 for(const auto& line:tutorialLines)check(tutorialSpecific(line),"default action binding wording");
 tutorialRemapped=true;for(const auto& line:tutorialLines)check(!tutorialSpecific(line),"remapped binding must use neutral voice wording");
 tutorialRemapped=false;beginTutorial(tutorialLines[3],10);tutorialStarted=GetTickCount64()-2000;
 controls.jump=true;check(tutorialAlpha(true,true)>0&&!tutorialFade,"held button cannot dismiss new cue");
 controls.jump=false;tutorialAlpha(true,true);controls.jump=true;tutorialAlpha(true,false);check(!tutorialFade,"cinematic input cannot dismiss cue");
 tutorialAlpha(true,true);check(!tutorialFade,"held cinematic input is not a fresh playable press");
 controls.jump=false;tutorialAlpha(true,true);controls.jump=true;tutorialAlpha(true,true);check(tutorialFade!=0,"fresh success fades cue");
 tutorialFade=GetTickCount64()-551;check(tutorialAlpha(true,true)==0&&!tutorialLine,"success fade completes");
 controls={};beginTutorial(tutorialLines[3],10);check(tutorialAlpha(false,true)==0&&tutorialLine,"menu/focus hides without acting");
 tutorialEnds=GetTickCount64()-1;check(tutorialAlpha(true,true)==0&&!tutorialLine,"timeout cleanup");
 for(const auto& line:tutorialLines)if(tutorialLessonLine(line)){
  beginTutorial(line,10);unsigned serial=tutorialSerial;void* lesson=reinterpret_cast<void*>(1);
  tutorialLessonPhase(lesson,false);check(tutorialLine==&line,"first spoken instruction remains visible");
  for(int attempt=0;attempt<6;++attempt){tutorialLessonPhase(lesson,true);tutorialLessonPhase(lesson,false);check(!tutorialLine&&tutorialSerial==serial,"drawing/retry/next round must not rearm controller");}
  tutorialLessonClosed(lesson);check(!tutorialLessonOwner,"lesson teardown");
 }
 const auto& inventory=tutorialLines[15];controls={};beginTutorial(inventory,10);tutorialStarted=GetTickCount64()-2000;
 tutorialAlpha(true,true);controls.showStatus=true;tutorialAlpha(true,true);check(tutorialFade!=0,"inventory grip acknowledges cue");
 controls={};beginTutorial(inventory,10);tutorialStarted=GetTickCount64()-2000;tutorialModels.update(true,false);
 check(tutorialModels.devices.size()==1&&tutorialModels.devices[0].index==2&&tutorialModels.devices[0].highlights[0]=="grip","inventory highlights left lower grip");
 tutorialRemapped=true;beginTutorial(inventory,10);tutorialStarted=GetTickCount64()-2000;tutorialModels.update(true,false);
 check(tutorialModels.devices[0].index==1&&tutorialModels.devices[0].highlights[0]=="trigger","remapped inventory follows real origin");tutorialRemapped=false;
 for(unsigned index:{3u,10u,0u}){
  beginTutorial(tutorialLines[index],10);tutorialStarted=GetTickCount64()-2000;tutorialModels.update(true,false);
  check(tutorialModels.devices.size()==1,"one relevant controller");auto& d=tutorialModels.devices[0];
  check(d.index==(index==0?2:1),"actual tracked device index");check(d.highlights[0]==(index==3?"a":index==10?"trigger":"joystick"),"actual input component");
 }
 tutorialRemapped=true;beginTutorial(tutorialLines[3],10);tutorialStarted=GetTickCount64()-2000;tutorialModels.update(true,false);
 check(tutorialModels.devices[0].index==2&&tutorialModels.devices[0].highlights[0]=="grip","changed binding moves highlight to other controller grip");
 controls.skip=true;tutorialModels.update(true,false);check(!tutorialLine&&tutorialModels.alpha==0,"skip clears hint");controls.skip=false;
 // Rendering coordinates retain the exact hardware-local mesh origin under turns.
 for(Vec angle:{Vec{},Vec{15,70,-30},Vec{-25,-90,50}}){auto matrix=hpvr::wandPose(hpvr::identity(),{.2f,-.1f,.4f},angle);auto origin=tutorialPoint(matrix,{});check(std::fabs(origin.x-.2f)<1e-5&&std::fabs(origin.y+.1f)<1e-5&&std::fabs(origin.z+.4f)<1e-5,"device mesh origin not wand/grip offset");}
 tutorialModels.close();runtime.input=nullptr;runtime.compositor=nullptr;runtime.system=nullptr;
 std::puts("Tutorials: current/remapped bindings, physical component selection, held/cinematic input, success fade, timeout, skip and model origin passed");
}
