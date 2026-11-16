#pragma once
// A small stereo flight-path cue. It shows where the actual velocity leads;
// neither head direction nor the next hoop pulls the broom toward the cue.
static void drawFlightPath(IDirect3DDevice7* device,void* frame,unsigned eye){
 if(!flightInteractive()||state(player)!=L"PlayerWalking"||menuVisible)return;
 Vec location=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));
 Vec velocity=mem<Vec>(player,field(player,L"Velocity",12,L"StructProperty")),targetPoint={};
 Rot body=mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty"));
 void* target=objectField(player,L"LookForTarget");bool hasTarget=target&&!flag(target,L"bHidden")&&!flag(target,L"bDeleteMe");
 if(hasTarget)targetPoint=mem<Vec>(target,field(target,L"Location",12,L"StructProperty"));
 Vec world=hp1::flightPathPoint(location,velocity,body,hasTarget?&targetPoint:nullptr);
 auto p=hpvr::sub(world,mem<Vec>(frame,0x34));float z=hpvr::dot(p,mem<Vec>(frame,0x58));if(z<=1)return;
 float w=float(mem<DWORD>(frame,0xa8)),h=float(mem<DWORD>(frame,0xac)),f=mem<float>(frame,0xdc);
 float x=w*.5f+f*hpvr::dot(p,mem<Vec>(frame,0x40))/z,y=h*.5f+f*hpvr::dot(p,mem<Vec>(frame,0x4c))/z;
 if(x<0||x>w||y<0||y>h)return;
 struct V{float x,y,z,w;DWORD color;};V vertices[256];unsigned count=0;float scale=w/2048;
 auto tri=[&](float ax,float ay,float bx,float by,float cx,float cy,DWORD color){for(auto a:{V{x+ax*scale,y+ay*scale,0,1,color},V{x+bx*scale,y+by*scale,0,1,color},V{x+cx*scale,y+cy*scale,0,1,color}})vertices[count++]=a;};
 auto ring=[&](float outer,float inner,DWORD color){for(int i=0;i<16;++i){float a=i*hpvr::pi/8,b=(i+1)*hpvr::pi/8;
  float ax=std::cos(a),ay=std::sin(a),bx=std::cos(b),by=std::sin(b);
  tri(ax*outer,ay*outer,bx*outer,by*outer,bx*inner,by*inner,color);tri(ax*outer,ay*outer,bx*inner,by*inner,ax*inner,ay*inner,color);
 }};
 auto box=[&](float l,float t,float r,float b,DWORD color){tri(l,t,r,t,r,b,color);tri(l,t,r,b,l,b,color);};
 DWORD color=controls.jump?0xffffd778:0xffe7faff;
 ring(11,5,0xff14202a);ring(9,7,color);
 box(-20,-2,-10,2,0xff14202a);box(10,-2,20,2,0xff14202a);box(-2,-20,2,-10,0xff14202a);
 box(-18,-1,-10,1,color);box(10,-1,18,1,color);box(-1,-18,1,-10,color);
 DWORD saved=0;hr(device->CreateStateBlock(D3DSBT_ALL,&saved),"Save flight cue state");
 try{
  hr(device->CaptureStateBlock(saved),"Capture flight cue state");hr(device->SetTexture(0,nullptr),"Flight cue texture");
  hr(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"Flight cue color op");hr(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE),"Flight cue color");
  hr(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"Flight cue alpha op");hr(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE),"Flight cue alpha");hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"Flight cue stages");
  for(auto s:{std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRENDERSTATE_ZENABLE,FALSE},{D3DRENDERSTATE_ZWRITEENABLE,FALSE},{D3DRENDERSTATE_ALPHABLENDENABLE,FALSE},{D3DRENDERSTATE_ALPHATESTENABLE,FALSE},{D3DRENDERSTATE_FOGENABLE,FALSE},{D3DRENDERSTATE_LIGHTING,FALSE},{D3DRENDERSTATE_CULLMODE,D3DCULL_NONE},{D3DRENDERSTATE_STENCILENABLE,FALSE},{D3DRENDERSTATE_FILLMODE,D3DFILL_SOLID}})hr(device->SetRenderState(s.first,s.second),"Flight cue state");
  hr(device->DrawPrimitive(D3DPT_TRIANGLELIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,vertices,count,0),"Flight path cue");
  hr(device->ApplyStateBlock(saved),"Restore flight cue state");hr(device->DeleteStateBlock(saved),"Release flight cue state");
 }catch(...){device->ApplyStateBlock(saved);device->DeleteStateBlock(saved);throw;}
 if(flightCapturePending)log("{\"event\":\"hp1_flight_path_cue\",\"eye\":%u,\"world\":[%.3f,%.3f,%.3f],\"screen\":[%.3f,%.3f],\"velocity_based\":true}",eye,world.x,world.y,world.z,x,y);
}
