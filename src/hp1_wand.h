#pragma once
static Vec cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static void drawHandWand(IDirect3DDevice7* device,void* frame,unsigned eye){
 if(!controls.hand)return;
 Vec wandBase=controls.wandBase,wandForward=controls.wandForward,wandUp=controls.wandUp,wandTip=hpvr::add(wandBase,hpvr::scale(wandForward,hp1::wandLength*64));
 struct Vertex{float x,y,z,rhw;DWORD color;};std::vector<Vertex> vertices;
 Vec origin=mem<Vec>(frame,0x34),right=mem<Vec>(frame,0x40),down=mem<Vec>(frame,0x4c),forward=mem<Vec>(frame,0x58);
 float halfW=mem<DWORD>(frame,0xa8)*.5f,halfH=mem<DWORD>(frame,0xac)*.5f,focal=mem<float>(frame,0xdc);
 Vec side=cross(wandUp,wandForward);
 auto vertex=[&](Vec world,DWORD colour,Vertex& out){
  Vec p=hpvr::sub(world,origin);float z=dot(p,forward);if(!std::isfinite(z)||z<=1)return false;
  float rhw=1/z;out={halfW+focal*dot(p,right)*rhw,halfH+focal*dot(p,down)*rhw,1-rhw,rhw,colour};return true;
 };
 auto triangle=[&](Vec a,Vec b,Vec c,DWORD color){Vertex va,vb,vc;if(vertex(a,color,va)&&vertex(b,color,vb)&&vertex(c,color,vc)){vertices.push_back(va);vertices.push_back(vb);vertices.push_back(vc);}};
 auto ring=[&](float distance,float radius,int i){float angle=i*hpvr::pi/4;return hpvr::add(hpvr::add(wandBase,hpvr::scale(wandForward,distance)),hpvr::scale(hpvr::add(hpvr::scale(side,std::cos(angle)),hpvr::scale(wandUp,std::sin(angle))),radius));};
 float length=hp1::wandLength*64;
 if(controls.hand)for(int band=0;band<2;++band){float start=band?length*.3f:0,end=band?length:length*.3f,r0=(band?.0045f:.007f)*64,r1=(band?.0018f:.006f)*64;
  for(int i=0;i<8;++i){DWORD shade=DWORD(18*(i%4)),color=0xff000000|((96+shade)<<16)|((48+shade/2)<<8)|(22+shade/3);
   Vec a=ring(start,r0,i),b=ring(start,r0,i+1),c=ring(end,r1,i+1),d=ring(end,r1,i);
   triangle(a,b,c,color);triangle(a,c,d,color);
  }
 }
 if(controls.cast){float radius=.005f*64;for(int i=0;i<8;++i)triangle(wandTip,ring(length,radius,i),ring(length,radius,i+1),0xff8edbff);}
 if(vertices.empty())return;
 DWORD state=0;hr(device->CreateStateBlock(D3DSBT_ALL,&state),"Save wand render state");
 try{
  hr(device->CaptureStateBlock(state),"Capture wand render state");
  hr(device->SetTexture(0,nullptr),"Wand texture");
  hr(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"Wand color op");
  hr(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE),"Wand color");
  hr(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"Wand alpha op");
  hr(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE),"Wand alpha");
  hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"Wand texture stage");
  for(auto setting:{std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRENDERSTATE_ZWRITEENABLE,FALSE},{D3DRENDERSTATE_ZFUNC,D3DCMP_LESSEQUAL},{D3DRENDERSTATE_ALPHABLENDENABLE,FALSE},{D3DRENDERSTATE_ALPHATESTENABLE,FALSE},{D3DRENDERSTATE_FOGENABLE,FALSE},{D3DRENDERSTATE_LIGHTING,FALSE},{D3DRENDERSTATE_CULLMODE,D3DCULL_NONE},{D3DRENDERSTATE_FILLMODE,D3DFILL_SOLID},{D3DRENDERSTATE_STENCILENABLE,FALSE}})hr(device->SetRenderState(setting.first,setting.second),"Wand render state");
  hr(device->DrawPrimitive(D3DPT_TRIANGLELIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,vertices.data(),DWORD(vertices.size()),0),"Draw tracked wand");
  hr(device->ApplyStateBlock(state),"Restore wand render state");hr(device->DeleteStateBlock(state),"Release wand state");state=0;
  
  if(vrFrames==0)log("{\"event\":\"wand_draw\",\"eye\":%u,\"vertices\":%u,\"base\":[%.5f,%.5f,%.5f],\"tip\":[%.5f,%.5f,%.5f]}",eye,unsigned(vertices.size()),wandBase.x,wandBase.y,wandBase.z,wandTip.x,wandTip.y,wandTip.z);
 }catch(...){if(state){device->ApplyStateBlock(state);device->DeleteStateBlock(state);}throw;}
}
