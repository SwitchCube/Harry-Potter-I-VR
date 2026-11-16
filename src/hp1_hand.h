#pragma once
#include "vr_wand_pose.h"
namespace hp1 {
// The user's confirmed HP2 grip calibration, in metres/degrees. HP1 keeps its
// own 64 world units/metre; render geometry, targeting and spell origin agree.
constexpr float wandLength=.28f;
inline hpvr::Mat wandPose(const hpvr::Mat& grip){return hpvr::heldWandPose(grip,{}, {-15.563f,0,0},wandLength);}
inline hpvr::Vec cross(hpvr::Vec a,hpvr::Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline hpvr::Vec unit(hpvr::Vec v,hpvr::Vec fallback){float n=std::sqrt(hpvr::dot(v,v));return std::isfinite(n)&&n>.0001f?hpvr::scale(v,1/n):fallback;}
// Same physical-fist attachment as HP2. Turning the head does not translate it.
inline hpvr::Mat statusPlane(const hpvr::Mat& grip,const hpvr::Mat& head){
 using namespace hpvr;Vec centre=translation(grip);centre.y+=.10f;
 Vec normal=unit(sub(translation(head),centre),axis(head,2));
 Vec right=unit(cross({0,1,0},normal),axis(head,0));
 Vec up=unit(cross(normal,right),axis(head,1));right=unit(cross(up,normal),right);
 Mat p={};Vec axes[4]={right,up,normal,centre};
 for(int col=0;col<4;++col){p.m[0][col]=axes[col].x;p.m[1][col]=axes[col].y;p.m[2][col]=axes[col].z;}return p;
}
inline int healthPercent(float life,float max){if(!std::isfinite(life)||!std::isfinite(max)||max<=0)return 0;return int(std::lround(std::max(0.0f,std::min(1.0f,life/max))*100));}
inline bool showStatus(bool held,bool validHand,bool sceneReady,bool menu,bool cinema){return held&&validHand&&sceneReady&&!menu&&!cinema;}
}
