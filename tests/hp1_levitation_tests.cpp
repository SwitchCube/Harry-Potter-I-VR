#include "hp1_levitation_math.h"
#include <cassert>
#include <cstdio>
int main(){
 using hpvr::Vec;Vec c={0,0,500},p={200,0,90};
 auto still=hp1::levitationTarget(p,c,0,0,0,true);assert(std::fabs(still.point.x-200)<.02f&&std::fabs(still.point.y)<.02f&&still.point.z==90);
 auto f=hp1::levitationTarget(p,c,0,1,0,true),b=hp1::levitationTarget(p,c,0,-1,0,true),r=hp1::levitationTarget(p,c,0,0,1,true),l=hp1::levitationTarget(p,c,0,0,-1,true);
 assert(f.point.x>p.x+90&&b.point.x<p.x-90&&r.point.y>90&&l.point.y< -90);
 auto turn=hp1::levitationTarget(p,c,16384,1,0,true);assert(turn.point.y>90&&std::fabs(turn.point.x-p.x)<.02f);
 for(bool active:{false,true})for(int yaw:{-32768,-16384,0,16384,32768})for(float x:{-1.f,0.f,1.f})for(float y:{-1.f,0.f,1.f}){
  auto v=hp1::levitationTarget(p,c,yaw,x,y,active);assert(v.distance>=60&&v.distance<=400&&v.point.z==p.z);
  assert(std::hypot(v.point.x-p.x,v.point.y-p.y)<=96.03f);if(!active)assert(std::hypot(v.point.x-p.x,v.point.y-p.y)<.03f);
 }
 assert(hp1::levitationTarget({450,0,90},c,0,1,0,true).distance==400);
 assert(hp1::levitationTarget({80,0,90},c,0,-1,0,true).distance==60);
 std::puts("HP1 levitation: head-relative directions, diagonal normalization, neutral/focus, native range and fixed height passed");
}
