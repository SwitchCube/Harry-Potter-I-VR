#pragma once
static unsigned projectileDepth=0;
static void* targetFrame=nullptr;
static Internal originalTraceActors=nullptr;
static unsigned targetRays=0;
static unsigned localField(void* frame,const wchar_t* key,unsigned size){
    void* property=findProperty(mem<void*>(frame,4),key);
    need(property&&mem<DWORD>(property,0x38)==size,"HP1 targeting local layout differs");
    unsigned off=mem<DWORD>(property,0x48);need(off<4096,"HP1 targeting local outside frame");return off;
}
static void __fastcall traceActorsHook(void* self,void*,void* frame,void* result){
    Fpu fpu;
    try{
        if(targetFrame&&frame==targetFrame&&controls.hand&&self==objectField(player,L"rectarget")){
            // Change only this script invocation's local ray. Harry's actor
            // transform/collision hash and the original targeting effects stay intact.
            void* locals=mem<void*>(frame,0x10);need(locals!=nullptr,"Missing HP1 target locals");
            Vec tip=hpvr::add(controls.wandBase,hpvr::scale(controls.wandForward,hp1::wandLength*64));
            float reach=flag(player,L"bExtendedTargetting")?1024.0f:512.0f;
            mem<Vec>(locals,localField(frame,L"TraceStart",12))=tip;
            mem<Vec>(locals,localField(frame,L"TraceEnd",12))=hpvr::add(tip,hpvr::scale(controls.wandForward,reach));
            mem<Vec>(locals,localField(frame,L"Cushion",12))=controls.wandForward;
            mem<Rot>(locals,localField(frame,L"TargetRot",12))=controls.wandRotation;
            if(++targetRays<=2)log("{\"event\":\"hp1_hand_target_ray\",\"pitch\":%d,\"reach\":%.0f,\"actor_transform_changed\":false}",controls.wandRotation.pitch,reach);
        }
        fpu.restore();originalTraceActors(self,frame,result);
    }catch(const std::exception& e){fatal("hp1_target_ray",e.what());}
}
struct RestoreFields {
    struct Item{void* object;unsigned offset;std::vector<BYTE> data;};std::vector<Item> items;
    template<class T>void set(void* o,unsigned off,T value){Item i={o,off,std::vector<BYTE>(sizeof(T))};std::memcpy(i.data.data(),static_cast<BYTE*>(o)+off,sizeof(T));items.push_back(i);mem<T>(o,off)=value;}
    ~RestoreFields(){for(auto i=items.rbegin();i!=items.rend();++i)std::memcpy(static_cast<BYTE*>(i->object)+i->offset,i->data.data(),i->data.size());}
};
static bool aimInternal(void* self,void* frame,void* result){
    if((startup.mode!=3&&startup.mode!=6&&!inputReplay())||!player||!frame)return false;
    auto function=name(mem<void*>(frame,4));void* weapon=objectField(player,L"Weapon");
    auto is=[&](const wchar_t* n){return !_wcsicmp(function.c_str(),n);};
    if(self==weapon&&is(L"CastSpell")&&!controls.hand)return true;
    if(!controls.hand)return false;
    Fpu fpu;
    if(targetFrame&&self==player&&is(L"ExtendTarget")){
        // The desktop cone originates at Harry, so it can select a different
        // object than the hand ray. Keep the original exact-ray selection.
        need(result!=nullptr,"Missing HP1 target return");*static_cast<void**>(result)=nullptr;fpu.restore();return true;
    }
    if(self==player&&is(L"AdjustAim")){
        need(result!=nullptr,"Missing HP1 aim return");*static_cast<Rot*>(result)=controls.wandRotation;fpu.restore();return true;
    }
    if(self==weapon&&is(L"CalcDrawOffset")&&projectileDepth){
        need(result!=nullptr,"Missing HP1 projectile offset return");
        // HP1 adds Owner.Location afterwards. The third-person FireOffset is
        // scoped to zero during ProjectileFire, avoiding a second hand offset.
        Vec tip=hpvr::add(controls.wandBase,hpvr::scale(controls.wandForward,hp1::wandLength*64));
        *static_cast<Vec*>(result)=hpvr::sub(tip,mem<Vec>(player,field(player,L"Location",12,L"StructProperty")));fpu.restore();return true;
    }
    if(self==weapon&&is(L"ProjectileFire")){
        RestoreFields restore;restore.set(player,field(player,L"ViewRotation",12,L"StructProperty"),controls.wandRotation);
        restore.set(weapon,field(weapon,L"FireOffset",12,L"StructProperty"),Vec{});
        ++projectileDepth;fpu.restore();originalInternal(self,frame,result);--projectileDepth;fpu.restore();
        void* spawned=result?*static_cast<void**>(result):nullptr;
        if(spawned&&stereoFixture())stereoFixtureProjectile=spawned;
        float originError=-1;
        if(spawned){Vec p=mem<Vec>(spawned,field(spawned,L"Location",12,L"StructProperty"));Vec expected=hpvr::add(controls.wandBase,hpvr::scale(controls.wandForward,hp1::wandLength*64));Vec d=hpvr::sub(p,expected);originError=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
        Rot actual=spawned?mem<Rot>(spawned,field(spawned,L"Rotation",12,L"StructProperty")):Rot{};
        int pitchError=short(actual.pitch-controls.wandRotation.pitch),yawError=short(actual.yaw-controls.wandRotation.yaw);
        log("{\"event\":\"hp1_controller_spell\",\"spawned\":%s,\"tip_origin_error\":%.5f,\"pitch_error\":%d,\"yaw_error\":%d}",spawned?"true":"false",originError,pitchError,yawError);return true;
    }
    if(!_wcsicmp(function.c_str(),L"setTarget")&&self==objectField(player,L"rectarget")){
        RestoreFields restore;
        restore.set(player,field(player,L"Rotation",12,L"StructProperty"),controls.wandRotation);
        for(auto key:{L"SmoothMouseX",L"SmoothMouseY",L"aForMove"})restore.set(player,field(player,key,4,L"FloatProperty"),0.0f);
        mem<int>(self,field(self,L"TargetPitch",4,L"IntProperty"))=controls.wandRotation.pitch;
        mem<int>(self,field(self,L"TargetYaw",4,L"IntProperty"))=0;
        void* previous=targetFrame;targetFrame=frame;
        fpu.restore();originalInternal(self,frame,result);targetFrame=previous;fpu.restore();static unsigned samples=0;
        if(++samples<=2)log("{\"event\":\"hp1_controller_target\",\"pitch\":%d,\"yaw\":%d}",controls.wandRotation.pitch,controls.wandRotation.yaw);return true;
    }
    fpu.restore();return false;
}
