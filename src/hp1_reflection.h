#pragma once
// HP1 Render.dll DrawMesh/DrawLodMesh and CreateChildFrame layouts are checked
// by prepare_target.py. Keep the actor in native visibility lists, then omit
// its mesh only from direct views. Never unhide a story-hidden actor.
using MeshDraw=void(__thiscall*)(void*,void*,void*,void*,const void*,DWORD);
static MeshDraw originalMeshDraw=nullptr,originalLodDraw=nullptr;
static bool hideDirectBody=false;
static bool reflectedFrame(void* frame){
 for(unsigned depth=0;frame&&depth<32;++depth){void* parent=mem<void*>(frame,8);if(!parent)return false;
  if(mem<float>(frame,0x20)!=mem<float>(parent,0x20))return true;frame=parent;
 }return false;
}
static bool omitDirectMesh(void* frame,void* actor){
 if(pairActive&&hideDirectBody&&actor==ownedFlute()&&!controls.hand&&!reflectedFrame(frame))return true;
 return pairActive&&hideDirectBody&&player&&(actor==player||actor==objectField(player,L"Weapon"))&&!reflectedFrame(frame);
}
static void noteMirrorPlayer(void* frame,void* actor){
 if(pairActive&&hideDirectBody&&controls.hand&&actor==ownedFlute()&&!reflectedFrame(frame)){
  unsigned bit=secondary?2:1;if(!(fluteMeshMask&bit)){fluteMeshMask|=bit;log("{\"event\":\"hp1_flute_mesh\",\"eye\":%u,\"native_mesh\":true}",secondary?1:0);}
 }
 if(!pairActive||!hideDirectBody||actor!=player||!reflectedFrame(frame))return;
 unsigned bit=secondary?2:1;mirrorCaptureMask|=bit;
 if(!(mirrorPlayerMask&bit)){mirrorPlayerMask|=bit;auto mesh=objectField(player,L"Mesh");log("{\"event\":\"hp1_mirror_player\",\"eye\":%u,\"mesh\":\"%ls\",\"native_reflection\":true}",secondary?1:0,mesh?name(mesh).c_str():L"None");}
}
static void __fastcall meshDrawHook(void* self,void*,void* frame,void* sprite,void* actor,const void* coords,DWORD flags){
 try{if(omitDirectMesh(frame,actor))return;originalMeshDraw(self,frame,sprite,actor,coords,flags);noteMirrorPlayer(frame,actor);}catch(const std::exception& e){fatal("hp1_mirror",e.what());}
}
static void __fastcall lodDrawHook(void* self,void*,void* frame,void* sprite,void* actor,const void* coords,DWORD flags){
 try{if(omitDirectMesh(frame,actor))return;originalLodDraw(self,frame,sprite,actor,coords,flags);noteMirrorPlayer(frame,actor);}catch(const std::exception& e){fatal("hp1_mirror",e.what());}
}
