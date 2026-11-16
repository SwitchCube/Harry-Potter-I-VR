#include "hp1_lesson_math.h"
#include "hp1_hand.h"
#include <cstdio>
#include <cstdlib>
using namespace hpvr;
static void check(bool ok,const char* reason){if(!ok){std::fprintf(stderr,"FAIL: %s\n",reason);std::exit(1);}}
static void near(float a,float b){check(std::fabs(a-b)<.0002f,"ray/plane coordinates");}
int main(){
 unsigned count=0;
 for(Rot r:{Rot{0,0,0},Rot{0,-16816,0},Rot{8192,12000,-4096}}){
  auto axes=hp1::gameAxes(r);auto round=rotation(axes);check(std::abs(round.pitch-r.pitch)<=1&&std::abs(round.yaw-r.yaw)<=1&&std::abs(round.roll-r.roll)<=1,"authored plane orientation");
  hp1::LessonPlane plane={{1498,-6725,820},axes,300};
  for(float u:{0.0f,.2f,.5f,.85f,1.0f})for(float v:{0.0f,.3f,.5f,.9f,1.0f})for(float handOffset:{-20.0f,0.0f,30.0f}){
   auto target=hp1::lessonPoint(plane,u,v),origin=add(sub(plane.centre,scale(axes.forward,175)),scale(axes.right,handOffset));
   auto ray=hp1::lessonRay(plane,origin,hp1::unit(sub(target,origin),axes.forward));check(ray.valid,"intersection from offset physical hand");near(ray.u,u);near(ray.v,v);check(std::fabs(ray.world.x-target.x)<.001f&&std::fabs(ray.world.y-target.y)<.001f&&std::fabs(ray.world.z-target.z)<.001f,"world ray hit within float precision at large map coordinates");++count;
  }
  check(!hp1::lessonRay(plane,plane.centre,axes.right).valid,"parallel controller rejected");
  check(!hp1::lessonRay(plane,sub(plane.centre,scale(axes.forward,20)),scale(axes.forward,-1)).valid,"backward ray rejected");
  check(!hp1::lessonRay(plane,add(plane.centre,scale(axes.forward,20)),axes.forward).valid,"plane behind controller rejected");
 }
 std::printf("HP1 lesson: %u hand-ray positions, rotated planes, outside/backward/parallel rays passed\n",count);
}
