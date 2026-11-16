#pragma once
#include "vr_runtime.h"
#include "vr_bridge.h"
static VrRuntime runtime;
static VrBridge bridge;
static hpvr::Origin trackingOrigin;
#include "hp1_actions.h"
#include "hp1_status.h"
#include "hp1_cinema.h"
#include "hp1_tutorial_render.h"
#include "hp1_flight_ui.h"
static hpvr::Projection eyeProjection={};
static unsigned vrFrames=0;
static unsigned submittedPairs=0;
#include "hp1_wand.h"
#include "hp1_flight_render.h"
static void* trackingLevel=nullptr;
static bool previousCinema=false;
static bool previousTroll=false;
static bool previousFlight=false,flightRecenterHeld=false;
static hpvr::Mat flightReference=hpvr::identity();
static hp1::FlightViewHeading flightViewHeading;
static ULONGLONG flightViewTime=0;
static hpvr::Mat sceneReference=hpvr::identity();
static bool capturedGameplay=false;
#include "hp1_lesson_render.h"
static void* target(const char*);
static unsigned preparedAnimations=0;
static void prepareHiddenAnimation(void* actor){
    void* mesh=objectField(actor,L"Mesh");if(!mesh)return;
    need(name(mem<void*>(mesh,0x24))==L"SkeletalMesh","Unreviewed HP1 player mesh type");
    auto location=field(actor,L"Location",12,L"StructProperty"),rot=field(actor,L"Rotation",12,L"StructProperty");
    Vec p=mem<Vec>(actor,location);Rot r=mem<Rot>(actor,rot);
    using ApplyAnim=void(__thiscall*)(void*,void*,void*,bool);
    Fpu fpu;fpu.restore();reinterpret_cast<ApplyAnim>(target("applyanim"))(mesh,actor,nullptr,false);
    need(!std::memcmp(&p,&mem<Vec>(actor,location),sizeof(p))&&!std::memcmp(&r,&mem<Rot>(actor,rot),sizeof(r)),"HP1 animation preparation changed actor pose");
    if(++preparedAnimations==1)log("{\"event\":\"hp1_hidden_animation\",\"stock_apply_anim\":true,\"physics_only\":false,\"pose_unchanged\":true}");
    fpu.restore();
}
#include "hp1_stereo_test.h"
static bool vrWorld(void* renderer,void* frame){
    if(startup.mode!=3&&startup.mode!=4&&startup.mode!=6)return false;
    if(loadingDepth){originalWorld(renderer,frame);return true;}
    Fpu fpu;
    try{
        void* viewport=mem<void*>(frame,0),*actor=mem<void*>(viewport,0x30),*rd=mem<void*>(viewport,0x5c);
        auto device=mem<IDirect3DDevice7*>(rd,0x9a4);need(device!=nullptr,"VR needs HP1 D3D7");
        unsigned width=mem<DWORD>(frame,0xa8),height=mem<DWORD>(frame,0xac);
        bool live=startup.mode==3;
        storyUiVisibility();
        vrProfile.begin();
        hpvr::Mat hmd=hpvr::identity(),eyeLocal[2]={hpvr::identity(),hpvr::identity()};
        float tangents[2][4]={{-1.1f,1.1f,-1.1f,1.1f},{-1.1f,1.1f,-1.1f,1.1f}};
        if(live){
            if(!runtime.initialized){
                auto dll=std::wstring(startup.root)+L"\\external\\openvr\\bin\\win32\\openvr_api.dll";
                need(runtime.start(dll.c_str()),runtime.error.c_str());
                log("{\"event\":\"hp1_runtime_ready\",\"adapter\":%d,\"recommended_width\":%u,\"recommended_height\":%u}",runtime.adapter,runtime.width,runtime.height);
            }
            bool valid;{VrStageTimer measured(VrProfile::PoseWait);valid=runtime.sample(true);}
            if(!valid){vrProfile.active=false;controls={};handStatus.hide();flightUi.hide();lessonOverlay.hide();tutorialModels.hide();originalWorld(renderer,frame);fpu.restore();return true;}
            std::memcpy(&hmd,&runtime.poses[0].mDeviceToAbsoluteTracking,sizeof(hmd));
            std::memcpy(eyeLocal,runtime.eyeToHead,sizeof(eyeLocal));std::memcpy(tangents,runtime.tangents,sizeof(tangents));
            bridge.open(runtime.adapter,width,height);
        }else{eyeLocal[0].m[0][3]=-.032f;eyeLocal[1].m[0][3]=.032f;}
        void* hud=objectField(actor,L"myHUD");bool chess=chessActive();bool cinema=(hud&&flag(hud,L"bCutSceneMode")&&!chess)||inLesson();
        stereoFixtureRoom(cinema);
        // Read THIS frame, not the last ComputeRenderCoords call (which may
        // belong to a sky/portal or the previous stereo eye).
        Vec originalPosition=mem<Vec>(frame,0x34);
        hpvr::Axes cameraAxes={mem<Vec>(frame,0x58),mem<Vec>(frame,0x40),hpvr::scale(mem<Vec>(frame,0x4c),-1)};
        Rot originalRotation=hpvr::rotation(cameraAxes);
        Vec anchor=mem<Vec>(actor,field(actor,L"Location",12,L"StructProperty"));anchor.z+=mem<float>(actor,field(actor,L"BaseEyeHeight",4,L"FloatProperty"));
        Rot body=mem<Rot>(actor,field(actor,L"Rotation",12,L"StructProperty"));
        void* level=mem<void*>(frame,4);
        bool newLevel=level!=trackingLevel;
        if(!trackingOrigin.ready||newLevel)trackingOrigin.recenter(hmd,body.yaw*hpvr::pi/32768);
        if(climbCaptureStart)trackingOrigin.recenter(hmd,controls.yaw*hpvr::pi/32768);
        if(mechanicsRecenter){trackingOrigin.recenter(hmd,controls.yaw*hpvr::pi/32768);mechanicsRecenter=false;}
        if(cinema&&(!previousCinema||newLevel))sceneReference=hmd;
        if(cinema!=previousCinema||newLevel)log("{\"event\":\"hp1_scene_view\",\"cinematic\":%s,\"camera_yaw\":%d,\"camera_pitch\":%d,\"relative_head_pose\":true}",cinema?"true":"false",originalRotation.yaw,originalRotation.pitch);
        trackingLevel=level;previousCinema=cinema;
        bool chasing=trollChase()&&!cinema;
        if(chasing&&(!previousTroll||newLevel)){
            trackingOrigin.recenter(hmd,0);
            log("{\"event\":\"hp1_troll_view\",\"escape_heading\":0,\"free_head\":true,\"course_relative_stick\":true}");
        }
        previousTroll=chasing;
        if(live){actions.poll(hmd,anchor,cinema);updateMenuInput();}
        bool lobby=lobbyLevel&&lobbyWanted();
        flatCinemaActive=lobby&&flatSceneWanted();
        if(!flatCinemaActive){cinemaScreen.hide();if(cinemaScene.target){cinemaScene.close();cinemaTexture.close();}}
        if(lobby&&!lobbyWasVisible){lobbyOrigin.recenter(hmd,lobbyHeading);log("{\"event\":\"hp1_lobby_visible\",\"visible\":true}");}
        if(!lobby&&lobbyWasVisible)log("{\"event\":\"hp1_lobby_visible\",\"visible\":false}");
        lobbyWasVisible=lobby;
        syncFlutePose();
        bool flying=flightKind()&&!cinema;
        auto flightNow=GetTickCount64();
        if(flying&&(!previousFlight||newLevel||(controls.recenter&&!flightRecenterHeld))){flightReference=hp1::flightHeadReference(hmd);flightViewHeading.reset(body.yaw);flightViewTime=flightNow;log("{\"event\":\"hp1_flight_view\",\"body_heading\":true,\"free_head\":true,\"bank_roll\":false,\"level_horizon\":true,\"smoothed_motion\":true}");}
        flightRecenterHeld=controls.recenter;previousFlight=flying;
        auto viewBody=body;if(flying)viewBody.yaw=flightViewHeading.update(body.yaw,float(flightNow-flightViewTime)/1000);flightViewTime=flightNow;
        auto broomAxes=hp1::flightAxes(viewBody);
        if(live&&!tutorialVisualFailed){try{tutorialModels.update(!cinema||lessonDrawing);}catch(const std::exception& e){tutorialVisualFailure(e.what());}}
        else if(tutorialTestMode)tutorialTestRender();
        // Reuse precisely the same world/cinematic/flight transform as the eyes.
        auto tutorialMapPoint=[&](Vec point){auto m=hpvr::identity();m.m[0][3]=point.x;m.m[1][3]=point.y;m.m[2][3]=point.z;
            if(lobby)return lobbyOrigin.position(m,lobbyAnchor,64);
            if(cinema)return hpvr::scenePose(sceneReference,m,originalPosition,cameraAxes,64).position;
            if(flying)return hpvr::scenePose(flightReference,m,anchor,broomAxes,64).position;
            return trackingOrigin.position(m,anchor,64);};
        hpvr::Mat tutorialMapping={};Vec mapOrigin=tutorialMapPoint({}),mapAxes[]={hpvr::sub(tutorialMapPoint({1,0,0}),mapOrigin),hpvr::sub(tutorialMapPoint({0,1,0}),mapOrigin),hpvr::sub(tutorialMapPoint({0,0,1}),mapOrigin),mapOrigin};
        for(unsigned col=0;col<4;++col){tutorialMapping.m[0][col]=mapAxes[col].x;tutorialMapping.m[1][col]=mapAxes[col].y;tutorialMapping.m[2][col]=mapAxes[col].z;}
        if(inLesson()&&lessonGeometry()){
            if(live&&controls.hand){
                auto relative=hpvr::multiply(hpvr::inverseRigid(sceneReference),controls.physicalWand);
                controls.wandBase=hpvr::add(originalPosition,hpvr::scale(hpvr::sceneVector(cameraAxes,hpvr::translation(relative)),64));
                controls.wandForward=hpvr::sceneVector(cameraAxes,hpvr::scale(hpvr::axis(relative,2),-1));
                controls.wandUp=hpvr::sceneVector(cameraAxes,hpvr::axis(relative,1));
            }
            lessonCursor=controls.hand?hp1::lessonRay(lessonPlane,controls.wandBase,controls.wandForward):hp1::LessonHit{};
        }else lessonCursor={};
        if(flatCinemaActive){
            hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Clear flat scene");fpu.restore();originalWorld(renderer,frame);
            cinemaScene.lastCapture=0;need(cinemaScene.begin(device),"Create HP1 cinema scene target");hr(device->EndScene(),"Finish flat camera copy");
            if(!cinemaScene.borrowed)hr(cinemaScene.target->Blt(nullptr,cinemaScene.saved,nullptr,DDBLT_WAIT,nullptr),"Preserve original cinema camera");
            cinemaScene.restore(device);hr(device->BeginScene(),"Begin entrance stereo background");
        }
        eyeProjection=hpvr::projection(tangents,float(width)/height);
        if(live&&vrFrames==0)for(unsigned e=0;e<2;++e){auto m=runtime.system->GetProjectionMatrix(static_cast<EVREye>(e),.1f,1000);float err=hpvr::projectionMatrixError(eyeProjection,e,m.m);need(err<.00001f,"HP1 projection differs from headset matrix");}
        unsigned fovOffset=field(actor,L"FOVAngle",4,L"FloatProperty");float savedFov=mem<float>(actor,fovOffset);mem<float>(actor,fovOffset)=eyeProjection.fov;
        using RenderSize=void(__thiscall*)(void*);auto computeSize=reinterpret_cast<RenderSize>(target("size"));
        bool behind=flag(actor,L"bBehindView");hideDirectBody=!cinema||inLesson();
        if(hideDirectBody){prepareHiddenAnimation(actor);setFlag(actor,L"bBehindView",true);}
        RestoreFields lessonVisibility;
        if(inLesson()&&lessonActor){void* wand=objectField(lessonActor,L"Wand");if(wand){auto prop=findProperty(mem<void*>(wand,0x24),L"bHidden");unsigned off=field(wand,L"bHidden",4,L"BoolProperty");lessonVisibility.set(wand,off,mem<DWORD>(wand,off)|mem<DWORD>(prop,0x5c));}}
        void* sourceLevelInfo=objectField(actor,L"Level");DWORD sourceShowFlags=mem<DWORD>(actor,field(actor,L"ShowFlags",4,L"IntProperty"));
        Vec sourcePlayerPosition=mem<Vec>(actor,field(actor,L"Location",12,L"StructProperty"));
        unsigned regionOffset=field(actor,L"Region",sizeof(Hp1PointRegion),L"StructProperty");auto sourceRegion=mem<Hp1PointRegion>(actor,regionOffset);
        void* sourceAudio=mem<void*>(mem<void*>(renderer,kRenderEngineOffset),kEngineAudioOffset);
        DWORD initialTick=ticks;IDirectDrawSurface7* surface=nullptr;hr(device->GetRenderTarget(&surface),"Get VR target");
        pairActive=true;secondary=false;primaryEvents.clear();
        for(unsigned eye=0;eye<2;++eye){
            auto pose=hpvr::multiply(hmd,eyeLocal[eye]);Vec p=trackingOrigin.position(pose,anchor,64);Rot r=hpvr::rotation(pose,trackingOrigin.baseYaw);
            if(cinema){auto scene=hpvr::scenePose(sceneReference,pose,originalPosition,cameraAxes,64);p=scene.position;r=scene.rotation;}
            else if(flying){auto view=hpvr::scenePose(flightReference,pose,anchor,broomAxes,64);p=view.position;r=view.rotation;}
            chessTestEye(eye,p,r,anchor,cinema);
            if(lobby){p=lobbyOrigin.position(pose,lobbyAnchor,64);r=hpvr::rotation(pose,lobbyOrigin.baseYaw);}
            // Audio still belongs to the real frame. The separate map is only
            // a visual background, so it must not move the listener or tick actors.
            void* engine=mem<void*>(renderer,kRenderEngineOffset);
            void* audio=mem<void*>(engine,kEngineAudioOffset);
            if(lobby&&!flatCinemaActive&&!eye&&audio){using Update=void(__thiscall*)(void*,void*);fpu.restore();reinterpret_cast<Update>(mem<void*>(mem<void*>(audio,0),0x88))(audio,frame);}
            RestoreFields background;
            if(lobby){
                need(field(actor,L"Level",4,L"ObjectProperty")==0x7c&&field(actor,L"ShowFlags",4,L"IntProperty")==0x4e4,"HP1 background player layout differs");
                background.set(actor,0x80,lobbyLevel);
                background.set(actor,0x7c,mem<void*>(mem<void*>(lobbyLevel,0x2c),0));
                need(regionOffset==0xa0,"HP1 player region layout differs");auto region=lobbyRegion(p);background.set(actor,regionOffset,region);
                if(lobbyTestMode&&lobbyTestEyes<2)log("{\"event\":\"hp1_lobby_region\",\"eye\":%u,\"source_leaf\":%d,\"lobby_leaf\":%d,\"lobby_leaves\":%d}",eye,sourceRegion.leaf,region.leaf,mem<int>(mem<void*>(lobbyLevel,0x98),0xdc));
                background.set(actor,0x4e4,mem<DWORD>(actor,0x4e4)&~DWORD(8));
                background.set(engine,kEngineAudioOffset,static_cast<void*>(nullptr));
            }
            lobbyRendering=lobby;
            // Both eyes need a NEW master frame. Merely replacing the original
            // camera coordinates leaves its BSP ZoneNumber (frame+0x18) stale.
            // CreateMasterFrame derives the zone/frustum from each actual eye.
            secondary=eye!=0;fpu.restore();void* current=createFrame(renderer,viewport,p,r,nullptr);
            need(current&&current!=frame&&mem<DWORD>(renderer,0xa8)==2,"VR eye frame lifetime");
            DWORD eyeZone=mem<DWORD>(current,0x18),sourceZone=mem<DWORD>(frame,0x18);
            static unsigned zoneCorrections=0;
            if(eyeZone!=sourceZone&&++zoneCorrections<=16)log("{\"event\":\"hp1_eye_zone_corrected\",\"eye\":%u,\"source\":%lu,\"actual\":%lu}",eye,sourceZone,eyeZone);
            stereoFixtureCleanup(eye);
            fpu.restore();computeSize(current);originalCoords(current,p,r);
            hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Clear VR eye");
            if(eye&&!lobby)mem<void*>(engine,kEngineAudioOffset)=nullptr;
            {VrStageTimer measured(eye?VrProfile::WorldRight:VrProfile::WorldLeft);fpu.restore();originalWorld(renderer,current);}if(eye&&!lobby)mem<void*>(engine,kEngineAudioOffset)=audio;
            if(!lobby&&!ownedFlute()&&!broomPlayer()&&(!cinema||inLesson())&&!menuVisible)drawHandWand(device,current,eye);
            if((!lobby||flatCinemaActive)&&!menuVisible&&!tutorialVisualFailed){try{tutorialModels.draw(device,current,tutorialMapping,eye);}catch(const std::exception& e){tutorialVisualFailure(e.what());}}
            if(!lobby&&inLesson()&&!menuVisible)drawLessonCursor(device,current);
            if(!lobby&&flying)drawFlightPath(device,current,eye);
            hr(device->EndScene(),"End VR eye");
            if(tutorialTestMode&&tutorialTestCapture){wchar_t filename[80];swprintf(filename,80,L"tutorial-%u-eye-%u.bmp",tutorialTestCapture,eye);capture(surface,filename);if(eye)tutorialTestCapture=0;}
            if(lobbyTestMode&&flatCinemaActive&&cinemaSamples==0)capture(surface,eye?L"cinema-hall-eye-b.bmp":L"cinema-hall-eye-a.bmp");
            if(lobbyTestMode&&lobby){need(mem<void*>(current,4)==lobbyLevel,"Background eye uses real map");++lobbyTestEyes;}
            if(lobbyTestMode&&lobbyTestCapture){wchar_t filename[80];swprintf(filename,80,L"lobby-%u-eye-%u.bmp",lobbyTestCapture,eye);capture(surface,filename);}
            if(mechanicsTestMode==1&&(mirrorCaptureMask&(1u<<eye))&&!(mirrorSavedMask&(1u<<eye))){capture(surface,eye?L"mirror-eye-b.bmp":L"mirror-eye-a.bmp");mirrorSavedMask|=1u<<eye;}
            if(mechanicsTestMode==2&&mechanicsCapture)capture(surface,mechanicsCapture==1?(eye?L"levitation-start-b.bmp":L"levitation-start-a.bmp"):(eye?L"levitation-float-b.bmp":L"levitation-float-a.bmp"));
            if(live){DDSURFACEDESC2 desc={};desc.dwSize=sizeof(desc);hr(surface->Lock(nullptr,&desc,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Read VR pixels");try{bridge.upload(eye,desc,!(bridge.verifiedMask&(1u<<eye)),false);}catch(...){surface->Unlock(nullptr);throw;}hr(surface->Unlock(nullptr),"Unlock VR pixels");}
            if(lessonFixture()&&lessonDrawing&&controls.cast&&!lessonDrawCaptured)capture(surface,eye?L"lesson-eye-b.bmp":L"lesson-eye-a.bmp");
            if(flightCapturePending)capture(surface,eye?L"flight-eye-b.bmp":L"flight-eye-a.bmp");
            if(trollCapture)capture(surface,eye?L"troll-eye-b.bmp":L"troll-eye-a.bmp");
            if(chessCapture)capture(surface,chessCapture==1?(eye?L"chess-player-b.bmp":L"chess-player-a.bmp"):(eye?L"chess-pieces-b.bmp":L"chess-pieces-a.bmp"));
            if(fluteCapture)capture(surface,fluteCapture==1?(eye?L"flute-eye-b.bmp":L"flute-eye-a.bmp"):(eye?L"flute-tilt-b.bmp":L"flute-tilt-a.bmp"));
            if(vrFrames==0)capture(surface,eye?L"vr-eye-b.bmp":L"vr-eye-a.bmp");
            if(inputReplay()&&replayCastStarted&&!capturedGameplay)capture(surface,eye?L"gameplay-eye-b.bmp":L"gameplay-eye-a.bmp");
            if(stereoRoomPending){capture(surface,eye?L"room-eye-b.bmp":L"room-eye-a.bmp");log("{\"event\":\"hp1_room_eye\",\"eye\":%u,\"source_zone\":%lu,\"eye_zone\":%lu}",eye,sourceZone,eyeZone);}
            if(climbCaptureStart)capture(surface,eye?L"climb-start-b.bmp":L"climb-start-a.bmp");
            if(climbCaptureEnd)capture(surface,eye?L"climb-end-b.bmp":L"climb-end-a.bmp");
            hr(device->BeginScene(),"Resume HP1 scene");
            fpu.restore();finishFrame(renderer);need(mem<DWORD>(renderer,0xa8)==1,"VR eye frame released");lobbyRendering=false;
        }
        lobbyTestCapture=0;chessCapture=0;fluteCapture=0;trollCapture=false;mechanicsCapture=0;secondary=false;fpu.restore();mem<float>(actor,fovOffset)=savedFov;setFlag(actor,L"bBehindView",behind);hideDirectBody=false;computeSize(frame);originalCoords(frame,originalPosition,originalRotation);surface->Release();pairActive=false;fpu.restore();
        need(ticks==initialTick&&mem<DWORD>(renderer,0xa8)==1,"VR pair changed simulation tick/lifetime");
        if(lobby){
            need(mem<void*>(actor,0x80)==level&&objectField(actor,L"Level")==sourceLevelInfo&&mem<DWORD>(actor,0x4e4)==sourceShowFlags&&mem<void*>(mem<void*>(renderer,kRenderEngineOffset),kEngineAudioOffset)==sourceAudio,"Entrance render did not restore game references");
            auto now=mem<Vec>(actor,field(actor,L"Location",12,L"StructProperty"));need(!std::memcmp(&now,&sourcePlayerPosition,sizeof(now)),"Entrance render moved Harry");
            need(!std::memcmp(&sourceRegion,&mem<Hp1PointRegion>(actor,regionOffset),sizeof(sourceRegion)),"Entrance render retained its BSP region in gameplay");
        }
        if(lessonFixture()&&lessonDrawing&&controls.cast)lessonDrawCaptured=true;
        if(flightCapturePending){
            auto values=readHp1Status(player);handStatus.paint(values);handStatus.panel.upload(0,nullptr);auto& panel=handStatus.panel;
            BITMAPFILEHEADER bf={};BITMAPINFOHEADER bi={};bf.bfType=0x4d42;bf.bfOffBits=sizeof(bf)+sizeof(bi);bf.bfSize=bf.bfOffBits+panel.width*panel.height*4;
            bi.biSize=sizeof(bi);bi.biWidth=panel.width;bi.biHeight=-LONG(panel.height);bi.biPlanes=1;bi.biBitCount=32;bi.biSizeImage=panel.width*panel.height*4;
            FILE* out=_wfopen((std::wstring(startup.profile)+L"\\flight-status.bmp").c_str(),L"wbx");need(out,"Flight status capture");
            bool written=fwrite(&bf,sizeof(bf),1,out)==1&&fwrite(&bi,sizeof(bi),1,out)==1&&fwrite(panel.data,bi.biSizeImage,1,out)==1;fclose(out);need(written,"Flight status pixels");handStatus.close();
            log("{\"event\":\"hp1_flight_status_test\",\"rings\":%d,\"max_rings\":%d,\"seconds\":%d,\"energy\":%d,\"gpu_readback\":true}",values.rings,values.maxRings,values.seconds,values.health);
            flightCapturePending=false;
        }
        if(stereoRoomPending){stereoRoomPending=false;stereoRoomCaptured=true;}
        bool didCapture=vrFrames==0||climbCaptureStart||climbCaptureEnd||(inputReplay()&&replayCastStarted&&!capturedGameplay);
        climbCaptureStart=climbCaptureEnd=false;
        if(inputReplay()&&replayCastStarted)capturedGameplay=true;
        if(live){bridge.flush();unsigned accepted=0;for(unsigned e=0;e<2;++e){auto b=eyeProjection.eye[e];VRTextureBounds_t bounds={b.uMin,b.vMin,b.uMax,b.vMax};auto status=runtime.submitEye(e,bridge.textures[e],&bounds);need(status==EVRCompositorError_VRCompositorError_None||VrRuntime::transientSubmit(status),"HP1 compositor submission failed");if(status==EVRCompositorError_VRCompositorError_None)++accepted;}if(accepted==2)++submittedPairs;runtime.presentHandoff();}
        if(live){handStatus.update(cinema);updateLessonPanel();}
        flightUi.update(cinema);
        if(++vrFrames==1||vrFrames%300==0)log("{\"event\":\"hp1_stereo_frame\",\"frame\":%u,\"headset\":%s,\"width\":%u,\"height\":%u,\"extra_ticks\":0,\"submitted_pairs\":%u}",vrFrames,live?"true":"false",width,height,submittedPairs);
        vrProfile.end(vrFrames,didCapture);vrProfile.reportInterval(trace,vrFrames);
        fpu.restore();return true;
    }catch(const std::exception& e){fatal("hp1_vr",e.what());}
}
