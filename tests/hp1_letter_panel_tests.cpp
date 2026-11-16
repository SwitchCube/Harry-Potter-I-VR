#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d.h>
#include <cwchar>
#include <cstdio>
#include <cstdarg>
#include <stdexcept>
#include <string>
static void log(const char*,...){}
#include "vr_bridge.h"
#include "vr_panel.h"
static const wchar_t* tr(const wchar_t* de,const wchar_t*){return de;}
#include "hp1_letter_panel.h"
static void need(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
int main(){try{
 Hp1LetterPanel letter;letter.paint(L"Lieber Harry,\n\nkomm bitte zu meiner Huette. Gruesse, Hagrid.",0);
 need(letter.pages==1&&letter.page==0,"short letter should fit without paging");
 std::wstring longText;for(unsigned i=0;i<100;++i)longText+=L"Briefabschnitt "+std::to_wstring(i)+L": Zauberuebungen, Fluegel und ein besonderer Gruss.\n";
 letter.paint(longText,0);int pages=letter.pages;need(pages>5&&letter.pageHeight%letter.lineHeight==0,"long letters need whole-line pages");
 std::vector<BYTE> first(letter.panel.data,letter.panel.data+letter.panel.width*letter.panel.height*4);
 letter.paint(longText,1);need(letter.page==1&&std::memcmp(first.data(),letter.panel.data,first.size()),"paging must change the displayed text");
 letter.paint(longText,9999);need(letter.page==pages-1&&letter.page*letter.pageHeight<letter.textHeight&&(letter.page+1)*letter.pageHeight>=letter.textHeight,"last page coverage");
 letter.paint(longText,-99);need(letter.page==0,"negative page clamp");letter.panel.upload(0,nullptr);need(letter.panel.texture.verifiedMask==1,"letter GPU transfer");
 letter.close();puts("Letters: native Unicode paragraphs, measured whole-line paging, first/last page bounds, changed visible content and GPU transfer passed");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
