"""Exercise menu GPU transfers and reloads using an isolated copy of real progress."""
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
from run_probe import menu_capture_coverage


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--borrow',action='store_true',help='Reject extra D3D7 HUD targets in this private test')
    args=parser.parse_args()
    with SessionLock():
        state=current_progress()
        if not state:raise ValueError('A completed checkpoint is required')
        source=inside(state['profile']);before=digest_files(source)
        state_bytes=inside('config/play-session.json').read_bytes()
        profile=prepare('VR',state,diagnostic=True)
        if digest_files(profile)!=before:raise ValueError('Checkpoint copy differs')
        write_text(profile/'play-test.flag','menu checkpoint replay\n')
        if args.borrow:write_text(profile/'test-menu-borrow.flag','isolated HUD target exhaustion\n')
        build=json.loads(inside('config/last-build.json').read_text())
        observer=inside(build['executable'])
        if hashlib.sha256(observer.read_bytes()).hexdigest()!=build['sha256']:raise ValueError('Build differs')
        log=inside('logs/menu-progress-'+stamp()+'.jsonl')
        env=os.environ.copy();env['TEMP']=env['TMP']=str(inside('cache/tmp'))
        print(json.dumps(dict(profile=profile.relative_to(ROOT).as_posix(),log=log.relative_to(ROOT).as_posix(),borrow=args.borrow)),flush=True)
        try:
            with log.open('xb') as out:
                with subprocess.Popen([str(observer),str(inside('cache/runtime/System/HP.exe')),str(profile),'90','save0.usa','native-gameplay'],cwd=ROOT,env=env,stdout=out,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW) as p:
                    registered=register(profile.relative_to(ROOT).as_posix(),p.pid)
                    try:
                        requested=False;deadline=time.monotonic()+115
                        while p.poll() is None:
                            native=profile/'hp1vr-native.jsonl'
                            if not requested and native.exists() and 'hp1_saved_menu_test_complete' in native.read_text():
                                write_text(profile/'stop-session.flag','completed private menu/reload test\n');requested=True
                            if time.monotonic()>deadline:
                                write_text(profile/'stop-session.flag','diagnostic timeout\n',exclusive=False)
                                try:p.wait(timeout=15)
                                except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
                                raise TimeoutError('Owned menu test did not exit')
                            time.sleep(.5)
                    finally:unregister(registered)
            events=[json.loads(line) for line in (profile/'hp1vr-native.jsonl').read_text().splitlines()]
            errors=[e for e in events if e.get('event') in ('fatal','init_error')]
            done=next((e for e in events if e.get('event')=='hp1_saved_menu_test_complete'),None)
            transfers=[e for e in events if e.get('event')=='hp1_menu_transfer']
            if p.returncode or errors or not done or done.get('cycles')!=15 or done.get('reloads')!=4:
                raise ValueError(dict(exit_code=p.returncode,errors=errors,complete=done))
            if len(transfers)<15 or any(not e['verified'] or e['cpu_bytes']>262144 for e in transfers):
                raise ValueError('Full menu GPU transfer was not verified after every reopening')
            if args.borrow and any(e['background_bytes']!=e['width']*e['height']*4 for e in transfers):
                raise ValueError('Borrowed menu background was not preserved')
            report=dict(profile=profile.relative_to(ROOT).as_posix(),log=log.relative_to(ROOT).as_posix(),build=build,borrow=args.borrow,exit_code=p.returncode,complete=done,transfers=transfers,coverage=menu_capture_coverage(profile),source_saves_unchanged=True,state_unchanged=True,originals_verified=True)
            write_text(log.with_suffix('.report.json'),json.dumps(report,indent=2)+'\n')
            print(json.dumps(report),flush=True)
        finally:
            if digest_files(source)!=before or inside('config/play-session.json').read_bytes()!=state_bytes:
                raise ValueError('Real progress changed during private diagnostic')
            verify_originals()


if __name__=='__main__':
    main()
