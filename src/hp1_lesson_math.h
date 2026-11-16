#pragma once
#include "vr_math.h"
namespace hp1 {
inline hpvr::Axes gameAxes(hpvr::Rot r){
 using namespace hpvr;float p=r.pitch*pi/32768,y=r.yaw*pi/32768,b=r.roll*pi/32768;
 Vec f={std::cos(p)*std::cos(y),std::cos(p)*std::sin(y),std::sin(p)},right={-std::sin(y),std::cos(y),0},up={-std::sin(p)*std::cos(y),-std::sin(p)*std::sin(y),std::cos(p)};
 return {f,add(scale(right,std::cos(b)),scale(up,-std::sin(b))),add(scale(up,std::cos(b)),scale(right,std::sin(b)))};
}
struct LessonPlane {hpvr::Vec centre;hpvr::Axes axes;float size;};
struct LessonHit {bool valid=false;float u=.5f,v=.5f;hpvr::Vec world={};};
inline hpvr::Vec lessonPoint(const LessonPlane& p,float u,float v){using namespace hpvr;return add(p.centre,add(scale(p.axes.right,(u-.5f)*p.size),scale(p.axes.up,(.5f-v)*p.size)));}
inline LessonHit lessonRay(const LessonPlane& p,hpvr::Vec origin,hpvr::Vec direction){
 using namespace hpvr;LessonHit hit;float d=dot(direction,p.axes.forward);
 if(!std::isfinite(d)||d<.0001f||!std::isfinite(p.size)||p.size<=0)return hit;
 float distance=dot(sub(p.centre,origin),p.axes.forward)/d;if(!std::isfinite(distance)||distance<0||distance>10000)return hit;
 auto point=add(origin,scale(direction,distance)),relative=sub(point,p.centre);
 float u=.5f+dot(relative,p.axes.right)/p.size,v=.5f-dot(relative,p.axes.up)/p.size;
 if(!std::isfinite(u)||!std::isfinite(v)||u<-.5f||u>1.5f||v<-.5f||v>1.5f)return hit;
 return {true,u,v,point};
}
}
