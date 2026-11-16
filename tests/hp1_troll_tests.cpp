#include "hp1_troll_math.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main(){
 auto f=hp1::trollAcceleration(1,0,1200,true);assert(f.x==1200&&f.y==0&&f.z==0);
 auto l=hp1::trollAcceleration(.8f,-.6f,1200,true),r=hp1::trollAcceleration(.8f,.6f,1200,true);
 assert(l.x>0&&l.y<0&&r.x==l.x&&r.y==-l.y);
 auto d=hp1::trollAcceleration(1,1,1200,true);assert(std::fabs(std::hypot(d.x,d.y)-1200)<.01f);
 for(auto p:{hp1::trollAcceleration(1,1,1200,false),hp1::trollAcceleration(0,0,1200,true),hp1::trollAcceleration(-1,0,1200,true),hp1::trollAcceleration(std::numeric_limits<float>::quiet_NaN(),1,1200,true)})assert(p.x==0&&p.y==0&&p.z==0);
 auto analog=hp1::trollAcceleration(.25f,0,1200,true);assert(analog.x==300);
 std::puts("Troll course axes, analog input, diagonal speed and inactive/invalid input passed");
}
