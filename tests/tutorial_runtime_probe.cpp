#include "vr_runtime.h"
#include <vector>
#include <fstream>
#include <iostream>
int wmain(int argc,wchar_t** argv){
 if(argc!=4)return 2;VrRuntime runtime;
 if(!runtime.start(argv[1],EVRApplicationType_VRApplication_Background)){std::cerr<<runtime.error;return 3;}
 auto api=static_cast<VR_IVRRenderModels_FnTable*>(runtime.table(IVRRenderModels_Version));
 CreateDirectoryW(argv[2],nullptr);FILE* report=_wfopen(argv[3],L"wb");if(!report)return 4;unsigned exported=0;
 for(unsigned device=1;device<64;++device){
  if(runtime.system->GetTrackedDeviceClass(device)!=ETrackedDeviceClass_TrackedDeviceClass_Controller)continue;
  ETrackedPropertyError error;char model[1024]={};runtime.system->GetStringTrackedDeviceProperty(device,ETrackedDeviceProperty_Prop_RenderModelName_String,model,sizeof(model),&error);if(error||!*model)continue;
  unsigned count=api->GetComponentCount(model);std::printf("device=%u model=%s components=%u\n",device,model,count);
  VRControllerState_t state={};runtime.system->GetControllerState(device,&state,sizeof(state));RenderModel_ControllerMode_State_t mode={};
  for(unsigned i=0;i<count&&i<64;++i){char component[128]={},part[1024]={};api->GetComponentName(model,i,component,sizeof(component));if(api->GetComponentRenderModelName(model,component,part,sizeof(part))<=1)continue;
   RenderModel_ComponentState_t pose={};if(!api->GetComponentState(model,component,&state,&mode,&pose))continue;
   RenderModel_t* mesh=nullptr;auto deadline=GetTickCount64()+5000;EVRRenderModelError err;
   do{err=api->LoadRenderModel_Async(part,&mesh);if(err==EVRRenderModelError_VRRenderModelError_Loading)Sleep(10);}while(err==EVRRenderModelError_VRRenderModelError_Loading&&GetTickCount64()<deadline);
   if(err||!mesh)continue;
   std::wstring filename=std::to_wstring(device)+L"-"+std::to_wstring(i)+L".mesh",path=std::wstring(argv[2])+L"\\"+filename;FILE* out=_wfopen(path.c_str(),L"wb");if(!out)return 5;
   fwrite(&mesh->unVertexCount,4,1,out);fwrite(&mesh->unTriangleCount,4,1,out);fwrite(mesh->rVertexData,sizeof(RenderModel_Vertex_t),mesh->unVertexCount,out);fwrite(mesh->rIndexData,2,mesh->unTriangleCount*3,out);fclose(out);
   fprintf(report,"{\"device\":%u,\"component\":\"%s\",\"file\":\"%ls\",\"vertices\":%u,\"triangles\":%u,\"transform\":[",device,component,filename.c_str(),mesh->unVertexCount,mesh->unTriangleCount);
   for(unsigned j=0;j<12;++j)fprintf(report,"%s%.9g",j?",":"",pose.mTrackingToComponentRenderModel.m[j/4][j%4]);fprintf(report,"]}\n");api->FreeRenderModel(mesh);++exported;
  }
 }
 fclose(report);std::printf("exported=%u\n",exported);return exported?0:6;
}
