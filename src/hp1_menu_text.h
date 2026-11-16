#pragma once
// HP1 scales widget coordinates but leaves bitmap font metrics unscaled at
// GUIScale 3.2. Replace text ONLY during our private menu capture, preserving
// localized strings, clipping, widget layout and original click handling.
struct MenuPlaneColor {float r,g,b,a;};
using MenuWrappedPrint=void(__cdecl*)(void*,int,int&,int&,void*,int,const wchar_t*);
using MenuDrawString=int(__cdecl*)(DWORD,void*,void*,int,int,const wchar_t*,MenuPlaneColor,int,int);
static MenuWrappedPrint originalMenuPrint=nullptr;
static MenuDrawString originalMenuString=nullptr;
#include "hp1_menu_raster.h"
struct Hp1MenuText : Hp1MenuRaster {
 bool active=false;float scale=1,labelEnd=-1;
 std::map<void*,int> heights;
 void close(){active=false;heights.clear();Hp1MenuRaster::close();}
 bool begin(unsigned w,unsigned h,unsigned failure=0){
  if(width!=w||height!=h)close();
  active=false;if(!open(w,h,failure))return false;
  commands.clear();occluders.clear();scale=float(w)/640;active=true;return true;
 }
 void cover(void* window,void* canvas){
  float x=mem<float>(canvas,field(canvas,L"OrgX",4,L"FloatProperty")),y=mem<float>(canvas,field(canvas,L"OrgY",4,L"FloatProperty"));
  float w=mem<float>(window,field(window,L"WinWidth",4,L"FloatProperty"))*scale,h=mem<float>(window,field(window,L"WinHeight",4,L"FloatProperty"))*scale;
  occluders.push_back({commands.size(),{LONG(x),LONG(y),LONG(std::ceil(x+w)),LONG(std::ceil(y+h))}});
 }
 int fontSize(void* canvas,void* font){
  auto found=heights.find(font);int h=0;if(found!=heights.end())h=found->second;else {
   RestoreFields restore;for(auto key:{L"CurX",L"CurY"})restore.set(canvas,field(canvas,key,4,L"FloatProperty"),0.0f);
   restore.set(canvas,field(canvas,L"ClipX",4,L"FloatProperty"),10000.0f);
   int w=0;originalMenuPrint(canvas,0,w,h,font,0,L"Mg");h=std::max(9,std::min(h,36));heights[font]=h;
  }
  int size=int(std::lround(h*scale));SelectObject(dc,Hp1MenuRaster::font(size));return size;
 }
 void add(void* canvas,const wchar_t* value,int x,int y,int size,COLORREF color,unsigned flags,int maxWidth){
  if(!value||!value[0]||commands.size()>1024)return;
  float ox=mem<float>(canvas,field(canvas,L"OrgX",4,L"FloatProperty")),oy=mem<float>(canvas,field(canvas,L"OrgY",4,L"FloatProperty"));
  RECT clip={LONG(std::ceil(ox)),LONG(std::ceil(oy)),LONG(std::floor(ox+mem<float>(canvas,field(canvas,L"ClipX",4,L"FloatProperty")))),LONG(std::floor(oy+mem<float>(canvas,field(canvas,L"ClipY",4,L"FloatProperty"))))};
  auto rect=measure(value,size,maxWidth,flags);commands.push_back({value,int(ox)+x,int(oy)+y,rect.right,rect.bottom,size,clip,color,flags});
 }
 void composite(DDSURFACEDESC2& target){
  if(!dc)return;
  need(target.dwWidth>=width&&target.dwHeight>=height&&target.lPitch>=LONG(width*4)&&VrHudCapture::rgb32(target),"Menu text surface layout");
  compositePixels(static_cast<BYTE*>(target.lpSurface),unsigned(target.lPitch));
 }

};
static Hp1MenuText menuText;
static void __cdecl menuPrintHook(void* canvas,int style,int& w,int& h,void* font,int cr,const wchar_t* value){
 if(!menuText.active){originalMenuPrint(canvas,style,w,h,font,cr,value);return;}
 int size=menuText.fontSize(canvas,font);float x=mem<float>(canvas,field(canvas,L"CurX",4,L"FloatProperty")),y=mem<float>(canvas,field(canvas,L"CurY",4,L"FloatProperty"));
 int limit=std::max(1,int(mem<float>(canvas,field(canvas,L"ClipX",4,L"FloatProperty"))-x));unsigned flags=DT_NOPREFIX|DT_WORDBREAK;
 auto r=menuText.measure(value,size,limit,flags);w=r.right;h=r.bottom;
 if(style){DWORD color=mem<DWORD>(canvas,field(canvas,L"DrawColor",4,L"StructProperty"));menuText.add(canvas,value,int(x),int(y),size,RGB(color&255,(color>>8)&255,(color>>16)&255),flags,limit);}
 // TextSize uses style=0 and restores the caller's cursor itself.
 if(cr){mem<float>(canvas,field(canvas,L"CurX",4,L"FloatProperty"))=0;mem<float>(canvas,field(canvas,L"CurY",4,L"FloatProperty"))=y+h;}else mem<float>(canvas,field(canvas,L"CurX",4,L"FloatProperty"))=x+w;
}
static int __cdecl menuStringHook(DWORD flags,void* canvas,void* font,int x,int y,const wchar_t* text,MenuPlaneColor color,int clip,int hotkey){
 if(!menuText.active)return originalMenuString(flags,canvas,font,x,y,text,color,clip,hotkey);
 int size=menuText.fontSize(canvas,font);unsigned format=DT_SINGLELINE|(hotkey?0:DT_NOPREFIX);auto r=menuText.measure(text,size,32767,format);
 // Preserve long localized labels inside their ORIGINAL clickable widgets.
 int available=std::max(1,int(mem<float>(canvas,field(canvas,L"ClipX",4,L"FloatProperty"))));
 if(menuText.labelEnd>=0)available=std::max(1,std::min(available,int(menuText.labelEnd-mem<float>(canvas,field(canvas,L"OrgX",4,L"FloatProperty")))));
 x=std::max(0,x);int room=std::max(1,available-x);
 if(r.right>room){size=std::max(12,int(size*float(room)/r.right));r=menuText.measure(text,size,32767,format);}
 auto component=[](float f){return BYTE(std::max(0.0f,std::min(255.0f,f*255)));};menuText.add(canvas,text,x,y,size,RGB(component(color.r),component(color.g),component(color.b)),format,32767);return r.right;
}
