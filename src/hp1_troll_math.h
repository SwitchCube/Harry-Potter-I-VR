#pragma once
#include <algorithm>
#include <cmath>
#include "vr_math.h"
namespace hp1 {
// Harry.UpdateRotation and BossTrollChase.Tick define this course along +X.
// Looking behind must not reverse escape/avoidance. Pull back stops advancing.
inline hpvr::Vec trollAcceleration(float forward,float side,float rate,bool enabled){
 if(!enabled||!std::isfinite(forward)||!std::isfinite(side)||!std::isfinite(rate)||rate<=0)return {};
 forward=std::clamp(forward,0.0f,1.0f);side=std::clamp(side,-1.0f,1.0f);
 float scale=rate/std::max(1.0f,std::hypot(forward,side));
 return {forward*scale,side*scale,0};
}
}
