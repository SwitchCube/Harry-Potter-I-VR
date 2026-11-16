#pragma once
#include "vr_hud_capture.h"
static VrHudCapture cinemaScene;
static VrBridge cinemaTexture;
static VrMenu cinemaScreen{"cinema"};
static bool flatCinemaActive=false,cinemaHudBusy=false;
static unsigned cinemaSamples=0;
static void closeCinema(){cinemaScene.close();cinemaTexture.close();cinemaScreen.close();flatCinemaActive=cinemaHudBusy=false;}
static bool cinemaHudInternal(void* self,void* frame,void* result){
 if(!flatCinemaActive||cinemaHudBusy||!frame||!player||loadingDepth||self!=objectField(player,L"myHUD")||name(mem<void*>(frame,4))!=L"PostRender")return false;
 if(!cinemaScene.target||!currentViewport)return false;
 Fpu fpu;auto device=mem<IDirect3DDevice7*>(mem<void*>(currentViewport,0x5c),0x9a4);
 IDirectDrawSurface7* saved=nullptr;hr(device->GetRenderTarget(&saved),"Save cinema background");
 hr(device->EndScene(),"Finish cinema background");if(cinemaScene.borrowed)cinemaScene.copyBackground(true);hr(device->SetRenderTarget(cinemaScene.target,0),"Restore flat scene for native subtitles");DWORD z=0;hr(device->GetRenderState(D3DRENDERSTATE_ZENABLE,&z),"Cinema HUD depth");hr(device->SetRenderState(D3DRENDERSTATE_ZENABLE,FALSE),"Cinema HUD without depth");hr(device->BeginScene(),"Begin native cinema HUD");
 cinemaHudBusy=true;fpu.restore();originalInternal(self,frame,result);cinemaHudBusy=false;
 hr(device->EndScene(),"Finish flat scene and subtitles");DDSURFACEDESC2 d={};d.dwSize=sizeof(d);hr(cinemaScene.target->Lock(nullptr,&d,DDLOCK_WAIT|DDLOCK_READONLY,nullptr),"Read complete cinema image");
 try{cinemaTexture.open(startup.mode==3?runtime.adapter:0,d.dwWidth,d.dwHeight,false,&bridge);cinemaTexture.upload(0,d,!(cinemaTexture.verifiedMask&1),false);}catch(...){cinemaScene.target->Unlock(nullptr);throw;}
 hr(cinemaScene.target->Unlock(nullptr),"Unlock cinema image");cinemaTexture.flush();if(lobbyTestMode&&(cinemaSamples==0||cinemaSamples==100))capture(cinemaScene.target,cinemaSamples?L"cinema-original-film-later.bmp":L"cinema-original-film.bmp");
 if(startup.mode==3&&runtime.compositor->CanRenderScene()){auto plane=lobbyScreenPose(2.4f);cinemaScreen.width=2.3f;cinemaScreen.submit(runtime,cinemaTexture.textures[0],d.dwWidth,d.dwHeight,false,false,&plane);}
 hr(device->SetRenderTarget(saved,0),"Restore hall after cinema HUD");saved->Release();hr(device->SetRenderState(D3DRENDERSTATE_ZENABLE,z),"Restore cinema depth");hr(device->BeginScene(),"Resume original game after cinema");
 if(++cinemaSamples==1)log("{\"event\":\"hp1_cinema_screen\",\"original_scene_and_subtitles\":true,\"fixed_entrance_anchor\":true}");fpu.restore();return true;
}
