#pragma once
#include <string>
namespace hp1 {
enum class StereoEvent {Reject,Skip,Prepare,Lifecycle,DuplicateDestruction};
inline StereoEvent stereoEvent(const std::wstring& name,bool seen,bool preparation,bool nativeDestruction){
 if(preparation||nativeDestruction){
  if(name==L"Destroyed"&&seen)return StereoEvent::DuplicateDestruction;
  return StereoEvent::Lifecycle;
 }
 if(name==L"Update"||name==L"RenderOverlays"||name==L"RenderTexture")return seen?StereoEvent::Skip:StereoEvent::Prepare;
 return StereoEvent::Reject;
}
}
