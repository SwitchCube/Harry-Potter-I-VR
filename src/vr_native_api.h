#pragma once
#include <windows.h>
struct NativeStartup {
    DWORD size,version,mode; // 1=A/A, 2=A/B, 3=VR, 4=stereo replay, 5=input replay, 6=stereo/input replay
    wchar_t root[260],profile[260];
};
