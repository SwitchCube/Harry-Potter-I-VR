#pragma once
#include "vr_wand_pose.h"
#include "hp1_action_list.h"
struct Hp1Actions {
    VRActionSetHandle_t set=0;
    std::map<std::string,VRActionHandle_t> handles;
    bool turned=false,recentered=false;
    bool raw[6]={};
    VRActionHandle_t mappedHandle(const char* key){return handles.at(vrPreferences.mapped(key));}
    static void check(EVRInputError e,const char* operation,const char* key=""){
        if(e==EVRInputError_VRInputError_None)return;
        log("{\"event\":\"hp1_input_error\",\"operation\":\"%s\",\"action\":\"%s\",\"code\":%d}",operation,key,int(e));
        throw std::runtime_error(std::string("HP1 SteamVR input: ")+operation+" "+key+" (code "+std::to_string(int(e))+")");
    }
    void open(){
        std::wstring path=std::wstring(startup.root)+L"\\config\\vr\\actions.json";
        int size=WideCharToMultiByte(CP_UTF8,0,path.c_str(),-1,nullptr,0,nullptr,nullptr);std::vector<char> utf8(size);WideCharToMultiByte(CP_UTF8,0,path.c_str(),-1,utf8.data(),size,nullptr,nullptr);
        check(runtime.input->SetActionManifestPath(utf8.data()),"SetActionManifestPath");char group[]="/actions/game";VRActionSetHandle_t actionSet=0;check(runtime.input->GetActionSetHandle(group,&actionSet),"GetActionSetHandle",group);
        for(const auto& spec:kHp1Actions){std::string action="/actions/game/in/";action+=spec.key;VRActionHandle_t h=0;check(runtime.input->GetActionHandle(&action[0],&h),"GetActionHandle",spec.key);need(h!=0,"HP1 SteamVR returned an empty action handle");handles[spec.key]=h;}
        set=actionSet;log("{\"event\":\"hp1_actions_ready\",\"actions\":%u}",unsigned(handles.size()));
    }
    bool button(const char* key){InputDigitalActionData_t data={};check(runtime.input->GetDigitalActionData(mappedHandle(key),&data,sizeof(data),0),"GetDigitalActionData",key);return data.bActive&&data.bState;}
    void poll(const hpvr::Mat& hmd,Vec anchor,bool cinema){
        controls={};
        if(!runtime.compositor->CanRenderScene())return;
        if(!set)open();VRActiveActionSet_t s={};s.ulActionSet=set;check(runtime.input->UpdateActionState(&s,sizeof(s),1),"UpdateActionState");
        for(int i=0;i<6;++i){InputDigitalActionData_t d={};check(runtime.input->GetDigitalActionData(handles.at(Hp1Preferences::keys[i]),&d,sizeof(d),0),"Read remapping source");raw[i]=d.bActive&&d.bState;}
        InputAnalogActionData_t move={},turn={};check(runtime.input->GetAnalogActionData(mappedHandle("move"),&move,sizeof(move),0),"GetAnalogActionData","move");check(runtime.input->GetAnalogActionData(mappedHandle("turn"),&turn,sizeof(turn),0),"GetAnalogActionData","turn");
        controls.active=move.bActive;controls.forward=move.y;controls.side=move.x;
        if(flightKind()){controls.forward=hp1::flightStick(controls.forward);controls.side=hp1::flightStick(controls.side);}
        else {float length=std::hypot(controls.forward,controls.side);
        if(length<.2f)controls.forward=controls.side=0;else{float scale=(std::min(length,1.0f)-.2f)/(.8f*length);controls.forward*=scale;controls.side*=scale;}}
        controls.cast=button("cast");controls.jump=button("jump");controls.menu=button("menu");controls.skip=button("confirm");controls.recenter=button("recenter");
        controls.turn=turn.x;
        if(menuVisible){controls.forward=controls.side=0;controls.cast=raw[0];controls.menu=raw[2];controls.skip=raw[3];}
        if(turn.bActive&&!cinema&&!broomPlayer()&&!menuVisible&&!letterBusy){if(std::fabs(turn.x)<.3f)turned=false;else if(!turned&&std::fabs(turn.x)>.7f){hpvr::turnAtHead(trackingOrigin,hmd,(turn.x>0?1:-1)*hpvr::pi/4);turned=true;}}
        if(controls.recenter&&!recentered&&!cinema)trackingOrigin.recenter(hmd,hpvr::rotation(hmd,trackingOrigin.baseYaw).yaw*hpvr::pi/32768);
        recentered=controls.recenter;
        controls.yaw=hpvr::rotation(hmd,trackingOrigin.baseYaw).yaw;
        InputPoseActionData_t hand={};check(runtime.input->GetPoseActionDataRelativeToNow(handles.at("right_grip"),ETrackingUniverseOrigin_TrackingUniverseStanding,0,&hand,sizeof(hand),0),"GetPoseActionDataRelativeToNow","right_grip");
        if(hand.bActive&&hand.pose.bPoseIsValid&&hand.pose.bDeviceIsConnected){
            hpvr::Mat grip={};std::memcpy(&grip,&hand.pose.mDeviceToAbsoluteTracking,sizeof(grip));
            auto pose=hp1::wandPose(grip);controls.physicalWand=pose;
            controls.hand=true;controls.wandBase=trackingOrigin.position(pose,anchor,64);controls.wandRotation=hpvr::rotation(pose,trackingOrigin.baseYaw);
            controls.wandForward=hpvr::yaw(hpvr::convert(hpvr::scale(hpvr::axis(pose,2),-1)),trackingOrigin.baseYaw);controls.wandUp=hpvr::yaw(hpvr::convert(hpvr::axis(pose,1)),trackingOrigin.baseYaw);
        }
        controls.showStatus=button("show_health");
        InputPoseActionData_t left={};check(runtime.input->GetPoseActionDataRelativeToNow(handles.at("left_grip"),ETrackingUniverseOrigin_TrackingUniverseStanding,0,&left,sizeof(left),0),"GetPoseActionDataRelativeToNow","left_grip");
        if(left.bActive&&left.pose.bPoseIsValid&&left.pose.bDeviceIsConnected){controls.leftValid=true;std::memcpy(&controls.leftGrip,&left.pose.mDeviceToAbsoluteTracking,sizeof(controls.leftGrip));}
        InputPoseActionData_t pointer={};check(runtime.input->GetPoseActionDataRelativeToNow(handles.at("right_hand"),ETrackingUniverseOrigin_TrackingUniverseStanding,0,&pointer,sizeof(pointer),0),"GetPoseActionDataRelativeToNow","right_hand");
        if(pointer.bActive&&pointer.pose.bPoseIsValid&&pointer.pose.bDeviceIsConnected){controls.pointerValid=true;std::memcpy(&controls.pointer,&pointer.pose.mDeviceToAbsoluteTracking,sizeof(controls.pointer));}
    }
};
static Hp1Actions actions;
