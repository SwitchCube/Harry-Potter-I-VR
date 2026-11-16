#include <windows.h>
#include <d3d11.h>
#include "vr_math.h"
#include "vr_runtime.h"
#include "vr_menu.h"
#include <cstdlib>
static void check(bool ok){if(!ok){std::puts("FAIL: menu pointer reference");std::exit(1);}}
int main(){
    VrMenu menu;menu.placed=true;menu.width=2;menu.height=1.5f;menu.plane=hpvr::identity();menu.plane.m[2][3]=-1;
    for(float u:{.05f,.25f,.5f,.8f,.95f})for(float v:{.05f,.4f,.9f}){
        auto hand=hpvr::identity();hand.m[0][3]=(u-.5f)*2;hand.m[1][3]=(.5f-v)*1.5f;
        check(menu.point(hand));check(std::fabs(menu.cursorX-u)<.00001f&&std::fabs(menu.cursorY-v)<.00001f);
    }
    auto hand=hpvr::identity();hand.m[0][3]=3;check(!menu.point(hand));
    hand=hpvr::identity();hand.m[2][3]=-2;check(!menu.point(hand));
    hand=hpvr::identity();hand.m[2][2]=-1;check(!menu.point(hand));
    hand=hpvr::identity();hand.m[0][2]=1;hand.m[2][2]=0;check(!menu.point(hand));
    std::puts("HP1 menu: 15 normalized pointer positions, outside/behind/backward/parallel rays passed");
}
