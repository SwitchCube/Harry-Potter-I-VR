#pragma once
#include "vr_math.h"
namespace hp1 {
// Inverse of HP1's tracking-to-entrance transform. The screen's game-world
// centre and upright normal do not depend on the entry head pose.
inline hpvr::Mat cinemaPlane(const hpvr::Origin& origin,float hallHeading,float distance){
 float q=hallHeading-origin.baseYaw,c=std::cos(q),s=std::sin(q);auto m=hpvr::identity();
 m.m[0][0]=c;m.m[2][0]=s;m.m[0][2]=-s;m.m[2][2]=c;
 m.m[0][3]=origin.reference.x+s*distance;m.m[1][3]=origin.reference.y;m.m[2][3]=origin.reference.z-c*distance;return m;
}
}
