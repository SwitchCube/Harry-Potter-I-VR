#pragma once
#include "hp1_cinema_math.h"
// A separately named, unticked copy of the native entrance map. Render only;
// never travel, teleport the real pawn, run its actors or change a save.
static void* lobbyLevel=nullptr;
static hpvr::Origin lobbyOrigin;
static bool lobbyWasVisible=false;
static Vec lobbyAnchor={};
static float lobbyHeading=0;
struct Hp1PointRegion{void* zone;int leaf;DWORD zoneNumber;};
static Hp1PointRegion lobbyRegion(Vec point){
 // Reviewed HP1 UModel::PointRegion ABI: this=model, hidden result, ZoneInfo, Vec.
 // DrawFrame also indexes Model.Leaves using Viewport.Actor.Region.iLeaf;
 // changing XLevel alone leaves a potentially out-of-bounds index from gameplay.
 using PointRegion=Hp1PointRegion*(__thiscall*)(void*,Hp1PointRegion*,void*,Vec);
 static auto query=reinterpret_cast<PointRegion>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?PointRegion@UModel@@QBE?AUFPointRegion@@PAVAZoneInfo@@VFVector@@@Z"));
 need(query&&lobbyLevel,"HP1 entrance region API missing");Hp1PointRegion region={};Fpu fpu;fpu.restore();
 void* model=mem<void*>(lobbyLevel,0x98);query(model,&region,mem<void*>(mem<void*>(lobbyLevel,0x2c),0),point);
 need(region.zone&&region.leaf>=-1&&region.leaf<mem<int>(model,0xdc),"HP1 entrance leaf outside model");return region;
}
static bool flatSceneWanted(){
 if(vrPreferences.spatial||!player||menuVisible||inLesson()||chessActive()||letterBusy)return false;
 auto hud=objectField(player,L"myHUD");return hud&&flag(hud,L"bCutSceneMode");
}
static hpvr::Mat lobbyScreenPose(float distance){return hp1::cinemaPlane(lobbyOrigin,lobbyHeading,distance);}
static bool lobbyWanted(){
 if(flatSceneWanted())return true;
 if(!menuConsole)return false;
 void* book=objectField(menuConsole,L"MenuBook");if(!book||!flag(book,L"bIsOpen"))return false;
 void* page=objectField(book,L"curPage");
 return page&&(page==objectField(book,L"MainPage")||page==objectField(book,L"SlotPage")||page==objectField(book,L"StoryBookPage")||flag(book,L"bInEndGame")||!flag(book,L"bGamePlaying"));
}
static void lobbyTick(){
 if((startup.mode!=3&&startup.mode!=6)||lobbyLevel||!lobbyWanted())return;
 Fpu fpu;
 auto engine=GetModuleHandleW(L"Engine.dll"),core=GetModuleHandleW(L"Core.dll");
 using Class=void*(__cdecl*)();using Load=void*(__cdecl*)(void*,void*,const wchar_t*,const wchar_t*,DWORD,void*);using Root=void(__thiscall*)(void*);
 auto cls=reinterpret_cast<Class>(GetProcAddress(engine,"?StaticClass@ULevel@@SAPAVUClass@@XZ"));
 auto load=reinterpret_cast<Load>(GetProcAddress(core,"?StaticLoadObject@UObject@@SAPAV1@PAVUClass@@PAV1@PBG2KPAVUPackageMap@@@Z"));
 auto root=reinterpret_cast<Root>(GetProcAddress(core,"?AddToRoot@UObject@@QAEXXZ"));
 need(cls&&load&&root,"HP1 lobby loading API missing");
 auto path=std::wstring(startup.root)+L"\\cache\\lobby\\HP1VRLobby.unr";
 need(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES,"HP1 entrance background was not prepared");
 fpu.restore();lobbyLevel=load(cls(),nullptr,L"HP1VRLobby.MyLevel",path.c_str(),0,nullptr);
 need(lobbyLevel&&name(lobbyLevel)==L"MyLevel"&&lobbyLevel!=mem<void*>(player,0x80),"Independent entrance level missing");
 root(lobbyLevel);need(mem<void*>(lobbyLevel,0x98)&&mem<void*>(lobbyLevel,0x2c),"Entrance model/actors missing");
 void* start=nullptr;void** actors=mem<void**>(lobbyLevel,0x2c);int count=mem<int>(lobbyLevel,0x30);
 need(count>0&&count<100000,"Entrance actor count differs");
 for(int i=0;i<count;++i)if(actors[i]&&name(actors[i])==L"PlayerStart0"&&name(mem<void*>(actors[i],0x24))==L"PlayerStart"){start=actors[i];break;}
 need(start,"Entrance PlayerStart missing");
 lobbyAnchor=mem<Vec>(start,field(start,L"Location",12,L"StructProperty"));lobbyAnchor.z+=28;
 lobbyHeading=mem<Rot>(start,field(start,L"Rotation",12,L"StructProperty")).yaw*hpvr::pi/32768;
 log("{\"event\":\"hp1_lobby_loaded\",\"independent_map\":true,\"ticked\":false}");fpu.restore();
}
static void closeLobby(){
 if(lobbyLevel){using Root=void(__thiscall*)(void*);auto remove=reinterpret_cast<Root>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"?RemoveFromRoot@UObject@@QAEXXZ"));if(remove)remove(lobbyLevel);lobbyLevel=nullptr;}
 lobbyWasVisible=false;lobbyOrigin.ready=false;
}
