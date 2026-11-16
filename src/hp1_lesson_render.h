#pragma once
static VrPanel lessonPanel;
static VrMenu lessonOverlay{"lesson"};
static void closeLessonPanel(){lessonOverlay.close();lessonPanel.close();}
static void drawLessonCursor(IDirect3DDevice7* device,void* frame){
 if(!lessonDrawing||!lessonCursor.valid)return;
 auto p=hpvr::sub(lessonCursor.world,mem<Vec>(frame,0x34));float z=hpvr::dot(p,mem<Vec>(frame,0x58));if(z<=1)return;
 float f=mem<float>(frame,0xdc),x=mem<DWORD>(frame,0xa8)*.5f+f*hpvr::dot(p,mem<Vec>(frame,0x40))/z,y=mem<DWORD>(frame,0xac)*.5f+f*hpvr::dot(p,mem<Vec>(frame,0x4c))/z;
 struct V{float x,y,z,w;DWORD color;};std::vector<V> v;
 auto box=[&](float l,float t,float r,float b,DWORD color){V a={x+l,y+t,0,1,color},bb={x+r,y+t,0,1,color},c={x+r,y+b,0,1,color},d={x+l,y+b,0,1,color};for(auto q:{a,bb,c,a,c,d})v.push_back(q);};
 box(-16,-4,16,4,0xff15101d);box(-4,-16,4,16,0xff15101d);
 DWORD color=controls.cast?0xffffda70:0xfff5fbff;box(-14,-2,14,2,color);box(-2,-14,2,14,color);
 DWORD saved=0;hr(device->CreateStateBlock(D3DSBT_ALL,&saved),"Save lesson cursor state");hr(device->CaptureStateBlock(saved),"Capture lesson cursor state");
 try{
  hr(device->SetTexture(0,nullptr),"Lesson cursor texture");
  hr(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"Lesson cursor color op");hr(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE),"Lesson cursor color");
  hr(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"Lesson cursor alpha op");hr(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE),"Lesson cursor alpha");hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"Lesson cursor stages");
  for(auto s:{std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRENDERSTATE_ZENABLE,FALSE},{D3DRENDERSTATE_ZWRITEENABLE,FALSE},{D3DRENDERSTATE_ALPHABLENDENABLE,FALSE},{D3DRENDERSTATE_ALPHATESTENABLE,FALSE},{D3DRENDERSTATE_FOGENABLE,FALSE},{D3DRENDERSTATE_LIGHTING,FALSE},{D3DRENDERSTATE_CULLMODE,D3DCULL_NONE},{D3DRENDERSTATE_STENCILENABLE,FALSE},{D3DRENDERSTATE_FILLMODE,D3DFILL_SOLID}})hr(device->SetRenderState(s.first,s.second),"Lesson cursor state");
  hr(device->DrawPrimitive(D3DPT_TRIANGLELIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,v.data(),v.size(),0),"Lesson cursor");
  hr(device->ApplyStateBlock(saved),"Restore lesson cursor state");hr(device->DeleteStateBlock(saved),"Release lesson cursor state");
 }catch(...){device->ApplyStateBlock(saved);device->DeleteStateBlock(saved);throw;}
}
static void updateLessonPanel(){
 if(!inLesson()||!lessonActor||!controls.showStatus||!controls.leftValid||menuVisible||!runtime.compositor||!runtime.compositor->CanRenderScene()){lessonOverlay.hide();return;}
 static ULONGLONG next=0;auto phase=state(lessonActor);
 if(GetTickCount64()>=next||!lessonPanel.dc){
  next=GetTickCount64()+200;lessonPanel.open(900,140);lessonPanel.clear();wchar_t info[160];
  float targetScore=mem<float>(player,field(player,L"LessonPass",4,L"FloatProperty"));
  if(phase==L"Draw"){
   bool started=flag(lessonActor,L"bStartedDrawing");float remaining=mem<float>(lessonActor,field(lessonActor,L"DrawTime",4,L"FloatProperty"))-mem<float>(lessonActor,field(lessonActor,L"TimeDrawn",4,L"FloatProperty"));
   lessonPanel.text(24,10,850,started?tr(L"Nachzeichnen - zum Beenden Trigger loslassen",L"Trace the symbol - release the cast button to finish"):tr(L"Auf den Anfang zeigen. Rechten Trigger halten.",L"Point at the start. Hold the cast button."),30);
   std::swprintf(info,160,tr(L"%ls  %.0f s   |   Ziel: %.0f %%",L"%ls  %.0f s   |   Target: %.0f %%"),started?tr(L"Restzeit:",L"Time left:"):tr(L"Zeit zum Zeichnen:",L"Drawing time:"),std::max(0.0f,remaining),targetScore*100);
  }else if(phase==L"Judge"){
   lessonPanel.text(24,10,850,tr(L"Auswertung deiner Zeichnung",L"Evaluating your drawing"),30);std::swprintf(info,160,tr(L"Ergebnis: %.0f %%   |   Ziel: %.0f %%",L"Score: %.0f %%   |   Target: %.0f %%"),mem<float>(player,field(player,L"LessonScore",4,L"FloatProperty"))*100,targetScore*100);
  }else {lessonPanel.text(24,10,850,tr(L"Schau dir die Zauberform an.",L"Watch the spell pattern."),30);std::swprintf(info,160,tr(L"Danach mit dem rechten Controller nachzeichnen.",L"Then trace it with your right controller."));}
  lessonPanel.text(24,73,850,info,30,RGB(239,204,118));lessonPanel.upload(runtime.adapter,&bridge);
 }
 hpvr::Mat head;std::memcpy(&head,&runtime.poses[0].mDeviceToAbsoluteTracking,sizeof(head));auto pose=hp1::statusPlane(controls.leftGrip,head);
 lessonOverlay.width=.48f;lessonOverlay.submit(runtime,lessonPanel.texture.textures[0],900,140,false,false,&pose);
}
