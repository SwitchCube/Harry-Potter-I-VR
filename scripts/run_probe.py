"""Run an owned, isolated HP1 session; seconds=0 waits for explicit exit."""
import argparse
import json
import os
import hashlib
import subprocess
from datetime import datetime
from project_paths import ROOT, inside, write_text
from prepare_target import verify_originals
from validate_actions import main as validate_actions
from probe_session import wait_timeout,register,unregister


def menu_capture_coverage(profile):
    """Catch a mismatched canvas/frame viewport that renders only one corner."""
    import struct
    data=(inside(profile)/'menu-capture.bmp').read_bytes()
    if data[:2]!=b'BM' or struct.unpack_from('<H',data,28)[0]!=24:
        raise ValueError('Expected menu RGB capture')
    offset=struct.unpack_from('<I',data,10)[0]
    width,height=struct.unpack_from('<ii',data,18)
    book_height=width*3//4
    if width<640 or abs(height) not in (book_height,width):raise ValueError('Unexpected book capture dimensions')
    stride=(width*3+3)&~3
    if len(data)<offset+stride*abs(height):raise ValueError('Truncated menu capture')
    # A borrowed scene target retains its square extent. Only the top 4:3
    # rectangle is the book; BMP scanlines can be bottom-up or top-down.
    first=abs(height)-book_height if height>0 else 0
    samples=[max(data[offset+y*stride+x*3:offset+y*stride+x*3+3])>16
             for y in range(first,first+book_height,16) for x in range(0,width,16)]
    coverage=sum(samples)/len(samples)
    if coverage<.65:raise ValueError('Book only covers a corner of the menu texture')
    return dict(width=width,height=abs(height),book_height=book_height,lit_fraction=round(coverage,5))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build',default='config/last-build.json',help='Build manifest to test without changing the selected play build')
    parser.add_argument('--mode',choices=['observe','shift-x8','native-control','native-capture','native-input','native-replay','native-gameplay','native-vr'],default='observe')
    parser.add_argument('--seconds',type=int,default=25,help='5..180 for diagnostics; 0 for manual exit in native-vr/native-replay')
    parser.add_argument('--start-at-lesson',action='store_true',help='Start a private live test at the original first lesson after the intro')
    parser.add_argument('--lesson-test',choices=['pass','fail'],help='Private original spell-learning controller trace fixture')
    parser.add_argument('--flight-test',action='store_true',help='Private native broom controls and original hoop progression fixture')
    parser.add_argument('--flight-action-test',action='store_true',help='Explicit private catch/bump boundary after flight control replay')
    parser.add_argument('--menu-memory-test',choices=['normal','failure'],help='Private Quidditch book reopen stress; failure injects unavailable text DIB')
    parser.add_argument('--mechanics-test',choices=['mirror','levitation'],help='Private Lev_Tut3 native mirror/levitation fixture')
    parser.add_argument('--story-ui-test',choices=['bosses','mail'],help='Private original boss HUD or original scroll/post fixture')
    parser.add_argument('--stereo-check',action='store_true',help='Private native lifecycle/room/status integration regression')
    parser.add_argument('--travel',action='store_true',help='Replay one stock ClientTravel in the private stereo/input test')
    parser.add_argument('--climb',nargs=5,type=float,metavar=('X','Y','Z','YAW','JUMP_SECONDS'),help='Private original-collision climb fixture; -1 disables jump')
    parser.add_argument('--map',choices=['Lev_Tut1.unr','Lev_Tut2.unr','Lev_Tut3.unr','Lev_Tut3b.unr','Lev5_fluffy.unr','Lev5_Final.unr','Lev2_RemChase.unr','Lev2_Quid1.unr','Lev3_Quid2.unr','Lev5_FlyKeys.unr',*[f'Quid_{h}{d}.unr' for h in ('Slyth','Raven','Huffle') for d in ('A','B','C')],'Startup.unr','Entry.unr'],default='Lev_Tut1.unr')
    args=parser.parse_args()
    timeout=wait_timeout(args.mode,args.seconds)
    if args.mode=='native-vr':
        validate_actions()
    verify_originals()
    build=json.loads(inside(args.build).read_text())
    probe=json.loads(inside('config/last-probe.json').read_text())
    if args.menu_memory_test:
        if args.mode!='native-gameplay' or not args.flight_test or args.flight_action_test or args.map!='Lev2_Quid1.unr':
            raise ValueError('Menu memory fixture requires isolated Quidditch flight replay')
        write_text(inside(probe['profile'])/'test-menu-cycles.flag','private twelve-cycle menu test\n')
        if args.menu_memory_test=='failure':write_text(inside(probe['profile'])/'test-menu-memory.flag','private failed text DIB\n')
    if args.story_ui_test:
        if args.mode!='native-gameplay' or any((args.mechanics_test,args.lesson_test,args.flight_test,args.climb,args.stereo_check,args.travel,args.start_at_lesson)):
            raise ValueError('Story UI fixture requires isolated native-gameplay')
        write_text(inside(probe['profile'])/'story-test.flag',args.story_ui_test+'\n')
    if args.mechanics_test:
        if args.mode!='native-gameplay' or args.map!='Lev_Tut3.unr' or args.lesson_test or args.flight_test or args.climb or args.stereo_check or args.travel or args.start_at_lesson:raise ValueError('Mechanics fixture requires isolated native-gameplay in Lev_Tut3')
        write_text(inside(probe['profile'])/'mechanics-test.flag',args.mechanics_test+'\n')
    if args.start_at_lesson:
        if args.mode!='native-vr':raise ValueError('--start-at-lesson requires native-vr')
        write_text(inside(probe['profile'])/'start-lesson.flag','private lesson start\n')
    if args.lesson_test:
        if args.mode!='native-gameplay' or args.travel or args.climb or args.stereo_check:raise ValueError('--lesson-test requires an isolated native-gameplay run')
        write_text(inside(probe['profile'])/'test-lesson.flag',('1' if args.lesson_test=='pass' else '2')+'\n')
    if args.flight_action_test:
        if not args.flight_test or args.map=='Lev_Tut2.unr':raise ValueError('Action boundary requires non-practice flight fixture')
        write_text(inside(probe['profile'])/'test-flight-action.flag','private original state boundary\n')
    if args.flight_test:
        if args.mode!='native-gameplay' or args.map not in ['Lev_Tut2.unr','Lev2_RemChase.unr','Lev2_Quid1.unr','Lev3_Quid2.unr','Lev5_FlyKeys.unr',*[f'Quid_{h}{d}.unr' for h in ('Slyth','Raven','Huffle') for d in ('A','B','C')]] or args.lesson_test or args.climb or args.stereo_check or args.travel:raise ValueError('--flight-test requires isolated native-gameplay in a reviewed broom map')
        write_text(inside(probe['profile'])/'test-flight.flag','private controls replay\n')
    if args.stereo_check:
        if args.mode!='native-gameplay' or args.climb:raise ValueError('--stereo-check requires native-gameplay without climb')
        write_text(inside(probe['profile'])/'test-stereo.flag','private regression only\n')
    if args.climb:
        if args.mode!='native-gameplay' or args.travel or args.map!='Lev_Tut1.unr':
            raise ValueError('--climb requires native-gameplay in Lev_Tut1 without travel')
        x,y,z,yaw,jump=args.climb
        if not all(__import__('math').isfinite(v) for v in args.climb):raise ValueError('Nonfinite fixture')
        write_text(inside(probe['profile'])/'test-climb.flag',f'{x} {y} {z} {int(yaw)} {jump}\n')
    if args.travel:
        if args.mode!='native-gameplay':
            raise ValueError('--travel requires native-gameplay')
        write_text(inside(probe['profile'])/'test-travel.flag','private test only\n')
    stamp=datetime.now().strftime('%Y%m%d-%H%M%S')
    log=inside('logs/probe-'+stamp+'.jsonl')
    env=os.environ.copy()
    env['TEMP']=env['TMP']=str(inside('cache/tmp'))
    cmd=[str(inside(build['executable'])),str(inside(probe['executable'])),
         str(inside(probe['profile'])),str(args.seconds),args.map,args.mode]
    with log.open('xb') as out:
        with subprocess.Popen(cmd,cwd=ROOT,env=env,stdout=out,stderr=subprocess.STDOUT) as process:
            session=register(probe['profile'],process.pid)
            try:
                process.wait(timeout=timeout)
            except (subprocess.TimeoutExpired,KeyboardInterrupt):
                write_text(inside(probe['profile'])/'stop-session.flag','runner cleanup\n',exclusive=False)
                try:
                    process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
                raise
            finally:
                unregister(session)
            run=subprocess.CompletedProcess(cmd,process.returncode)
    verify_originals()
    native_complete=None
    events=[]
    if args.mode.startswith('native-') and args.mode not in ('native-replay','native-gameplay','native-vr'):
        native_log=inside(probe['profile'])/'hp1vr-native.jsonl'
        events=[json.loads(s) for s in native_log.read_text().splitlines()] if native_log.exists() else []
        native_complete=next((e for e in events if e.get('event')=='hp1_pair_complete'),None)
        if not native_complete or any(e.get('event') in ('fatal','init_error') for e in events):
            run.returncode=run.returncode or 7
        else:
            a=(inside(probe['profile'])/'eye-a.bmp').read_bytes()
            b=(inside(probe['profile'])/'eye-b.bmp').read_bytes()
            equal=a==b
            if equal!=(args.mode=='native-control'):
                run.returncode=8
            native_complete.update(images_equal=equal,eye_a_sha256=hashlib.sha256(a).hexdigest(),eye_b_sha256=hashlib.sha256(b).hexdigest())
    elif args.mode in ('native-replay','native-gameplay','native-vr'):
        native_log=inside(probe['profile'])/'hp1vr-native.jsonl'
        events=[json.loads(s) for s in native_log.read_text().splitlines()] if native_log.exists() else []
        stereo=[e for e in events if e.get('event')=='hp1_stereo_frame']
        if not stereo or (args.mode!='native-vr' and stereo[-1]['frame']<300) or any(e.get('event') in ('fatal','init_error') for e in events):
            run.returncode=run.returncode or 7
        native_complete=stereo[-1] if stereo else None
        if args.mode=='native-vr':
            ready=any(e.get('event')=='hp1_actions_ready' for e in events)
            submitted=max((e.get('submitted_pairs',0) for e in events),default=0)
            if not ready or not submitted:
                run.returncode=run.returncode or 15
    if args.mode in ('native-input','native-gameplay') and not args.climb and not args.lesson_test and not args.flight_test and not args.mechanics_test and not args.story_ui_test:
        movement=next((e for e in events if e.get('event')=='hp1_movement_replay_complete'),None)
        skipped=next((e for e in events if e.get('event')=='hp1_scene_skip_stopped' and e.get('scene_released')),None)
        spells=[e for e in events if e.get('event')=='hp1_controller_spell' and e.get('spawned')]
        if not movement or not movement.get('passed') or not skipped or len(spells)!=1:
            run.returncode=run.returncode or 9
        elif not (0<=spells[0]['tip_origin_error']<.05 and abs(spells[0]['pitch_error'])<=1 and abs(spells[0]['yaw_error'])<=1):
            run.returncode=run.returncode or 10
        rays=[e for e in events if e.get('event')=='hp1_hand_target_ray']
        if not rays or (args.mode=='native-gameplay' and rays[0]['pitch']!=-12000):
            run.returncode=run.returncode or 12
        if args.mode=='native-gameplay':
            if not any(e.get('event')=='hp1_menu_captured' for e in events) or not any(e.get('event')=='hp1_menu_visibility' and e.get('visible') is False for e in events):
                run.returncode=run.returncode or 13
            if not any(e.get('event')=='hp1_menu_confirmation' for e in events):run.returncode=run.returncode or 19
            if not any(e.get('event')=='hp1_menu_pointer_click' and e.get('options_opened') for e in events):
                run.returncode=run.returncode or 14
    if args.travel:
        loaded=next((i for i,e in enumerate(events) if e.get('event')=='hp1_level_end' and e.get('loaded')),None)
        if loaded is None or not any(e.get('event')=='hp1_stereo_frame' for e in events[loaded+1:]):
            run.returncode=run.returncode or 11
    if args.climb:
        completed=next((e for e in events if e.get('event')=='hp1_climb_complete'),None)
        if not completed or completed.get('mounts',0)<1 or not completed.get('grounded'):
            run.returncode=run.returncode or 16
    if args.lesson_test:
        judged=next((e for e in events if e.get('event')=='hp1_lesson_phase' and e.get('phase')=='Judge'),None)
        if not judged or (judged['score']<.75 if args.lesson_test=='pass' else judged['score']>.3):run.returncode=run.returncode or 18
    if args.flight_test:
        completed=next((e for e in events if e.get('event')=='hp1_flight_test_complete'),None)
        if not completed or not completed.get('passed'):run.returncode=run.returncode or 20
        if not any(e.get('event')=='hp1_flight_menu_test' and e.get('passed') for e in events):run.returncode=run.returncode or 21
        if not any(e.get('event')=='hp1_flight_status_test' and e.get('gpu_readback') and (args.map!='Lev_Tut2.unr' or e.get('max_rings',0)>0) for e in events):run.returncode=run.returncode or 22
        cues=[e for e in events if e.get('event')=='hp1_flight_path_cue']
        if len(cues)!=2 or {e['eye'] for e in cues}!={0,1} or cues[0]['world']!=cues[1]['world'] or abs(cues[0]['screen'][0]-cues[1]['screen'][0])<.1:run.returncode=run.returncode or 23
    if args.flight_action_test:
        if not any(e.get('event')=='hp1_flight_action_test_complete' and e.get('passed') for e in events):run.returncode=run.returncode or 27
    if args.menu_memory_test:
        cycles=next((e for e in events if e.get('event')=='hp1_menu_cycles_complete' and e.get('passed')),None)
        fallback=[e for e in events if e.get('event')=='hp1_menu_text_fallback']
        rasters=[e for e in events if e.get('event')=='hp1_menu_text_raster']
        if not cycles or cycles.get('cycles')!=12:run.returncode=run.returncode or 28
        if args.menu_memory_test=='failure':
            if len(fallback)<13 or any(not e.get('simulated') or not e.get('original_text') for e in fallback):run.returncode=run.returncode or 29
        elif fallback or len(rasters)<13 or any(e['scratch_bytes']>262144 for e in rasters):run.returncode=run.returncode or 30
        try:menu_capture_coverage(probe['profile'])
        except ValueError as error:
            print('Menu capture failed:',error)
            run.returncode=run.returncode or 31
    if args.mechanics_test:
        completed=next((e for e in events if e.get('event')=='hp1_'+args.mechanics_test+'_test_complete'),None)
        if not completed or not completed.get('passed'):run.returncode=run.returncode or 24
    if args.story_ui_test:
        event='hp1_boss_ui_test_complete' if args.story_ui_test=='bosses' else 'hp1_mail_ui_test_complete'
        completed=next((e for e in events if e.get('event')==event),None)
        if not completed or not completed.get('passed'):run.returncode=run.returncode or 25
        if completed and args.story_ui_test=='bosses':
            for n in range(completed['bosses']):
                a=(inside(probe['profile'])/f'boss-{n*2}.bmp').read_bytes()
                b=(inside(probe['profile'])/f'boss-{n*2+1}.bmp').read_bytes()
                if a==b:run.returncode=run.returncode or 26
    stereo_regression=None
    if args.stereo_check:
        from check_stereo_regression import check
        try:stereo_regression=check(probe['profile'],events)
        except ValueError as error:
            stereo_regression=dict(passed=False,reason=str(error))
            run.returncode=run.returncode or 17
    record=dict(log=log.relative_to(ROOT).as_posix(),exit_code=run.returncode,mode=args.mode,
                originals_verified=True,profile=probe['profile'],native_pair=native_complete,
                automatic_time_limit_seconds=args.seconds or None)
    if args.climb:record['climb']=completed
    if args.flight_test:record['flight']=completed
    if stereo_regression:record['stereo_regression']=stereo_regression
    errors=[e for e in events if e.get('event') in ('fatal','init_error')]
    if errors:
        record['native_errors']=errors
    write_text('config/last-result.json',json.dumps(record,indent=2)+'\n',exclusive=False)
    print(json.dumps(record))
    print('\n'.join(log.read_text(errors='replace').splitlines()[-12:]))
    for error in errors:
        print('VR-Fehler: [%s] %s' % (error.get('stage','init'),error.get('reason','Unbekannter Fehler')))
    raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
