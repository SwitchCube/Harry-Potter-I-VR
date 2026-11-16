#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <stdexcept>
#include <algorithm>
#undef NDEBUG
#include <cassert>
#include "hp1_preferences.h"
#include "hp1_cinema_math.h"
int main(){
 wchar_t cwd[32768];assert(GetCurrentDirectoryW(32768,cwd));auto root=std::wstring(cwd)+L"\\cache\\preferences-unit";CreateDirectoryW(root.c_str(),nullptr);CreateDirectoryW((root+L"\\config").c_str(),nullptr);CreateDirectoryW((root+L"\\profile").c_str(),nullptr);
 auto profile=root+L"\\profile";auto ini=profile+L"\\HP.ini";assert(WritePrivateProfileStringW(L"Engine.Engine",L"Language",L"ger",ini.c_str()));
 Hp1Preferences p;p.load(root.c_str(),profile.c_str());p.language=0;p.locale();assert(!p.english);p.language=2;p.locale();assert(p.english);
 for(int i=0;i<6;++i)p.source[i]=i;p.assign(0,5);assert(!strcmp(p.mapped("cast"),"show_health")&&!strcmp(p.mapped("show_health"),"cast"));p.swapSticks=true;assert(!strcmp(p.mapped("move"),"turn"));p.resolution=1536;p.spatial=false;p.save();
 Hp1Preferences restored;restored.load(root.c_str(),profile.c_str());assert(restored.english&&restored.source[0]==5&&restored.source[5]==0&&restored.resolution==1536&&!restored.spatial&&restored.swapSticks);
 assert(WritePrivateProfileStringW(L"Bindings",L"Action1",L"5",p.path.c_str()));restored.load(root.c_str(),profile.c_str());for(int i=0;i<6;++i)assert(restored.source[i]==i);
 const hpvr::Vec anchor={120,-600,48};float hall=.73f;
 for(float heading:{-2.f,-.7f,0.f,1.2f,3.f}){auto h=hpvr::identity();h.m[0][0]=cos(heading);h.m[2][0]=sin(heading);h.m[0][2]=-sin(heading);h.m[2][2]=cos(heading);h.m[0][3]=1.2f;h.m[1][3]=1.6f;h.m[2][3]=-.4f;hpvr::Origin o;o.recenter(h,hall);
  auto plane=hp1::cinemaPlane(o,hall,2.4f);auto world=o.position(plane,anchor,64);auto expected=hpvr::add(anchor,hpvr::yaw({2.4f*64,0,0},hall));auto delta=hpvr::sub(world,expected);assert(hpvr::dot(delta,delta)<.00001f);assert(fabs(plane.m[1][0])<.00001&&fabs(plane.m[1][2])<.00001&&plane.m[1][1]==1);
 }
 puts("HP1 preferences: permutation, persistence, locale override, invalid mapping recovery and fixed entrance screen across entry poses passed.");
}
