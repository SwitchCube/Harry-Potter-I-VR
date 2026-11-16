#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include "hp1_menu_raster.h"
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
// Independent reference: the previous full-height bitmap renderer.
static void reference(Hp1MenuRaster& raster,std::vector<BYTE>& out,unsigned pitch){
 HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
 bi.bmiHeader.biWidth=raster.width;bi.bmiHeader.biHeight=-LONG(raster.height);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 BYTE* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);need(dc&&bitmap,"reference bitmap");
 HGDIOBJ old=SelectObject(dc,bitmap);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
 for(size_t index=0;index<raster.commands.size();++index){auto& t=raster.commands[index];
  RECT r={std::max({0,t.x,int(t.clip.left)}),std::max({0,t.y,int(t.clip.top)}),std::min({int(raster.width),t.x+t.w,int(t.clip.right)}),std::min({int(raster.height),t.y+t.h,int(t.clip.bottom)})};if(r.right<=r.left||r.bottom<=r.top)continue;
  int saved=SaveDC(dc);IntersectClipRect(dc,r.left,r.top,r.right,r.bottom);PatBlt(dc,r.left,r.top,r.right-r.left,r.bottom-r.top,BLACKNESS);
  for(auto& c:raster.occluders)if(index<c.before)ExcludeClipRect(dc,c.rect.left,c.rect.top,c.rect.right,c.rect.bottom);
  SelectObject(dc,raster.font(t.size));RECT draw={t.x,t.y,t.x+t.w,t.y+t.h};DrawTextW(dc,t.value.c_str(),-1,&draw,t.flags);RestoreDC(dc,saved);GdiFlush();
  unsigned colors[3]={GetBValue(t.color),GetGValue(t.color),GetRValue(t.color)};
  for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x){unsigned a=pixels[(y*raster.width+x)*4];if(!a)continue;BYTE* p=out.data()+y*pitch+x*4;for(int c=0;c<3;++c)p[c]=BYTE((colors[c]*a+p[c]*(255-a)+127)/255);p[3]=BYTE(a+(p[3]*(255-a)+127)/255);}
 }
 SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
}
int main(){try{
 for(unsigned w:{640u,2048u}){
  unsigned h=w*3/4,pitch=w*4+20;Hp1MenuRaster r;need(r.open(w,h),"open");
  need(r.rows==32&&r.width*r.rows*4<=262144,"scratch memory bound");
  RECT clip={0,0,LONG(w),LONG(h)};
  for(int y:{-17,13,31,32,55,121,int(h)-70}){
   auto flags=DT_WORDBREAK|DT_NOPREFIX;auto size=r.measure(L"Energie, Bohnen und Zauberspr\u00fcche\nYes / Nein - Wingardium Leviosa",43,479,flags);
   r.commands.push_back({L"Energie, Bohnen und Zauberspr\u00fcche\nYes / Nein - Wingardium Leviosa",17,y,size.right,size.bottom,43,clip,RGB(251,149,37),unsigned(flags)});
  }
  r.occluders.push_back({r.commands.size(),{55,60,400,141}});
  r.commands.push_back({L"&Ja     &Nein",66,71,300,60,47,{71,80,355,127},RGB(80,230,255),DT_SINGLELINE});
  std::vector<BYTE> actual(size_t(pitch)*h);for(size_t i=0;i<actual.size();++i)actual[i]=BYTE(i*37+11);
  auto expected=actual,original=actual;reference(r,expected,pitch);r.compositePixels(actual.data(),pitch);
  need(actual==expected,"strip raster differs from full-resolution text reference");need(actual!=original,"no glyphs drawn");
 }
 // Failure cleanup, recovery and repeated opens must not leak GDI handles.
 DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 for(unsigned n=0;n<500;++n){Hp1MenuRaster r;
  for(unsigned fail:{1u,2u,3u}){need(!r.open(2048,1536,fail),"injected allocation succeeded");need(!r.dc&&!r.bitmap&&!r.pixels&&!r.width,"partial allocation leaked");need(r.allocationError==ERROR_NOT_ENOUGH_MEMORY,"allocation error lost");}
  need(r.open(2048,1536),"reopen after failure");r.measure(L"Resume",48,600,DT_SINGLELINE);r.close();
 }
 need(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=before+1,"GDI resource leak");
 puts("Menu raster: full-resolution pixel equivalence, Unicode/multiline/modal clips, strip boundaries, 256 KiB bound, DC/DIB/selection failure recovery and 500 reopen cycles passed");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
