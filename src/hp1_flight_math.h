#pragma once
#include "hp1_lesson_math.h"
namespace hp1 {
constexpr float flightPitchRate=6000,flightYawRate=8192;
constexpr float flightTurnAcceleration=24576,flightTurnBraking=49152;
inline float flightAxis(float v){return std::isfinite(v)?std::clamp(v,-1.0f,1.0f):0;}
inline int signedAngle(int angle){int a=angle&65535;return a>=32768?a-65536:a;}
// A scene-skip button already held at the start of a catch must not spend a try.
struct FlightActionLatch {
 bool wasWindow=false,armed=false;
 bool update(bool window,bool held){if(!window||!wasWindow)armed=false;wasWindow=window;if(window&&!held)armed=true;return window&&armed&&held;}
};
struct FlightViewHeading {
 float yaw=0;bool ready=false;
 void reset(int angle){yaw=float(angle&65535);ready=true;}
 int update(int target,float dt){
  if(!ready)reset(target);
  if(std::isfinite(dt)&&dt>0){float error=float(signedAngle(target-int(std::lround(yaw))));yaw+=std::clamp(error,-flightYawRate*std::min(dt,.05f),flightYawRate*std::min(dt,.05f));yaw=std::fmod(yaw+65536,65536.0f);}
  return int(std::lround(yaw))&65535;
 }
};
inline float flightCatchFraction(int target){return std::clamp(target/256.0f,0.0f,1.0f);}
inline bool flightCatchReady(int target,int limit){return target<limit;}
// Independent dead zones prevent an incidental vertical component from
// changing the flight slope while making a small horizontal correction.
inline float flightStick(float v){v=flightAxis(v);return std::fabs(v)<=.12f?0:std::copysign((std::fabs(v)-.12f)/.88f,v);}
inline float flightYaw(float x){x=flightAxis(x);return x*(.65f+.35f*x*x);}
inline float flightSpeed(float stock,float normal,float turn,float climb,bool boost,bool brake,bool pursuit=false){
 if(brake)return std::min(stock,normal*.20f);
 // Chases need the original boost speed even while correcting direction:
 // Draco accelerates to 720 nearby. Hooch's stronger curve slowdown would
 // cap Harry below that and prevent closing the gap. A remains precision.
 if(pursuit&&boost)return std::max(0.0f,stock);
 float correction=std::max(std::fabs(flightAxis(turn)),.65f*std::fabs(flightAxis(climb)));
 // Reduce the turn radius instead of requiring faster headset rotation.
 float cruise=normal*(.85f-.40f*correction);
 float limit=boost?std::max(cruise,stock*(1-.65f*correction)):cruise;
 return std::max(0.0f,std::min(stock,limit));
}
inline hpvr::Vec flightPathPoint(hpvr::Vec origin,hpvr::Vec velocity,hpvr::Rot body,const hpvr::Vec* target=nullptr){
 using namespace hpvr;float speed=std::sqrt(dot(velocity,velocity));
 Vec direction=std::isfinite(speed)&&speed>5?scale(velocity,1/speed):gameAxes(body).forward;
 float distance=target?dot(sub(*target,origin),direction):384;
 return add(origin,scale(direction,std::clamp(distance,128.0f,1400.0f)));
}
// Artificial motion never pitches or banks the horizon. Real head tilt stays
// immediate, including when entering/recentring while looking up or down.
inline hpvr::Axes flightAxes(hpvr::Rot body){body.pitch=body.roll=0;return gameAxes(body);}
inline hpvr::Mat flightHeadReference(const hpvr::Mat& head){
 using namespace hpvr;float a=heading(head);Mat ref=identity();
 ref.m[0][0]=ref.m[2][2]=std::cos(a);ref.m[0][2]=-std::sin(a);ref.m[2][0]=std::sin(a);
 for(int i=0;i<3;++i)ref.m[i][3]=head.m[i][3];return ref;
}
struct FlightMotion {
 float yawRate=0,yawRemainder=0,pitchRate=0,pitchRemainder=0,speed=0;bool ready=false;
 void reset(float actualSpeed=0){yawRate=yawRemainder=pitchRate=pitchRemainder=0;speed=std::max(0.0f,actualSpeed);ready=true;}
 static float step(float from,float to,float amount){return from+std::clamp(to-from,-amount,amount);}
 static float rateStep(float rate,float requested,float dt,float acceleration,float brakingRate){
  bool braking=requested*rate<=0||std::fabs(requested)<std::fabs(rate);
  float filtered=rate+(requested-rate)*(1-std::exp(-dt/(braking?.035f:.075f)));
  rate=step(rate,filtered,(braking?brakingRate:acceleration)*dt);
  return std::fabs(rate)<1&&requested==0?0:rate;
 }
 // Deflection selects the climb angle. Per the user's preference, centre
 // returns to level. Brake the slope adjustment before reaching its target.
 int pitchStep(float input,int current,float up,float down,bool invert,float dt){
  current=signedAngle(current);if(!std::isfinite(dt)||dt<=0)return current;dt=std::min(dt,.05f);
  input=flightAxis(input)*(invert?-1:1);int target=int(input*(input>=0?up:down)*32768/180),error=target-current;
  float limit=input==0?9000:flightPitchRate;
  float requested=std::clamp(error/(input==0?.06f:.10f),-limit,limit);
  pitchRate=rateStep(pitchRate,requested,dt,24000,48000);
  float value=pitchRate*dt+pitchRemainder;int delta=int(std::round(value));pitchRemainder=value-delta;
  if(error==0||(error>0&&delta>=error)||(error<0&&delta<=error)){pitchRate=pitchRemainder=0;return target;}
  int wanted=current+delta,result=std::clamp(wanted,-int(down*32768/180),int(up*32768/180));
  if(result!=wanted){pitchRate=0;pitchRemainder=0;}
  return result;
 }
 int yawStep(float requested,float dt){
  if(!std::isfinite(dt)||dt<=0)return 0;dt=std::min(dt,.05f);
  requested=std::clamp(requested,-flightYawRate,flightYawRate);
  yawRate=rateStep(yawRate,requested,dt,flightTurnAcceleration,flightTurnBraking);
  float value=yawRate*dt+yawRemainder;int whole=int(std::round(value));yawRemainder=value-whole;return whole;
 }
 float speedStep(float requested,float actual,float dt){
  if(!ready)reset(actual);if(!std::isfinite(dt)||dt<=0)return speed;dt=std::min(dt,.05f);
  // After a real collision, accelerate from the actual stopped/slower speed.
  if(actual+5<speed)speed=std::max(0.0f,actual);
  speed=step(speed,std::max(0.0f,requested),(requested<speed?400.0f:220.0f)*dt);return speed;
 }
};
}
