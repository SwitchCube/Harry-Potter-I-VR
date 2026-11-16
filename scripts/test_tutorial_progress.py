"""Test native voice/text replacement and a deterministic controller driver in a private checkpoint copy."""
import argparse
import hashlib
import json
import os
import subprocess
import time
from project_paths import ROOT, inside, write_text
from play_sessions import SessionLock,current_progress,digest_files,prepare,stamp
from prepare_target import verify_originals
from probe_session import register,unregister
from tutorial_audio import verify_assets


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--build',default='config/last-build.json');args=parser.parse_args()
    verify_assets()
    with SessionLock():
        state=current_progress();source=inside(state['profile']);before=digest_files(source);state_bytes=inside('config/play-session.json').read_bytes()
        profile=prepare('VR',state,diagnostic=True);write_text(profile/'test-tutorial.flag','private tutorial fixture\n')
        build=json.loads(inside(args.build).read_text());observer=inside(build['executable'])
        if hashlib.sha256(observer.read_bytes()).hexdigest()!=build['sha256']:raise ValueError('Observer differs')
        log=inside('logs/tutorial-progress-'+stamp()+'.jsonl');env=os.environ.copy();env['TEMP']=env['TMP']=str(inside('cache/tmp'))
        print(json.dumps(dict(profile=profile.relative_to(ROOT).as_posix(),log=log.relative_to(ROOT).as_posix())),flush=True)
        try:
            with log.open('xb') as out,subprocess.Popen([str(observer),str(inside('cache/runtime/System/HP.exe')),str(profile),'100','save0.usa','native-gameplay'],cwd=ROOT,env=env,stdout=out,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW) as process:
                registration=register(profile.relative_to(ROOT).as_posix(),process.pid)
                try:
                    stop=False;deadline=time.monotonic()+125
                    while process.poll() is None:
                        native=profile/'hp1vr-native.jsonl'
                        if not stop and native.exists() and 'hp1_tutorial_test_complete' in native.read_text():write_text(profile/'stop-session.flag','private tutorial fixture complete\n');stop=True
                        if time.monotonic()>deadline:
                            write_text(profile/'stop-session.flag','private diagnostic timeout\n',exclusive=False)
                            try:process.wait(timeout=10)
                            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
                            raise TimeoutError('Owned tutorial diagnostic timed out')
                        time.sleep(.3)
                finally:unregister(registration)
            events=[json.loads(line) for line in (profile/'hp1vr-native.jsonl').read_text().splitlines()]
            errors=[e for e in events if e.get('event') in ('fatal','init_error')]
            done=next((e for e in events if e.get('event')=='hp1_tutorial_test_complete'),None)
            if process.returncode or errors or not done or not done['passed']:raise ValueError(dict(exit_code=process.returncode,errors=errors,complete=done))
            captures=[f'tutorial-{stage}-eye-{eye}.bmp' for stage in range(1,6) for eye in range(2)]
            for filename in captures:
                if not (profile/filename).is_file():raise ValueError('Missing controller capture: '+filename)
            report=dict(profile=profile.relative_to(ROOT).as_posix(),build=build,exit_code=process.returncode,complete=done,captures=captures,shutdown=next((e for e in events if e.get('event')=='hp1_shutdown'),None),originals_verified=True,source_saves_unchanged=True,state_unchanged=True,headset_test=False)
            write_text(log.with_suffix('.report.json'),json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
        finally:
            if digest_files(source)!=before or inside('config/play-session.json').read_bytes()!=state_bytes:raise ValueError('Real progress changed')
            verify_originals()


if __name__=='__main__':main()
