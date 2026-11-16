#include "hp1_flight_math.h"
#include <cstdio>
#include <cstdlib>
using namespace hpvr;
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(){
 hp1::FlightActionLatch action;
 check(!action.update(false,true)&&!action.update(true,true),"held scene-skip cannot spend a new catch attempt");
 check(!action.update(true,false)&&action.update(true,true),"fresh X reaches original flight action");
 check(!action.update(false,true)&&!action.update(true,true),"menu/focus loss requires release before another action");
 check(hp1::flightCatchReady(30,31)&&!hp1::flightCatchReady(31,31),"catch indicator follows exact native strict threshold");
 for(float dt:{1.0f/30,1.0f/72,1.0f/144}){
  hp1::FlightViewHeading view;view.reset(65500);int yaw=view.update(30,dt);check(std::abs(hp1::signedAngle(yaw-65500))<100,"heading wrap uses short direction");
  int previous=yaw;for(int i=0;i<50;++i){yaw=view.update(25000,dt);check(std::abs(hp1::signedAngle(yaw-previous))<=int(hp1::flightYawRate*dt)+1,"automatic pursuit camera has bounded turn speed");previous=yaw;}
 }
 for(float dt:{1.0f/30,1.0f/72,1.0f/90,1.0f/144}){
  hp1::FlightMotion pitch;int p=0;for(int n=0;n<int(2/dt);++n)p=pitch.pitchStep(.5f,p,45,35,false,dt);
  check(std::fabs(p-4096)<2,"stable analog climb angle at different frame rates");
  for(int n=0;n<int(1/dt);++n){int previous=p;p=pitch.pitchStep(0,p,45,35,false,dt);check(p>=0&&p<=previous,"neutral recovery cannot overshoot level");}
  check(std::fabs(p)<2,"neutral stick levels broom");
  pitch.reset();check(pitch.pitchStep(.5f,0,45,35,true,dt)<0,"original invert setting supported");
  pitch.reset();check(pitch.pitchStep(-.5f,65535,45,35,false,dt)<0,"wrapped negative angle descends");
  for(int n=0;n<int(4/dt);++n)p=pitch.pitchStep(-1,p,45,35,false,dt);
  check(p>=-int(35*32768/180)&&p< -6300,"downward limit retained");
 }
 check(hp1::flightYaw(.25f)>.0f&&hp1::flightYaw(.25f)<.25f&&hp1::flightYaw(-.8f)<0,"progressive proportional yaw");
 check(hp1::flightYaw(NAN)==0&&hp1::flightStick(NAN)==0,"invalid input neutral");
 check(hp1::flightStick(.1f)==0&&hp1::flightStick(.15f)>0,"fine input outside small flight dead zone");
 check(hp1::flightYaw(hp1::flightStick(.3f))>.12f,"small stick corrections are no longer squared away");
 Mat head=identity();auto axes=hp1::flightAxes({2500,14000,12000});auto pose=scenePose(head,head,{100,200,300},axes,64);
 check(std::abs(pose.rotation.roll)<=1&&std::abs(pose.rotation.yaw-14000)<=1&&std::abs(pose.rotation.pitch)<=1,"level horizon despite broom pitch and bank");
 Mat turned=head;float a=.7f;turned.m[0][0]=turned.m[2][2]=std::cos(a);turned.m[0][2]=-std::sin(a);turned.m[2][0]=std::sin(a);
 auto look=scenePose(head,turned,{100,200,300},axes,64);check(std::abs(look.rotation.yaw-pose.rotation.yaw)>5000,"free head look independent of flight steering");
 // Pitching the real head remains direct even if entry/recentre was tilted.
 Mat tilted=identity();float tilt=.3f;tilted.m[1][1]=tilted.m[2][2]=std::cos(tilt);tilted.m[1][2]=-std::sin(tilt);tilted.m[2][1]=std::sin(tilt);
 auto real=scenePose(hp1::flightHeadReference(tilted),tilted,{0,0,0},hp1::flightAxes({12000,0,6000}),64);
 check(std::abs(real.rotation.pitch-int(tilt*32768/pi))<=1,"real head pitch survives entry without artificial pitch");
 int distances[4]={};int rateIndex=0;
 for(float dt:{1.0f/30,1.0f/72,1.0f/90,1.0f/144}){
  hp1::FlightMotion motion;motion.reset(200);int angle=0;float actual=200;
  int hz=int(std::round(1/dt));
  for(int n=0;n<hz*3;++n){float old=motion.yawRate;int step=motion.yawStep(n<hz?8192:-8192,dt);angle+=step;
   check(std::fabs(motion.yawRate)<=8192.01f,"bounded turning speed");check(std::fabs(motion.yawRate-old)<=hp1::flightTurnBraking*dt+.01f,"bounded acceleration through reversal");
   float speed=motion.speedStep(n<hz?800:200,actual,dt);check(speed-actual<=220*dt+.01f&&actual-speed<=400*dt+.01f,"boost and brake cannot jump");actual=speed;
  }
  distances[rateIndex++]=angle;
  int coast=0;for(int n=0;n<hz;++n)coast+=motion.yawStep(0,dt);
  check(motion.yawRate==0&&std::abs(coast)<730,"release stops with less than four degrees of drift");
  for(int n=0;n<hz;++n)motion.yawStep(8192,dt);
  float reversed=0;while(motion.yawRate>0&&reversed<1){motion.yawStep(-8192,dt);reversed+=dt;}
  check(reversed<.2f,"opposite input takes effect within 200 ms");
  float collision=motion.speedStep(800,0,dt);check(collision<=220*dt+.01f,"soft restart after physical wall collision");
  float oldRate=motion.yawRate;motion.yawStep(8192,.8f);check(motion.yawRate-oldRate<=hp1::flightTurnBraking*.05f+.01f,"stall cannot trigger a large rotation step");
 }
 for(int n=1;n<4;++n)check(std::abs(distances[n]-distances[0])<160,"similar trajectories at 30/72/90/144 Hz");
 check(hp1::flightSpeed(500,500,1,0,false,false)<hp1::flightSpeed(500,500,0,0,false,false),"tight turns reduce speed rather than increasing view rotation");
 check(hp1::flightSpeed(800,500,0,0,true,false)==800,"straight boost retains original top speed");
 check(hp1::flightSpeed(800,500,0,0,true,true)==100,"precision brake overrides boost");
 check(hp1::flightSpeed(800,400,.6f,.5f,true,false,true)==800,"chase boost stays above Draco speed while correcting aim");
 check(hp1::flightSpeed(800,400,.6f,.5f,true,true,true)==80,"chase precision brake overrides native auxiliary boost");
 Vec origin={100,200,300},target={700,210,300};auto cue=hp1::flightPathPoint(origin,{300,0,0},{0,0,0},&target);
 check(std::fabs(cue.x-700)<.01f&&cue.y==200&&cue.z==300,"flight cue follows trajectory, not ring centre");
 auto stopped=hp1::flightPathPoint(origin,{0,0,0},{0,16384,0});check(std::fabs(stopped.x-100)<.01f&&stopped.y==584,"stopped cue follows broom, independent of head");
 std::puts("HP1 flight: precise input, short stopping distance, neutral levelling, turn speed, stereo cue geometry and comfort bounds passed");
}
