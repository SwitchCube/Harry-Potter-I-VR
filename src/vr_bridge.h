#pragma once
#ifndef D3DCOLORVALUE_DEFINED
#define D3DCOLORVALUE_DEFINED // d3d.h already provided the four-float D3D7 structure.
#endif
#include "vr_profile.h"
#include "vr_pixels.h"
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <string>
// D3D7 cannot be submitted to OpenVR. This initial bridge explicitly copies CPU pixels
// to two persistent D3D11 textures on the compositor's adapter. No zero-copy claim.
struct VrBridge {
#ifdef HP1VR_DIRECT_BGRX
 static constexpr bool directBgrx=true;
#else
 static constexpr bool directBgrx=false;
#endif
 static const char* modeName(){return directBgrx?"direct-bgrx":"opaque-bgra-sse2";}
 ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
 ID3D11Texture2D* textures[2]={};ID3D11Texture2D* staging=nullptr;
 // Read back in strips: x86 address space can be fragmented even when hundreds
 // of MB remain free. Verification must not require another full-size texture.
 static constexpr unsigned verificationRows=32;
 bool alphaTexture=false;unsigned width=0,height=0,verifiedMask=0,verificationSkippedMask=0,generation=0;std::vector<unsigned char> pixels;
#ifdef HP1VR_BRIDGE_TEST
 HRESULT testVerificationCreate=S_OK,testVerificationMap=S_OK;unsigned testCreateCalls=0;
#endif
 static void check(HRESULT r,const char* what){if(FAILED(r))throw std::runtime_error(std::string(what)+" HRESULT="+std::to_string(r));}
 ~VrBridge(){close();}
 void resetVerification(){verifiedMask=verificationSkippedMask=0;}
 void releaseTextures(){if(staging)staging->Release();staging=nullptr;for(auto& t:textures){if(t)t->Release();t=nullptr;}width=height=0;resetVerification();std::vector<unsigned char>().swap(pixels);}
 void close(){releaseTextures();if(context){context->ClearState();context->Flush();context->Release();context=nullptr;}if(device){device->Release();device=nullptr;}}
 void open(int adapterIndex,unsigned w,unsigned h,bool alpha=false,VrBridge* parent=nullptr){
  // Check the owner before the size fast path: a menu may have opened before the world.
  if(device&&parent&&parent->device&&device!=parent->device)close();
  if(device&&width==w&&height==h&&alphaTexture==alpha)return;
  // Keep one D3D11 device for all compositor textures, including size changes.
  releaseTextures();
  if(adapterIndex<0||w<64||h<64||w>4096||h>4096)throw std::runtime_error("Invalid bridge adapter/size");
  alphaTexture=alpha;
  if(!device){
  if(parent&&parent->device){device=parent->device;context=parent->context;device->AddRef();context->AddRef();}else{
  IDXGIFactory1* factory=nullptr;IDXGIAdapter* adapter=nullptr;
  // OpenVR requires DXGI 1.1 sharing semantics (Submit error 106 with DXGI 1.0).
  check(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory)),"DXGI 1.1 factory");
  HRESULT r=factory->EnumAdapters(adapterIndex,&adapter);factory->Release();check(r,"VR adapter enumeration");
  D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0},selected;
  r=D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,3,D3D11_SDK_VERSION,&device,&selected,&context);adapter->Release();check(r,"D3D11 VR device");
  }
  }
  D3D11_TEXTURE2D_DESC d={};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.Format=directBgrx&&!alphaTexture?DXGI_FORMAT_B8G8R8X8_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
  UINT support=0;check(device->CheckFormatSupport(d.Format,&support),"Eye format support");
  const UINT required=D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE|D3D11_FORMAT_SUPPORT_RENDER_TARGET;
  if((support&required)!=required)throw std::runtime_error("Eye texture format lacks required device support");
  for(unsigned eye=0;eye<(alphaTexture?1u:2u);++eye)check(device->CreateTexture2D(&d,nullptr,&textures[eye]),"D3D11 eye texture");

  // Menus must never request a contiguous full-frame CPU copy in this x86
  // process. Allocate before marking this size ready so a failed open retries.
  if(alphaTexture)pixels.resize(std::size_t(w)*std::min(h,verificationRows)*4);
  else if(!directBgrx)pixels.resize(std::size_t(w)*h*4);
  width=w;height=h;++generation;
 }
 HRESULT verificationSurface(){
  if(staging)return S_OK;
#ifdef HP1VR_BRIDGE_TEST
  ++testCreateCalls;if(FAILED(testVerificationCreate))return testVerificationCreate;
#endif
  D3D11_TEXTURE2D_DESC d={};textures[0]->GetDesc(&d);d.Height=std::min(height,verificationRows);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  return device->CreateTexture2D(&d,nullptr,&staging);
 }
 void releaseVerification(){if(staging){staging->Release();staging=nullptr;}}
 void releaseCpu(){if(directBgrx&&!alphaTexture)std::vector<unsigned char>().swap(pixels);}
 bool verificationResult(HRESULT result,unsigned eye,const char* stage){
  if(result==E_OUTOFMEMORY){
   // This optional readback runs AFTER a successful upload. Do not mark it as
   // verified, retry every frame, or treat other/device-loss errors as benign.
   verificationSkippedMask|=1u<<eye;releaseVerification();
   log("{\"event\":\"vr_verification_skipped\",\"stage\":\"%s\",\"hresult\":%ld,\"eye\":%u,\"width\":%u,\"height\":%u,\"generation\":%u,\"upload_preserved\":true}",stage,long(result),eye,width,height,generation);
   return false;
  }
  check(result,stage);return true;
 }
 template<class Rows> void verifyTextureRows(unsigned eye,std::size_t pitch,bool rgbOnly,Rows expectedRows){
  if((verifiedMask|verificationSkippedMask)&(1u<<eye))return;
  VrStageTimer measured(VrProfile::Verify);
  try{
   if(!verificationResult(verificationSurface(),eye,"Verification surface"))return;
   for(unsigned y=0;y<height;y+=verificationRows){
    unsigned rows=std::min(verificationRows,height-y);D3D11_BOX box={0,y,0,width,y+rows,1};
    const unsigned char* expected=expectedRows(y,rows);
    context->CopySubresourceRegion(staging,0,0,0,0,textures[eye],0,&box);
    D3D11_MAPPED_SUBRESOURCE map={};HRESULT result;
#ifdef HP1VR_BRIDGE_TEST
    result=FAILED(testVerificationMap)?testVerificationMap:context->Map(staging,0,D3D11_MAP_READ,0,&map);
#else
    result=context->Map(staging,0,D3D11_MAP_READ,0,&map);
#endif
    if(!verificationResult(result,eye,"Map GPU verification strip"))return;
    bool equal=true;
    for(unsigned row=0;row<rows&&equal;++row){
     auto src=expected+std::size_t(row)*pitch;auto actual=static_cast<const unsigned char*>(map.pData)+std::size_t(row)*map.RowPitch;
     if(!rgbOnly)equal=!std::memcmp(src,actual,width*4);
     else for(unsigned x=0;x<width&&equal;++x)equal=src[x*4]==actual[x*4]&&src[x*4+1]==actual[x*4+1]&&src[x*4+2]==actual[x*4+2];
    }
    context->Unmap(staging,0);
    if(!equal)throw std::runtime_error("GPU texture differs from uploaded game pixels");
   }
   verifiedMask|=1u<<eye;releaseVerification();
  }catch(...){releaseVerification();throw;}
 }
 void verifyTexture(unsigned eye,const unsigned char* expected,std::size_t pitch,bool rgbOnly){
  verifyTextureRows(eye,pitch,rgbOnly,[&](unsigned y,unsigned){return expected+std::size_t(y)*pitch;});
 }
 void upload(unsigned eye,const DDSURFACEDESC2& d,bool verify,bool capture){
  if(eye>1||!device||!d.lpSurface||d.dwWidth!=width||d.dwHeight!=height||d.lPitch<LONG(width*4)||d.ddpfPixelFormat.dwRGBBitCount!=32||d.ddpfPixelFormat.dwRBitMask!=0x00ff0000||d.ddpfPixelFormat.dwGBitMask!=0x0000ff00||d.ddpfPixelFormat.dwBBitMask!=0x000000ff)throw std::runtime_error("Unreviewed D3D7 pixel layout");
  if(!directBgrx||capture){pixels.resize(width*height*4);VrStageTimer measured(VrProfile::CpuPack);copyOpaqueBGRA(pixels.data(),static_cast<const unsigned char*>(d.lpSurface),width,height,d.lPitch);}
  // UpdateSubresource snapshots pSrcData before returning; the D3D7 lock stays held through this call.
  {VrStageTimer measured(VrProfile::Upload);context->UpdateSubresource(textures[eye],0,nullptr,directBgrx?d.lpSurface:pixels.data(),directBgrx?UINT(d.lPitch):width*4,0);}
  if(verify)verifyTexture(eye,directBgrx?static_cast<const unsigned char*>(d.lpSurface):pixels.data(),directBgrx?std::size_t(d.lPitch):width*4,directBgrx);
  if(!capture)releaseCpu();
 }
 unsigned uploadArgb(const DDSURFACEDESC2& d,bool verify,bool sourceAlpha=true){
  if(!alphaTexture||!device||!d.lpSurface||d.dwWidth!=width||d.dwHeight!=height||d.lPitch<LONG(width*4)||d.ddpfPixelFormat.dwRGBBitCount!=32||d.ddpfPixelFormat.dwRBitMask!=0x00ff0000||d.ddpfPixelFormat.dwGBitMask!=0x0000ff00||d.ddpfPixelFormat.dwBBitMask!=0x000000ff||(sourceAlpha&&d.ddpfPixelFormat.dwRGBAlphaBitMask!=0xff000000))throw std::runtime_error("Unsupported HUD ARGB layout");
  unsigned visible=0;
  auto pack=[&](unsigned y,unsigned rows){return copyHudBGRA(pixels.data(),static_cast<const unsigned char*>(d.lpSurface)+std::size_t(y)*d.lPitch,width,rows,d.lPitch,sourceAlpha);};
  for(unsigned y=0;y<height;y+=verificationRows){
   unsigned rows=std::min(verificationRows,height-y);visible+=pack(y,rows);
   D3D11_BOX box={0,y,0,width,y+rows,1};context->UpdateSubresource(textures[0],0,&box,pixels.data(),width*4,0);
  }
  // All rows are uploaded before optional verification: a readback OOM must
  // never leave the lower part of the HUD missing. Repack only while checking.
  if(verify)verifyTextureRows(0,width*4,false,[&](unsigned y,unsigned rows){pack(y,rows);return pixels.data();});
  return visible;
 }
 void flush(){if(context)context->Flush();}
};
