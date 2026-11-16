#pragma once
// Private native-gameplay fixture only. Placement uses the original FarMoveActor
// API behind SetLocation; subsequent collision, mounts and animations are stock.
static bool climbStarted=false,climbDone=false,climbCaptureStart=false,climbCaptureEnd=false;
static bool climbReplayTick(bool cinema){
    if(!climbFixture())return false;
    static ULONGLONG start=0,next=0;static Vec initial={};static int yaw=0;static float jumpAt=-1;static bool jumped=false;
    controls={};
    if(cinema)return true;
    if(!climbStarted){
        if(state(player)!=L"PlayerWalking"||flag(player,L"bKeepStationary"))return true;
        FILE* f=_wfopen((std::wstring(startup.profile)+L"\\test-climb.flag").c_str(),L"r");
        need(f!=nullptr,"Open private climb fixture");int count=std::fscanf(f,"%f %f %f %d %f",&initial.x,&initial.y,&initial.z,&yaw,&jumpAt);std::fclose(f);need(count==5,"Invalid private climb fixture");
        using GetLevel=void*(__thiscall*)(void*);using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);
        auto engine=GetModuleHandleW(L"Engine.dll");
        auto getLevel=reinterpret_cast<GetLevel>(GetProcAddress(engine,"?GetLevel@AActor@@QBEPAVULevel@@XZ"));
        auto move=reinterpret_cast<FarMove>(GetProcAddress(engine,"?FarMoveActor@ULevel@@UAEHPAVAActor@@VFVector@@HH@Z"));
        need(getLevel&&move,"Missing HP1 fixture placement API");
        need(move(getLevel(player),player,initial,0,0)!=0,"Climb fixture placement blocked");
        void* cam=objectField(player,L"cam");
        if(cam){Vec cp=hpvr::add(initial,hpvr::yaw({-80,0,100},yaw*hpvr::pi/32768));need(move(getLevel(cam),cam,cp,0,0)!=0,"Climb fixture camera placement blocked");}
        mem<Vec>(player,field(player,L"Velocity",12,L"StructProperty"))={};
        mem<Vec>(player,field(player,L"Acceleration",12,L"StructProperty"))={};
        for(auto key:{L"Rotation",L"ViewRotation",L"DesiredRotation"})mem<Rot>(player,field(player,key,12,L"StructProperty"))={0,yaw+32768,0};
        start=GetTickCount64();climbStarted=true;climbCaptureStart=true;originalMounts=0;
        log("{\"event\":\"hp1_climb_start\",\"x\":%.4f,\"y\":%.4f,\"z\":%.4f,\"head_yaw\":%d,\"initial_body_yaw\":%d,\"jump_at_seconds\":%.3f,\"max_mount_height\":%.3f}",initial.x,initial.y,initial.z,yaw,yaw+32768,jumpAt,mem<float>(player,field(player,L"MaxMountHeight",4,L"FloatProperty")));
    }
    auto elapsed=GetTickCount64()-start;
    controls.active=true;controls.yaw=yaw;controls.forward=(!climbDone&&elapsed>700)?1:0;
    if(jumpAt>=0&&!jumped&&elapsed>=ULONGLONG(jumpAt*1000)){float value=1;callScript(player,L"DoJump",&value);jumped=true;log("{\"event\":\"hp1_climb_jump\"}");}
    Vec p=mem<Vec>(player,field(player,L"Location",12,L"StructProperty"));BYTE physics=mem<BYTE>(player,field(player,L"Physics",1,L"ByteProperty"));
    if(GetTickCount64()>=next){next=GetTickCount64()+500;log("{\"event\":\"hp1_climb_tick\",\"elapsed_ms\":%llu,\"state\":\"%ls\",\"physics\":%u,\"position\":[%.4f,%.4f,%.4f],\"yaw\":%d,\"mounts\":%u}",elapsed,state(player).c_str(),physics,p.x,p.y,p.z,mem<Rot>(player,field(player,L"Rotation",12,L"StructProperty")).yaw,originalMounts);}
    if(!climbDone&&elapsed>9000){climbDone=true;climbCaptureEnd=true;controls.forward=0;log("{\"event\":\"hp1_climb_complete\",\"mounts\":%u,\"height_gain\":%.4f,\"grounded\":%s}",originalMounts,p.z-initial.z,physics==1?"true":"false");}
    return true;
}
