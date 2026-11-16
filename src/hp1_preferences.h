#pragma once
// HP1-only preferences. Never write the original game or SteamVR bindings.
struct Hp1Preferences {
 static constexpr const char* keys[6]={"cast","jump","menu","confirm","recenter","show_health"};
 int source[6]={0,1,2,3,4,5};bool swapSticks=false,spatial=true,english=false;
 int language=0,resolution=2048;unsigned revision=0;std::wstring path,gameLanguage;
 const char* mapped(const char* key)const {
  if(!std::strcmp(key,"move"))return swapSticks?"turn":"move";
  if(!std::strcmp(key,"turn"))return swapSticks?"move":"turn";
  for(int i=0;i<6;++i)if(!std::strcmp(key,keys[i]))return keys[source[i]];return key;
 }
 void locale(){english=language==2||(language==0&&_wcsicmp(gameLanguage.c_str(),L"ger")&&_wcsicmp(gameLanguage.c_str(),L"de"));++revision;}
 void load(const wchar_t* root,const wchar_t* profile){
  path=std::wstring(root)+L"\\config\\vr-preferences.ini";
  if(GetFileAttributesW((std::wstring(profile)+L"\\test-vr-settings.flag").c_str())!=INVALID_FILE_ATTRIBUTES)path=std::wstring(profile)+L"\\vr-preferences.ini";
  wchar_t value[32]={};
  auto ini=std::wstring(profile)+L"\\HP.ini";
  GetPrivateProfileStringW(L"Engine.Engine",L"Language",L"int",value,32,ini.c_str());gameLanguage=value;
  language=int(GetPrivateProfileIntW(L"VR",L"Language",0,path.c_str()));if(language<0||language>2)language=0;
  resolution=int(GetPrivateProfileIntW(L"VR",L"RenderSize",2048,path.c_str()));if(resolution!=1024&&resolution!=1280&&resolution!=1536&&resolution!=2048)resolution=2048;
  spatial=GetPrivateProfileIntW(L"VR",L"SpatialScenes",1,path.c_str())!=0;swapSticks=GetPrivateProfileIntW(L"VR",L"SwapSticks",0,path.c_str())!=0;
  unsigned mask=0;bool valid=true;for(int i=0;i<6;++i){wchar_t key[16];swprintf(key,16,L"Action%d",i);int v=int(GetPrivateProfileIntW(L"Bindings",key,i,path.c_str()));if(v<0||v>=6||(mask&(1u<<v))){valid=false;break;}source[i]=v;mask|=1u<<v;}
  if(!valid)for(int i=0;i<6;++i)source[i]=i;locale();
 }
 void assign(int action,int physical){if(action<0||action>=6||physical<0||physical>=6)return;for(int i=0;i<6;++i)if(source[i]==physical){std::swap(source[i],source[action]);break;}}
 void save(){
  auto tmp=path+L".new";FILE* f=_wfopen(tmp.c_str(),L"wb");if(!f)throw std::runtime_error("Cannot save HP1 VR preferences");
  int n=fprintf(f,"[VR]\r\nLanguage=%d\r\nRenderSize=%d\r\nSpatialScenes=%d\r\nSwapSticks=%d\r\n[Bindings]\r\n",language,resolution,int(spatial),int(swapSticks));
  for(int i=0;i<6;++i)n=std::min(n,fprintf(f,"Action%d=%d\r\n",i,source[i]));bool ok=n>0&&fflush(f)==0;ok=fclose(f)==0&&ok;
  if(!ok||!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit HP1 VR preferences");++revision;
 }
};
static Hp1Preferences vrPreferences;
static const wchar_t* tr(const wchar_t* de,const wchar_t* en){return vrPreferences.english?en:de;}
static bool settingsOpen=false;
