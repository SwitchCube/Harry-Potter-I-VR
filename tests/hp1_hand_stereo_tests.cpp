#include "hp1_hand.h"
#include "hp1_stereo_policy.h"
#include <cstdio>
#include <cstdlib>
using namespace hpvr;
static void check(bool value,const char* why){if(!value){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static void near(float a,float b){check(std::fabs(a-b)<.0001f,"numerical reference");}
int main(){
 // Confirmed HP2 calibration and exact fist centre across controller rotations.
 for(Vec angle:{Vec{},Vec{70,30,-40},Vec{-60,-125,90}}){
  auto hand=wandPose(identity(),{.2f,-.4f,.35f},angle),wand=hp1::wandPose(hand);
  auto centre=add(translation(wand),scale(axis(wand,2),-hp1::wandLength*.15f));
  for(auto p:{std::pair<float,float>{centre.x,hand.m[0][3]},{centre.y,hand.m[1][3]},{centre.z,hand.m[2][3]}})near(p.first,p.second);
  auto local=multiply(inverseRigid(hand),wand);near(local.m[1][2],.268288f);near(local.m[2][2],.963339f);
  near(std::sqrt(dot(scale(axis(wand,2),hp1::wandLength),scale(axis(wand,2),hp1::wandLength))),.28f);
 }
 Mat grip=identity(),head=identity();grip.m[0][3]=-.3f;grip.m[1][3]=1.1f;grip.m[2][3]=-.4f;head.m[1][3]=1.7f;
 auto a=hp1::statusPlane(grip,head);
 auto turned=wandPose(head,{},Vec{30,60,25}),b=hp1::statusPlane(grip,turned);
 for(int r=0;r<3;++r)near(a.m[r][3],b.m[r][3]);near(a.m[0][3],-.3f);near(a.m[1][3],1.2f);near(a.m[2][3],-.4f);
 auto normal=axis(a,2);check(dot(normal,sub(translation(head),translation(a)))>0,"panel faces wearer");
 for(auto pose:{a,hp1::statusPlane(head,head)}){near(dot(axis(pose,0),axis(pose,1)),0);for(int i=0;i<3;++i)near(dot(axis(pose,i),axis(pose,i)),1);}
 for(unsigned mask=0;mask<32;++mask)check(hp1::showStatus(mask&1,mask&2,mask&4,mask&8,mask&16)==(mask==7),"hold/release/tracking/menu/cinema gate");
 check(hp1::healthPercent(3,4)==75&&hp1::healthPercent(-1,4)==0&&hp1::healthPercent(8,4)==100&&hp1::healthPercent(1,0)==0,"HP1 health proportion");
 using E=hp1::StereoEvent;
 check(hp1::stereoEvent(L"Destroyed",false,false,false)==E::Reject,"unknown lifecycle remains guarded");
 check(hp1::stereoEvent(L"Destroyed",false,false,true)==E::Lifecycle,"native removal gets original cleanup");
 check(hp1::stereoEvent(L"Destroyed",true,false,true)==E::DuplicateDestruction,"duplicate destruction rejected");
 check(hp1::stereoEvent(L"Destroyed",false,true,false)==E::Lifecycle,"first render preparation nested cleanup");
 for(auto name:{L"Update",L"RenderOverlays",L"RenderTexture"}){check(hp1::stereoEvent(name,false,false,false)==E::Prepare,"right-only preparation");check(hp1::stereoEvent(name,true,false,false)==E::Skip,"no double render events");}
 check(hp1::stereoEvent(L"Tick",false,false,false)==E::Reject,"no extra simulation");
 std::puts("HP1: confirmed wand calibration, hand panel pose/visibility, health and scoped stereo lifecycle passed");
}
