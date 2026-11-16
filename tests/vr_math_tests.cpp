#include "vr_math.h"
#include "vr_wand_pose.h"
#include "hp1_hand.h"
#include <cstdio>
#include <cstdlib>
using namespace hpvr;
void check(bool b,const char* m){if(!b){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
void near(float a,float b){check(std::fabs(a-b)<.0001f,"numerical reference");}
int main(){
 auto id=identity();auto r=rotation(id,0);check(!r.pitch&&!r.yaw&&!r.roll,"identity forward");
 r=rotation(id,pi/2);check(r.yaw==16384&&!r.pitch&&!r.roll,"world yaw quarter turn");
 Mat pitch={{{1,0,0,0},{0,0,-1,0},{0,1,0,0}}};r=rotation(pitch,0);check(r.pitch==16384,"look up");
 Mat roll={{{0,-1,0,0},{1,0,0,0},{0,0,1,0}}};r=rotation(roll,0);check(r.roll==-16384,"roll sign");
 Mat h=id;h.m[0][3]=2;h.m[1][3]=1.7f;h.m[2][3]=3;Origin o;o.recenter(h,0);
 Vec eye={100,200,140};auto p=o.position(h,eye,50);near(p.x,100);near(p.y,200);near(p.z,140);
 Mat left=id,right=id;left.m[0][3]=-.032f;right.m[0][3]=.032f;
 auto l=o.position(multiply(h,left),eye,50),q=o.position(multiply(h,right),eye,50);near(l.y,198.4f);near(q.y,201.6f);near(q.y-l.y,3.2f);
 h.m[2][3]-=.2f;p=o.position(h,eye,50);near(p.x,110);near(p.z,140);
 float t[2][4]={{-1.2f,.9f,-1.1f,1},{-.9f,1.2f,-1.1f,1}};auto v=projection(t,4.0f/3);near(v.tanV,1.1f);check(v.fov>100&&v.fov<120,"overscan FOV");
 for(int e=0;e<2;++e){auto b=v.eye[e];near((2*b.uMin-1)*v.tanH,t[e][0]);near((2*b.uMax-1)*v.tanH,t[e][1]);near((1-2*b.vMin)*v.tanV,t[e][3]);near((1-2*b.vMax)*v.tanV,t[e][2]);check(b.uMin>=0&&b.vMin>=0&&b.uMax<=1&&b.vMax<=1,"bounded crop");}
 // Asymmetric frustum reference: RH eye coordinates, -Z forward, clip +Y up.
 // This fixture catches the old vertical crop sign/endpoint error.
 float matrix[4][4]={{.952380952f,0,-.142857143f,0},{0,.952380952f,-.047619048f,0},{0,0,-1.001001f,-.1001001f},{0,0,-1,0}};
 near(projectionMatrixError(v,0,matrix),0);near(v.eye[0].vMin,1.0f/22);near(v.eye[0].vMax,1);
 auto bad=v;bad.eye[0].vMin=0;bad.eye[0].vMax=21.0f/22;check(projectionMatrixError(bad,0,matrix)>.04f,"reject mirrored vertical offset");
 matrix[0][2]=.142857143f;near(projectionMatrixError(v,1,matrix),0);
 bool caught=false;try{t[0][0]=1;projection(t,1);}catch(...){caught=true;}check(caught,"reject malformed runtime projection");
 // Calibration is local to the hand, preserves a 28 cm wand, and does not
 // change direction when only its grip origin is moved.
 auto grip=wandPose(identity(),Vec{.02f,-.04f,.03f},Vec{0,0,0});
 near(grip.m[0][3],.02f);near(grip.m[1][3],-.04f);near(grip.m[2][3],-.03f);
 auto direction=scale(axis(grip,2),-1);near(direction.x,0);near(direction.y,0);near(direction.z,-1);
 auto rotatedGrip=wandPose(roll,Vec{0,-.04f,0},Vec{0,0,0});
 near(rotatedGrip.m[0][3],.04f);near(rotatedGrip.m[1][3],0);
 auto aimed=wandPose(identity(),Vec{0,0,0},Vec{90,0,0});direction=scale(axis(aimed,2),-1);near(direction.y,1);near(direction.z,0);
 aimed=wandPose(identity(),Vec{0,0,0},Vec{0,90,0});direction=scale(axis(aimed,2),-1);near(direction.x,1);near(direction.z,0);
 aimed=wandPose(identity(),Vec{0,0,0},Vec{20,35,10});direction=scale(axis(aimed,2),-.28f);
 near(std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z),.28f);
 // Different aim/grip origins must still put the handle centre in the fist,
 // even with head-independent controller rotation and a calibrated angle.
 auto aim=identity(),fist=identity();aim.m[1][3]=.3f;fist.m[0][3]=.2f;fist.m[1][3]=-.15f;fist.m[2][3]=-.4f;
 for(float yawAngle:{0.0f,90.0f,-50.0f}){
  auto held=heldWandPose(fist,Vec{0,0,0},Vec{25,yawAngle,0},.28f);
  auto middle=add(translation(held),scale(axis(held,2),-.28f*.15f));
  near(middle.x,.2f);near(middle.y,-.15f);near(middle.z,-.4f);
 }
 auto tiltedFist=wandPose(fist,Vec{0,0,0},Vec{35,20,0});
 auto held=heldWandPose(tiltedFist,Vec{0,0,0},Vec{0,0,0},.28f);
 for(int row=0;row<3;++row)for(int col=0;col<3;++col)near(held.m[row][col],tiltedFist.m[row][col]);

 // A snap must not translate a head that has moved from the recentered origin.
 for(float headingAngle:{0.0f,1.0f,-2.7f})for(float snap:{pi/4,-pi/4,pi}){
  Mat head=identity(); Origin origin;origin.recenter(head,headingAngle);
  head.m[0][3]=.31f;head.m[1][3]=-.17f;head.m[2][3]=-.47f;
  Vec anchor={100,200,180},before=origin.position(head,anchor,64);
  turnAtHead(origin,head,snap);Vec after=origin.position(head,anchor,64);
  near(before.x,after.x);near(before.y,after.y);near(before.z,after.z);
 }
 // HP1's calibrated handle midpoint remains in the grip at arbitrary rotations.
 for(Vec a:{Vec{},Vec{70,30,-40},Vec{-60,-125,90}}){
  Mat hand=wandPose(identity(),{.2f,-.4f,.35f},a);
  Mat wand=hp1::wandPose(hand);
  Vec middle=add(translation(wand),scale(axis(wand,2),-hp1::wandLength*.15f));
  near(middle.x,hand.m[0][3]);near(middle.y,hand.m[1][3]);near(middle.z,hand.m[2][3]);
 }
 // A scene must start along the authored camera even if the wearer was looking
 // sideways/up, then preserve new head motion through camera pans and cuts.
 Axes camera={{0,1,0},{-1,0,0},{0,0,1}};
 Mat reference=wandPose(identity(),{.2f,1.7f,.4f},{25,70,-15});
 auto scene=scenePose(reference,reference,{100,200,300},camera,64);
 near(scene.position.x,100);near(scene.position.y,200);near(scene.position.z,300);
 check(std::abs(scene.rotation.yaw-16384)<=1&&std::abs(scene.rotation.pitch)<=1&&std::abs(scene.rotation.roll)<=1,"scene initial camera despite tilted head");
 Mat relative=wandPose(identity(),{.1f,0,0},{0,45,0});
 scene=scenePose(reference,multiply(reference,relative),{100,200,300},camera,64);
 near(scene.position.x,93.6f);near(scene.position.y,200);near(scene.position.z,300);
 check(std::abs(scene.rotation.yaw-24576)<=1,"head turn relative to scene camera");
 Axes cut={{1,0,0},{0,1,0},{0,0,1}};
 scene=scenePose(reference,multiply(reference,relative),{},cut,64);
 check(std::abs(scene.rotation.yaw-8192)<=1,"camera cut updates heading while preserving head turn");
 Axes elevated={{.8f,0,.6f},{0,1,0},{-.6f,0,.8f}};
 scene=scenePose(reference,reference,{},elevated,64);
 check(std::abs(scene.rotation.pitch-6712)<=1,"original camera pitch without double counting head pitch");
 auto leftScene=scenePose(reference,multiply(reference,left),{},elevated,64);
 auto rightScene=scenePose(reference,multiply(reference,right),{},elevated,64);
 near(rightScene.position.y-leftScene.position.y,.064f*64);
 near(leftScene.position.x,0);near(leftScene.position.z,0);
 std::puts("HP1 math: axes, stereo projection, head-centred turn, wand grip and cinematic relative poses passed");
}
