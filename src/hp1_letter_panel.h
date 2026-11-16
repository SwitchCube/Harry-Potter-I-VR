#pragma once
// Original localized letter text, laid out at readable VR texel density. Page
// boundaries are whole measured text lines, never arbitrary character cuts.
struct Hp1LetterPanel {
 VrPanel panel;int page=0,pages=1,lineHeight=0,pageHeight=0,textHeight=0;
 void close(){panel.close();page=0;pages=1;}
 void paint(const std::wstring& text,int requested){
  panel.open(1000,720);panel.box(0,0,1000,720,RGB(228,208,166));panel.box(0,0,10,720,RGB(128,84,38));
  panel.text(48,18,904,tr(L"Brief",L"Letter"),42,RGB(68,43,26));panel.box(48,90,904,2,RGB(157,123,76));
  HFONT font=CreateFontW(-36,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  auto old=SelectObject(panel.dc,font);TEXTMETRICW metrics={};GetTextMetricsW(panel.dc,&metrics);lineHeight=std::max(1,int(metrics.tmHeight));pageHeight=(504/lineHeight)*lineHeight;
  RECT measure={0,0,904,0};DrawTextW(panel.dc,text.c_str(),int(text.size()),&measure,DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);
  textHeight=measure.bottom;pages=std::max(1,(textHeight+pageHeight-1)/pageHeight);page=std::max(0,std::min(requested,pages-1));
  int saved=SaveDC(panel.dc);IntersectClipRect(panel.dc,48,104,952,104+pageHeight);SetTextColor(panel.dc,RGB(40,30,24));
  RECT body={48,104-page*pageHeight,952,104-page*pageHeight+std::max(textHeight,pageHeight)};
  DrawTextW(panel.dc,text.c_str(),int(text.size()),&body,DT_WORDBREAK|DT_NOPREFIX);RestoreDC(panel.dc,saved);SelectObject(panel.dc,old);DeleteObject(font);
  panel.box(48,624,904,2,RGB(157,123,76));
  wchar_t info[100];std::swprintf(info,100,tr(L"Seite %d / %d%ls",L"Page %d / %d%ls"),page+1,pages,pages>1?tr(L"  |  Linker Stick: bl\u00e4ttern",L"  |  Movement stick: turn page"):L"");panel.text(48,634,904,info,25,RGB(68,43,26));
  panel.text(48,673,904,tr(L"X oder rechter Trigger: schlie\u00dfen",L"Action or cast button: close"),25,RGB(68,43,26));GdiFlush();
 }
};
