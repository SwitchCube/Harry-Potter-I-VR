#define WIN32_LEAN_AND_MEAN
#define HP1VR_BRIDGE_TEST 1
#include <windows.h>
#include <d3d.h>
#include <cstdio>
#include <cstdarg>
#include <stdexcept>
#include <string>
#include "allocation_ceiling.h"
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static unsigned skipped=0;
static void log(const char* format,...){++skipped;va_list args;va_start(args,format);vprintf(format,args);va_end(args);puts("");}
#include "vr_bridge.h"

struct Source {
 std::vector<unsigned char> bytes;DDSURFACEDESC2 d={};
 Source(unsigned w,unsigned h):bytes((w*4+20)*h,0xaa){
  d.dwSize=sizeof(d);d.dwWidth=w;d.dwHeight=h;d.lPitch=w*4+20;d.lpSurface=bytes.data();
  d.ddpfPixelFormat.dwRGBBitCount=32;d.ddpfPixelFormat.dwRBitMask=0xff0000;d.ddpfPixelFormat.dwGBitMask=0xff00;d.ddpfPixelFormat.dwBBitMask=0xff;d.ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){auto p=bytes.data()+y*d.lPitch+x*4;p[0]=(x+y)%251;p[1]=(x*3+y)%253;p[2]=(x+y*7)%255;p[3]=(x*11+y*13)%256;}
 }
};
static void uploaded(VrBridge& b,Source& s,bool hud=true){if(hud)b.uploadArgb(s.d,true);else for(unsigned eye=0;eye<2;++eye)b.upload(eye,s.d,true,false);}
static void expectFailure(VrBridge& b,const unsigned char* expected,std::size_t pitch,bool rgbOnly){
 bool failed=false;try{b.verifyTexture(0,expected,pitch,rgbOnly);}catch(const std::runtime_error& e){failed=std::string(e.what()).find("differs")!=std::string::npos;}
 need(failed,"corrupt GPU byte was not detected");need(!b.staging&&!b.verifiedMask,"failed verification retained staging or reported success");
}
int main(){try{
 VrBridge world;world.open(0,2048,2048);Source eye(2048,2048);uploaded(world,eye,false);
 need(world.verifiedMask==3&&!world.staging,"both 2048-square eyes must be verified");
 if(VrBridge::directBgrx)need(world.pixels.capacity()==0,"direct eye path retained a full-frame CPU verification copy");
 world.verificationSurface();D3D11_TEXTURE2D_DESC strip={};world.staging->GetDesc(&strip);
 need(strip.Width==2048&&strip.Height==32,"verification must use bounded readback strips");world.releaseVerification();
 VrBridge hud;Source small(129,75);hud.open(0,129,75,true,&world);uploaded(hud,small);
 need(hud.device==world.device&&hud.verifiedMask==1,"shared-device HUD/padded input verification");
 std::vector<unsigned char> fullHud(129*75*4);copyHudBGRA(fullHud.data(),small.bytes.data(),129,75,small.d.lPitch);
 need(hud.pixels.size()==129*32*4,"HUD retained a full-frame CPU copy");
 // Every row, including the partial final strip and strip boundaries, matters.
 for(unsigned y:{0u,31u,32u,74u}){
  auto expected=fullHud;expected[y*129*4+128*4+2]^=1;hud.resetVerification();expectFailure(hud,expected.data(),129*4,false);
 }
 auto expected=fullHud;expected.back()^=1;hud.resetVerification();expectFailure(hud,expected.data(),129*4,false);
 // X is not alpha in the direct world path, and must not cause false failures.
 VrBridge odd;odd.open(0,129,75,false,&world);uploaded(odd,small,false);
 if(VrBridge::directBgrx){auto rgb=small.bytes;rgb[74*small.d.lPitch+128*4+3]^=1;odd.resetVerification();odd.verifyTexture(0,rgb.data(),small.d.lPitch,true);need(odd.verifiedMask==1,"unused X byte rejected");}
 // Reproduce the user's exact HRESULT at both allocation and mapping. Uploads
 // remain readable, skipped checks never claim success, no per-frame retries.
 for(bool mapFailure:{false,true}){
  hud.close();hud.open(0,129,75,true,&world);hud.testVerificationCreate=mapFailure?S_OK:E_OUTOFMEMORY;hud.testVerificationMap=mapFailure?E_OUTOFMEMORY:S_OK;
  unsigned notices=skipped;uploaded(hud,small);unsigned attempts=hud.testCreateCalls;
  need(hud.verificationSkippedMask==1&&!hud.verifiedMask&&!hud.staging,"OOM must defer only the optional verification");
  for(int frame=0;frame<20;++frame)uploaded(hud,small);
  need(skipped==notices+1&&hud.testCreateCalls==attempts,"low-memory verification retried each frame");
  hud.testVerificationCreate=hud.testVerificationMap=S_OK;hud.resetVerification();hud.verifyTexture(0,fullHud.data(),129*4,false);
  need(hud.verifiedMask==1,"upload did not survive failed verification");
 }
 // Device removal is an actual rendering error and must still propagate.
 hud.resetVerification();hud.testVerificationCreate=DXGI_ERROR_DEVICE_REMOVED;
 bool failed=false;try{uploaded(hud,small);}catch(const std::runtime_error&){failed=true;}
 need(failed&&!hud.staging&&!hud.verificationSkippedMask,"device failure was incorrectly swallowed");hud.testVerificationCreate=S_OK;
 // Status height changes while gripping/levitating; reopen repeatedly without
 // duplicating the parent's D3D11 device or retaining a readback resource.
 for(unsigned n=0;n<48;++n){unsigned h=n%2?568:650;Source status(640,h);hud.open(0,640,h,true,&world);uploaded(hud,status);need(hud.device==world.device&&hud.verifiedMask==1&&!hud.staging,"resized status verification");}
 // The former 12 MiB allocation must fail with this budget, while complete
 // 2048-wide UI uploads/readbacks/reopens succeed with identical RGB and alpha.
 Source book(2048,1539);std::vector<unsigned char> bookExpected(2048*1539*4);
 for(bool alpha:{true,false}){
  unsigned visible=copyHudBGRA(bookExpected.data(),book.bytes.data(),2048,1539,book.d.lPitch,alpha);
  for(unsigned n=0;n<12;++n){hud.close();AllocationCeiling limit(2048*32*4);
   hud.open(0,2048,1539,true,&world);
   need(hud.uploadArgb(book.d,true,alpha)==visible,"strip upload visible count differs");
   need(hud.verifiedMask==1&&hud.pixels.capacity()<=2048*32*4,"large HUD buffer is unbounded");
   hud.resetVerification();hud.verifyTexture(0,bookExpected.data(),2048*4,false);
   need(hud.verifiedMask==1&&!rejectedAllocation,"bounded HUD allocation/upload failed");
  }
 }
 // A verification OOM must still upload the FINAL rows, even after source changes.
 hud.testVerificationMap=E_OUTOFMEMORY;hud.resetVerification();book.bytes[1538*book.d.lPitch+2047*4]^=13;
 hud.uploadArgb(book.d,true,false);need(hud.verificationSkippedMask==1,"expected deferred check");
 copyHudBGRA(bookExpected.data(),book.bytes.data(),2048,1539,book.d.lPitch,false);
 hud.testVerificationMap=S_OK;hud.resetVerification();hud.verifyTexture(0,bookExpected.data(),2048*4,false);
 hud.close();odd.close();world.close();
 puts("Bridge: full 2048-square stereo, padded rows, HUD alpha, partial strips, corruption, OOM creation/map continuation, no repeated retries, device loss and 48 status resizes passed");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
