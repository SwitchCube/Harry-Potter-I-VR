// HP1-specific integration. Built only against fingerprinted local engine modules.
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <d3d.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <xmmintrin.h>
#include "MinHook.h"
#include "vr_native_api.h"
#include "vr_math.h"
#include "hp1_hand.h"
#include "hp1_stereo_policy.h"
#include "hp1_native_target.h"
using hpvr::Vec;using hpvr::Rot;
#include "hp1_preferences.h"
#include "hp1_controls.h"
static_assert(sizeof(void*)==4,"HP1 is an x86 game");
template<class T>T& mem(void* p,unsigned off){return *reinterpret_cast<T*>(static_cast<BYTE*>(p)+off);}
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static FILE* trace=nullptr;
static NativeStartup startup={};
static int playTestMode=0;
static int mechanicsTestMode=0;
static int storyTestMode=0;
static bool trollTestMode=false;
static bool fluteTestMode=false;
static bool chessTestMode=false;
static bool lobbyTestMode=false;
static bool tutorialTestMode=false;
static bool tutorialLiveTestMode=false;
static bool mechanicsRecenter=false;
static unsigned mechanicsCapture=0;
static unsigned mirrorPlayerMask=0,mirrorCaptureMask=0,mirrorSavedMask=0;
static bool passiveTestMode=false;
static bool inputReplay(){return !passiveTestMode&&!playTestMode&&!mechanicsTestMode&&!storyTestMode&&!trollTestMode&&!fluteTestMode&&!chessTestMode&&!lobbyTestMode&&!tutorialTestMode&&(startup.mode==5||startup.mode==6);}
static DWORD gameThread=0;
static unsigned ticks=0,draws=0,secondaryCalls=0;
static bool pairActive=false,secondary=false,complete=false;
static unsigned loadingDepth=0,levelGeneration=0;
static unsigned secondaryPreparation=0,destroyActorDepth=0,secondaryLifecycleEvents=0;
using DestroyActor=int(__thiscall*)(void*,void*,int);
static DestroyActor originalDestroyActor=nullptr;
static int __fastcall destroyActorHook(void* level,void*,void* actor,int force){
    struct Scope{Scope(){++destroyActorDepth;}~Scope(){--destroyActorDepth;}} scope;
    return originalDestroyActor(level,actor,force);
}
static Rot cameraRot={};
static std::map<std::pair<void*,void*>,unsigned> primaryEvents;
using Tick=void(__thiscall*)(void*,float);
using World=void(__thiscall*)(void*,void*);
using Coords=void(__thiscall*)(void*,Vec&,Rot&);
using Create=void*(__thiscall*)(void*,void*,Vec,Rot,void*);
using Finish=void(__thiscall*)(void*);
using Dispatch=void(__thiscall*)(void*,void*,void*,void*);
using Property=void*(__cdecl*)(void*,const wchar_t*);
static Tick originalTick=nullptr;
static World originalWorld=nullptr;
static Coords originalCoords=nullptr;
static Create createFrame=nullptr;
static Finish finishFrame=nullptr;
static Dispatch originalDispatch=nullptr;
static Property findProperty=nullptr;
static void log(const char* fmt,...){if(!trace)return;va_list a;va_start(a,fmt);vfprintf(trace,fmt,a);va_end(a);fputc('\n',trace);fflush(trace);}
// Observe native faults before the original engine's error handler unwinds them.
// Never swallow an exception or attempt to resume a broken render frame.
static LONG CALLBACK nativeFault(EXCEPTION_POINTERS* info){
    if(!info||!info->ExceptionRecord||info->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION)return EXCEPTION_CONTINUE_SEARCH;
    static LONG reported=0;if(InterlockedIncrement(&reported)>4)return EXCEPTION_CONTINUE_SEARCH;
    HMODULE module=nullptr;char path[MAX_PATH]={};auto c=info->ContextRecord;auto e=info->ExceptionRecord;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(e->ExceptionAddress),&module);
    if(module)GetModuleFileNameA(module,path,MAX_PATH);const char* base=strrchr(path,'\\');base=base?base+1:path;
    log("{\"event\":\"hp1_native_fault\",\"module\":\"%s\",\"rva\":%lu,\"access\":%lu,\"address\":%lu,\"eax\":%lu,\"ebx\":%lu,\"ecx\":%lu,\"edx\":%lu,\"esi\":%lu,\"edi\":%lu,\"lobby\":%s,\"secondary\":%s}",base,DWORD(e->ExceptionAddress)-DWORD(module),e->NumberParameters?e->ExceptionInformation[0]:0,e->NumberParameters>1?e->ExceptionInformation[1]:0,c->Eax,c->Ebx,c->Ecx,c->Edx,c->Esi,c->Edi,lobbyRendering?"true":"false",secondary?"true":"false");
    return EXCEPTION_CONTINUE_SEARCH;
}
[[noreturn]]static void fatal(const char* stage,const char* what){log("{\"event\":\"fatal\",\"stage\":\"%s\",\"reason\":\"%s\"}",stage,what);TerminateProcess(GetCurrentProcess(),126);for(;;)Sleep(INFINITE);}
static std::wstring name(void* o){need(o!=nullptr,"Null reflected object");auto entry=mem<wchar_t*>(o,0x20);need(entry!=nullptr,"Missing FName entry");std::wstring n(entry+6);need(n.size()<256,"Invalid reflected name");return n;}
static unsigned field(void* o,const wchar_t* key,unsigned size,const wchar_t* type){
    void* p=findProperty(mem<void*>(o,0x24),key);need(p!=nullptr,"Missing HP1 reflected property");
    need(name(mem<void*>(p,0x24))==type,"HP1 reflected property type differs");
    need(mem<DWORD>(p,0x38)==size,"HP1 property element size differs");
    unsigned off=mem<DWORD>(p,0x48);need(off>=0x28&&off+size<65536,"HP1 property offset outside object");return off;
}
struct Fpu{unsigned short cw;unsigned csr;Fpu(){asm volatile("fnstcw %0":"=m"(cw));csr=_mm_getcsr();}void restore(){asm volatile("fldcw %0"::"m"(cw));_mm_setcsr(csr);}};
static void hr(HRESULT h,const char* why){if(FAILED(h)){log("{\"event\":\"d3d_error\",\"hr\":%ld}",h);throw std::runtime_error(why);}}
#include "hp1_gameplay.h"
#include "hp1_reflection.h"
static void capture(IDirectDrawSurface7* surface,const wchar_t* file){
    DDSURFACEDESC2 d={};d.dwSize=sizeof(d);hr(surface->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Lock HP1 target");
    try{
        need(d.dwWidth>=64&&d.dwWidth<=4096&&d.dwHeight>=64&&d.dwHeight<=4096,"Invalid target size");
        need(d.ddpfPixelFormat.dwRGBBitCount==32&&d.lPitch>=LONG(d.dwWidth*4),"HP1 target must be RGB32");
        // Captures must not need another contiguous 12 MB after a long x86
        // session. Stream one scanline while retaining the surface lock.
        unsigned stride=(d.dwWidth*3+3)&~3u;std::vector<BYTE> pixels(stride);
        auto channel=[](DWORD v,DWORD mask){unsigned shift=0;while(mask&&!(mask&1)){++shift;mask>>=1;}return BYTE(mask?(((v>>shift)&mask)*255+mask/2)/mask:0);};
        BITMAPFILEHEADER f={};BITMAPINFOHEADER i={};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(i);f.bfSize=f.bfOffBits+stride*d.dwHeight;i.biSize=sizeof(i);i.biWidth=d.dwWidth;i.biHeight=-LONG(d.dwHeight);i.biPlanes=1;i.biBitCount=24;i.biSizeImage=stride*d.dwHeight;
        auto path=std::wstring(startup.profile)+L"\\"+file;HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);need(h!=INVALID_HANDLE_VALUE,"Create HP1 capture");
        DWORD count=0;bool ok=WriteFile(h,&f,sizeof(f),&count,nullptr)&&count==sizeof(f)&&WriteFile(h,&i,sizeof(i),&count,nullptr)&&count==sizeof(i);
        for(unsigned y=0;y<d.dwHeight&&ok;++y){for(unsigned x=0;x<d.dwWidth;++x){DWORD v=mem<DWORD>(static_cast<BYTE*>(d.lpSurface)+y*d.lPitch,x*4);auto p=&pixels[x*3];p[0]=channel(v,d.ddpfPixelFormat.dwBBitMask);p[1]=channel(v,d.ddpfPixelFormat.dwGBitMask);p[2]=channel(v,d.ddpfPixelFormat.dwRBitMask);}ok=WriteFile(h,pixels.data(),stride,&count,nullptr)&&count==stride;}
        CloseHandle(h);need(ok,"Write HP1 capture");
        log("{\"event\":\"capture\",\"file\":\"%ls\",\"width\":%lu,\"height\":%lu}",file,d.dwWidth,d.dwHeight);
    }catch(...){surface->Unlock(nullptr);throw;}
    hr(surface->Unlock(nullptr),"Unlock HP1 target");
}
static void __fastcall tickHook(void* self,void*,float dt){if(pairActive)fatal("tick","Game tick repeated inside stereo pair");currentEngine=self;++ticks;originalTick(self,dt);try{gameplayTick();}catch(const std::exception& e){fatal("hp1_scene",e.what());}}
static void __fastcall coordsHook(void* frame,void*,Vec& p,Rot& r){cameraRot=r;if(!loadingDepth){currentViewport=mem<void*>(frame,0);player=mem<void*>(currentViewport,0x30);}originalCoords(frame,p,r);}
static void __fastcall dispatchHook(void* self,void*,void* function,void* params,void* result){
    if(lobbyRendering)return; // Static background has no gameplay callbacks.
    if(self==player&&name(function)==L"Mount"){
        ++originalMounts;Vec delta=params?*static_cast<Vec*>(params):Vec{};
        log("{\"event\":\"hp1_original_mount\",\"number\":%u,\"delta\":[%.4f,%.4f,%.4f]}",originalMounts,delta.x,delta.y,delta.z);
    }
    if(pairActive){
        auto key=std::make_pair(self,function);
        if(secondary){
            auto n=name(function);
            if(startup.mode==3||startup.mode==4||startup.mode==6){
                auto decision=hp1::stereoEvent(n,primaryEvents.count(key)!=0,secondaryPreparation!=0,destroyActorDepth!=0);
                if(decision==hp1::StereoEvent::DuplicateDestruction)fatal("stereo","Duplicate actor destruction inside stereo pair");
                if(decision==hp1::StereoEvent::Lifecycle){
                    ++primaryEvents[key];
                    if(n==L"Destroyed"&&++secondaryLifecycleEvents<=16)log("{\"event\":\"hp1_secondary_lifecycle\",\"name\":\"Destroyed\",\"actor\":\"%ls\",\"class\":\"%ls\",\"native_destroy_depth\":%u}",name(self).c_str(),name(mem<void*>(self,0x24)).c_str(),destroyActorDepth);
                    originalDispatch(self,function,params,result);return;
                }
                if(decision==hp1::StereoEvent::Skip){++secondaryCalls;return;}
                if(decision==hp1::StereoEvent::Prepare){
                    ++primaryEvents[key];++secondaryPreparation;originalDispatch(self,function,params,result);--secondaryPreparation;return;
                }
            }
            if(primaryEvents.count(key)&&(n==L"Update"||n==L"RenderOverlays"||n==L"RenderTexture")){++secondaryCalls;return;}
            // Initial proof fails closed on unknown render-time effects.
            log("{\"event\":\"secondary_callback\",\"name\":\"%ls\",\"actor\":\"%ls\",\"class\":\"%ls\",\"native_destroy_depth\":%u}",n.c_str(),name(self).c_str(),name(mem<void*>(self,0x24)).c_str(),destroyActorDepth);
            fatal("stereo","Unreviewed second-eye script callback");
        }
        ++primaryEvents[key];
    }
    originalDispatch(self,function,params,result);
}
#include "hp1_lobby.h"
#include "hp1_vr.h"
#include "hp1_menu.h"
#include "hp1_tutorial_test.h"
#include "hp1_settings_test.h"
#include "hp1_lobby_test.h"
#include "hp1_story_ui.h"
#include "hp1_lifecycle.h"
static void __fastcall worldHook(void* renderer,void*,void* frame){
    if(vrWorld(renderer,frame))return;
    ++draws;
    if(complete||draws<180){originalWorld(renderer,frame);return;}
    Fpu fpu;
    try{
        need(GetCurrentThreadId()==gameThread,"HP1 render thread changed");
        need(mem<DWORD>(renderer,0xa8)==1,"HP1 master depth differs");
        void* viewport=mem<void*>(frame,0),*actor=mem<void*>(viewport,0x30),*rd=mem<void*>(viewport,0x5c);
        need(name(mem<void*>(rd,0x24))==L"D3DRenderDevice","HP1 requires original D3D7 renderer");
        unsigned loc=field(actor,L"Location",12,L"StructProperty"),rot=field(actor,L"Rotation",12,L"StructProperty");
        Vec actorPos=mem<Vec>(actor,loc);Rot actorRot=mem<Rot>(actor,rot);
        unsigned eye=field(actor,L"BaseEyeHeight",4,L"FloatProperty");
        log("{\"event\":\"hp1_actor_verified\",\"class\":\"%ls\",\"location_offset\":%u,\"rotation_offset\":%u,\"eye_height_offset\":%u,\"eye_height\":%.6g}",name(mem<void*>(actor,0x24)).c_str(),loc,rot,eye,mem<float>(actor,eye));
        auto device=mem<IDirect3DDevice7*>(rd,0x9a4);need(device!=nullptr,"HP1 D3D7 device missing");
        Vec position=mem<Vec>(frame,0x34),right=mem<Vec>(frame,0x40);Rot rotation=cameraRot;unsigned tick=ticks;
        IDirectDrawSurface7* surface=nullptr;
        hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Clear first eye");
        pairActive=true;primaryEvents.clear();fpu.restore();originalWorld(renderer,frame);
        hr(device->EndScene(),"End first eye");hr(device->GetRenderTarget(&surface),"Read original HP1 target");capture(surface,L"eye-a.bmp");
        hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Clear second eye");hr(device->BeginScene(),"Begin second eye");
        float shift=startup.mode==1?0:8;Vec other=hpvr::add(position,hpvr::scale(right,shift));
        secondary=true;fpu.restore();void* second=createFrame(renderer,viewport,other,rotation,nullptr);
        need(second&&second!=frame&&mem<DWORD>(renderer,0xa8)==2,"HP1 second frame lifetime differs");
        void* engine=mem<void*>(renderer,kRenderEngineOffset);unsigned ao=kEngineAudioOffset;void* audio=mem<void*>(engine,ao);mem<void*>(engine,ao)=nullptr;
        fpu.restore();originalWorld(renderer,second);mem<void*>(engine,ao)=audio;
        hr(device->EndScene(),"End second eye");capture(surface,L"eye-b.bmp");hr(device->BeginScene(),"Restore original scene");
        secondary=false;fpu.restore();finishFrame(renderer);originalCoords(frame,position,rotation);surface->Release();fpu.restore();
        need(ticks==tick&&mem<DWORD>(renderer,0xa8)==1,"HP1 stereo lifetime/tick mismatch");
        need(!std::memcmp(&actorPos,&mem<Vec>(actor,loc),12)&&!std::memcmp(&actorRot,&mem<Rot>(actor,rot),12),"HP1 stereo changed actor pose");
        complete=true;pairActive=false;
        log("{\"event\":\"hp1_pair_complete\",\"shift\":%.1f,\"extra_ticks\":0,\"pose_unchanged\":true,\"suppressed_second_eye_callbacks\":%u}",shift,secondaryCalls);
    }catch(const std::exception& e){fatal("render",e.what());}
}
static void* target(const char* key){for(const auto& t:kNativeTargets)if(!std::strcmp(key,t.key)){auto m=GetModuleHandleW(t.module);need(m!=nullptr,"Missing HP1 native module");auto p=reinterpret_cast<BYTE*>(m)+t.rva;need(!std::memcmp(p,t.prefix,5),"HP1 native prolog differs");return p;}throw std::runtime_error("Missing target descriptor");}
static void hook(const char* key,void* replacement,void** original){void* p=target(key);need(MH_CreateHook(p,replacement,original)==MH_OK,"Create HP1 hook");need(MH_QueueEnableHook(p)==MH_OK,"Queue HP1 hook");}
extern "C" __declspec(dllexport) DWORD __stdcall HP1VR_Initialize(NativeStartup* args){
    try{
        need(args&&args->size==sizeof(NativeStartup)&&args->version==1,"Invalid HP1 startup ABI");startup=*args;
        need(startup.mode>=1&&startup.mode<=6,"Unsupported HP1 native mode");
        std::wstring root=startup.root,profile=startup.profile;need(profile.find(root+L"\\cache\\")==0,"Native profile outside HP1");
        if(startup.mode==6){FILE* test=_wfopen((profile+L"\\play-test.flag").c_str(),L"rb");if(test){int c=fgetc(test);fclose(test);playTestMode=c=='n'?1:c=='l'?2:c=='m'?3:0;}}
        if(startup.mode==6){FILE* test=_wfopen((profile+L"\\mechanics-test.flag").c_str(),L"rb");if(test){int c=fgetc(test);fclose(test);mechanicsTestMode=c=='m'?1:c=='l'?2:0;}}
        if(startup.mode==6){FILE* test=_wfopen((profile+L"\\story-test.flag").c_str(),L"rb");if(test){int c=fgetc(test);fclose(test);storyTestMode=c=='b'?1:c=='m'?2:0;}}
        trollTestMode=startup.mode==6&&GetFileAttributesW((profile+L"\\test-troll.flag").c_str())!=INVALID_FILE_ATTRIBUTES;
        lobbyTestMode=startup.mode==6&&GetFileAttributesW((profile+L"\\test-lobby.flag").c_str())!=INVALID_FILE_ATTRIBUTES;
        tutorialTestMode=startup.mode==6&&GetFileAttributesW((profile+L"\\test-tutorial.flag").c_str())!=INVALID_FILE_ATTRIBUTES;
        tutorialLiveTestMode=startup.mode==3&&GetFileAttributesW((profile+L"\\test-tutorial-live.flag").c_str())!=INVALID_FILE_ATTRIBUTES;
        chessTestMode=startup.mode==6&&GetFileAttributesW((profile+L"\\test-chess.flag").c_str())!=INVALID_FILE_ATTRIBUTES;
        fluteTestMode=startup.mode==6&&GetFileAttributesW((profile+L"\\test-flute.flag").c_str())!=INVALID_FILE_ATTRIBUTES;
        trace=_wfopen((profile+L"\\hp1vr-native.jsonl").c_str(),L"wx");need(trace!=nullptr,"Open private HP1 native log");gameThread=GetCurrentThreadId();
        vrPreferences.load(startup.root,startup.profile);
        passiveTestMode=startup.mode==6&&GetFileAttributesW((profile+L"\\test-passive.flag").c_str())!=INVALID_FILE_ATTRIBUTES;
        log("{\"event\":\"hp1_preferences_loaded\",\"english\":%s,\"resolution\":%d}",vrPreferences.english?"true":"false",vrPreferences.resolution);
        AddVectoredExceptionHandler(1,nativeFault);
        findProperty=reinterpret_cast<Property>(target("property"));createFrame=reinterpret_cast<Create>(target("create"));finishFrame=reinterpret_cast<Finish>(target("finish"));
        need(MH_Initialize()==MH_OK,"Initialize HP1 hooks");
        hook("tick",reinterpret_cast<void*>(tickHook),reinterpret_cast<void**>(&originalTick));
        hook("coords",reinterpret_cast<void*>(coordsHook),reinterpret_cast<void**>(&originalCoords));
        hook("world",reinterpret_cast<void*>(worldHook),reinterpret_cast<void**>(&originalWorld));
        hook("dispatch",reinterpret_cast<void*>(dispatchHook),reinterpret_cast<void**>(&originalDispatch));
        hook("internal",reinterpret_cast<void*>(internalHook),reinterpret_cast<void**>(&originalInternal));
        hook("loadmap",reinterpret_cast<void*>(loadMapHook),reinterpret_cast<void**>(&originalLoadMap));
        hook("destroyactor",reinterpret_cast<void*>(destroyActorHook),reinterpret_cast<void**>(&originalDestroyActor));
        hook("mesh",reinterpret_cast<void*>(meshDrawHook),reinterpret_cast<void**>(&originalMeshDraw));
        hook("lodmesh",reinterpret_cast<void*>(lodDrawHook),reinterpret_cast<void**>(&originalLodDraw));
        hook("destroy",reinterpret_cast<void*>(destroyHook),reinterpret_cast<void**>(&originalDestroy));
        hook("menuprint",reinterpret_cast<void*>(menuPrintHook),reinterpret_cast<void**>(&originalMenuPrint));
        hook("menustring",reinterpret_cast<void*>(menuStringHook),reinterpret_cast<void**>(&originalMenuString));
        hook("traceactors",reinterpret_cast<void*>(traceActorsHook),reinterpret_cast<void**>(&originalTraceActors));
        need(MH_ApplyQueued()==MH_OK,"Enable HP1 hooks");
        log("{\"event\":\"hp1_native_ready\",\"mode\":%lu,\"vr\":false}",startup.mode);return 0;
    }catch(const std::exception& e){log("{\"event\":\"init_error\",\"reason\":\"%s\"}",e.what());return 1;}
}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
