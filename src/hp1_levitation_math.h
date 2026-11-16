#pragma once
#include "vr_math.h"
namespace hp1 {
// HP1 baseProps.Tick uses a horizontal target encoded as camera-relative
// dist/rotv, followed by native Trace/Move and its original landing triggers.
struct LevitationTarget {float distance;hpvr::Rot rotation;hpvr::Vec point;};
inline LevitationTarget levitationTarget(hpvr::Vec object,hpvr::Vec camera,int yaw,float forward,float side,bool active){
 if(!active||!std::isfinite(forward)||!std::isfinite(side))forward=side=0;
 float length=std::hypot(forward,side);if(length>1){forward/=length;side/=length;}
 auto point=hpvr::add(object,hpvr::yaw({forward*96,side*96,0},yaw*hpvr::pi/32768));
 float dx=point.x-camera.x,dy=point.y-camera.y,range=std::hypot(dx,dy);
 float distance=std::max(60.0f,std::min(400.0f,range));
 int angle=range>.001f?int(std::lround(-std::atan2(dy,dx)*32768/hpvr::pi)):0;
 float a=-angle*hpvr::pi/32768;
 return {distance,{0,angle,0},{camera.x+std::cos(a)*distance,camera.y+std::sin(a)*distance,object.z}};
}
}
