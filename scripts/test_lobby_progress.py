"""Native Lobby inputs from a copied checkpoint; no real save activation/writes."""
import argparse
import hashlib
import json
import os
import subprocess
import time
from project_paths import ROOT, inside, write_text
from play_sessions import SessionLock, current_progress, digest_files, prepare, stamp
from probe_session import register, unregister
from prepare_target import verify_originals


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--settings',action='store_true');parser.add_argument('--startup',action='store_true');parser.add_argument('--slot',type=int,default=0,choices=range(6));parser.add_argument('--build',default='config/last-build.json');args=parser.parse_args()
    with SessionLock():
        state=current_progress()
        if not state:raise ValueError('A Lobby checkpoint is required')
        source=inside(state['profile']); before=digest_files(source)
        state_bytes=inside('config/play-session.json').read_bytes()
        profile=prepare('VR',state,diagnostic=True)
        if digest_files(profile)!=before:raise ValueError('Checkpoint copy differs')
        if args.settings:write_text(profile/'test-vr-settings.flag','isolated preferences and menu fixture\n')
        write_text(profile/'test-lobby.flag','private copied checkpoint controls\n')
        build=json.loads(inside(args.build).read_text())
        observer=inside(build['executable'])
        if hashlib.sha256(observer.read_bytes()).hexdigest()!=build['sha256']:raise ValueError('Build differs')
        log=inside('logs/lobby-progress-'+stamp()+'.jsonl')
        env=os.environ.copy();env['TEMP']=env['TMP']=str(inside('cache/tmp'))
        print(json.dumps(dict(profile=profile.relative_to(ROOT).as_posix(),log=log.relative_to(ROOT).as_posix())),flush=True)
        try:
            with log.open('xb') as out:
                with subprocess.Popen([str(observer),str(inside('cache/runtime/System/HP.exe')),str(profile),'180','Startup.unr' if args.startup else f'save{args.slot}.usa','native-gameplay'],cwd=ROOT,env=env,stdout=out,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW) as p:
                    registered=register(profile.relative_to(ROOT).as_posix(),p.pid)
                    try:
                        requested=False; deadline=time.monotonic()+210
                        while p.poll() is None:
                            native=profile/'hp1vr-native.jsonl'
                            if not requested and native.exists() and 'hp1_lobby_test_complete' in native.read_text():
                                write_text(profile/'stop-session.flag','completed private lobby test\n'); requested=True
                            if time.monotonic()>deadline:
                                write_text(profile/'stop-session.flag','diagnostic timeout\n',exclusive=False)
                                try:p.wait(timeout=15)
                                except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
                                raise TimeoutError('Owned Lobby test did not exit')
                            time.sleep(.4)
                    finally:unregister(registered)
            if not (profile/'hp1vr-native.jsonl').is_file():raise ValueError('Private game did not reach native initialization; inspect observer and HP1VR.log')
            events=[json.loads(line) for line in (profile/'hp1vr-native.jsonl').read_text().splitlines()]
            errors=[e for e in events if e.get('event') in ('fatal','init_error','hp1_native_fault')]
            done=next((e for e in events if e.get('event')=='hp1_lobby_test_complete'),None)
            if p.returncode or errors or not done or not done['passed']:raise ValueError(dict(exit_code=p.returncode,errors=errors,complete=done))
            captures=[f'lobby-{stage}-eye-{eye}.bmp' for stage in (2,3,4,6,7,13,15) for eye in (0,1)]
            captures += [f'lobby-{stage}-menu.bmp' for stage in (2,3,4,6,7,13,15)]
            for filename in captures:
                if not (profile/filename).is_file():raise ValueError('Missing lobby eye capture: '+filename)
            if digest_files(source)!=before or inside('config/play-session.json').read_bytes()!=state_bytes:raise ValueError('Real progress changed')
            game_log=(profile/'HP1VR.log').read_bytes()
            if b'Level is Level Lev_Tut1.MyLevel' not in game_log:raise ValueError('Native new game did not reach the original entrance map')
            verify_originals()
            report=dict(startup_menu=args.startup,source_slot=args.slot,quit_confirmation=True,regions=[e for e in events if e.get('event')=='hp1_lobby_region'],profile=profile.relative_to(ROOT).as_posix(),log=log.relative_to(ROOT).as_posix(),build=build,exit_code=p.returncode,complete=done,captures=captures,shutdown=next((e for e in events if e.get('event')=='hp1_shutdown'),None),source_saves_unchanged=True,state_unchanged=True,originals_verified=True,headset_test=False)
            write_text(log.with_suffix('.report.json'),json.dumps(report,indent=2)+'\n')
            print(json.dumps(report),flush=True)
        finally:
            if digest_files(source)!=before or inside('config/play-session.json').read_bytes()!=state_bytes:raise ValueError('Real progress changed during private diagnostic')
            verify_originals()


if __name__=='__main__':main()
