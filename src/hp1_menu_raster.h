#pragma once
#include <algorithm>
#include <map>
#include <string>
#include <vector>
// Keep full-resolution glyph metrics without reserving a full-menu GDI DIB.
// A 32-row scratch bitmap needs 256 KiB at 2048 pixels, instead of 12 MiB.
struct Hp1MenuRaster {
 struct Text{std::wstring value;int x,y,w,h,size;RECT clip;COLORREF color;unsigned flags;};
 struct Occluder{size_t before;RECT rect;};
 static constexpr unsigned stripRows=32;
 unsigned width=0,height=0,rows=0;DWORD allocationError=0;
 HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ oldBitmap=nullptr;BYTE* pixels=nullptr;
 std::map<int,HFONT> fonts;std::vector<Text> commands;std::vector<Occluder> occluders;
 void close(){
  commands.clear();occluders.clear();
  if(dc&&oldBitmap&&oldBitmap!=HGDI_ERROR)SelectObject(dc,oldBitmap);
  if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);
  dc=nullptr;bitmap=nullptr;oldBitmap=nullptr;pixels=nullptr;
  for(auto& f:fonts)if(f.second)DeleteObject(f.second);fonts.clear();width=height=rows=0;
 }
 ~Hp1MenuRaster(){close();}
 // Nonzero failure is used only by explicit, isolated diagnostic fixtures.
 bool open(unsigned w,unsigned h,unsigned failure=0){
  if(dc&&width==w&&height==h)return true;
  close();allocationError=0;SetLastError(ERROR_SUCCESS);
  dc=failure==1?nullptr:CreateCompatibleDC(nullptr);
  if(dc){
   BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;
   bi.bmiHeader.biHeight=-LONG(std::min(h,stripRows));bi.bmiHeader.biPlanes=1;
   bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
   bitmap=failure==2?nullptr:CreateDIBSection(dc,&bi,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
   if(bitmap)oldBitmap=failure==3?nullptr:SelectObject(dc,bitmap);
  }
  if(!dc||!bitmap||!pixels||!oldBitmap||oldBitmap==HGDI_ERROR){
   allocationError=failure?ERROR_NOT_ENOUGH_MEMORY:GetLastError();close();return false;
  }
  width=w;height=h;rows=std::min(h,stripRows);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));return true;
 }
 HFONT font(int size){
  auto it=fonts.find(size);if(it==fonts.end())it=fonts.emplace(size,CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI")).first;
  return it->second?it->second:static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
 }
 RECT measure(const wchar_t* text,int size,int maxWidth,unsigned flags){SelectObject(dc,font(size));RECT rect={0,0,std::max(1,maxWidth),0};DrawTextW(dc,text,-1,&rect,flags|DT_CALCRECT);return rect;}
 void compositePixels(BYTE* target,unsigned pitch){
  if(!dc)return;
  for(size_t index=0;index<commands.size();++index){auto& t=commands[index];
   RECT r={std::max({0,t.x,int(t.clip.left)}),std::max({0,t.y,int(t.clip.top)}),std::min({int(width),t.x+t.w,int(t.clip.right)}),std::min({int(height),t.y+t.h,int(t.clip.bottom)})};
   if(r.right<=r.left||r.bottom<=r.top)continue;
   unsigned color[3]={GetBValue(t.color),GetGValue(t.color),GetRValue(t.color)};
   for(int top=r.top;top<r.bottom;top+=rows){int bottom=std::min(int(r.bottom),top+int(rows));
    int saved=SaveDC(dc);SetViewportOrgEx(dc,0,-top,nullptr);
    IntersectClipRect(dc,r.left,top,r.right,bottom);PatBlt(dc,r.left,top,r.right-r.left,bottom-top,BLACKNESS);
    // Clear first: covered glyphs must not leave pixels from the preceding strip.
    for(auto& cover:occluders)if(index<cover.before)ExcludeClipRect(dc,cover.rect.left,cover.rect.top,cover.rect.right,cover.rect.bottom);
    SelectObject(dc,font(t.size));RECT draw={t.x,t.y,t.x+t.w,t.y+t.h};
    DrawTextW(dc,t.value.c_str(),-1,&draw,t.flags);RestoreDC(dc,saved);GdiFlush();
    for(int y=top;y<bottom;++y)for(int x=r.left;x<r.right;++x){
     unsigned alpha=pixels[((y-top)*width+x)*4];if(!alpha)continue;
     BYTE* out=target+size_t(y)*pitch+x*4;
     for(int c=0;c<3;++c)out[c]=BYTE((color[c]*alpha+out[c]*(255-alpha)+127)/255);
     out[3]=BYTE(alpha+(out[3]*(255-alpha)+127)/255);
    }
   }
  }
 }
};
