"""Native Flute inputs from a copied checkpoint; no real save activation/writes."""
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
    with SessionLock():
        state=current_progress()
        if not state:raise ValueError('A Flute checkpoint is required')
        source=inside(state['profile']); before=digest_files(source)
        state_bytes=inside('config/play-session.json').read_bytes()
        profile=prepare('VR',state,diagnostic=True)
        if digest_files(profile)!=before:raise ValueError('Checkpoint copy differs')
        write_text(profile/'test-flute.flag','private copied checkpoint controls\n')
        build=json.loads(inside('config/last-build.json').read_text())
        observer=inside(build['executable'])
        if hashlib.sha256(observer.read_bytes()).hexdigest()!=build['sha256']:raise ValueError('Build differs')
        log=inside('logs/flute-progress-'+stamp()+'.jsonl')
        env=os.environ.copy();env['TEMP']=env['TMP']=str(inside('cache/tmp'))
        print(json.dumps(dict(profile=profile.relative_to(ROOT).as_posix(),log=log.relative_to(ROOT).as_posix())),flush=True)
        try:
            with log.open('xb') as out:
                with subprocess.Popen([str(observer),str(inside('cache/runtime/System/HP.exe')),str(profile),'90','save0.usa','native-gameplay'],cwd=ROOT,env=env,stdout=out,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW) as p:
                    registered=register(profile.relative_to(ROOT).as_posix(),p.pid)
                    try:
                        requested=False; deadline=time.monotonic()+115
                        while p.poll() is None:
                            native=profile/'hp1vr-native.jsonl'
                            if not requested and native.exists() and 'hp1_flute_test_complete' in native.read_text():
                                write_text(profile/'stop-session.flag','completed private flute test\n'); requested=True
                            if time.monotonic()>deadline:
                                write_text(profile/'stop-session.flag','diagnostic timeout\n',exclusive=False)
                                try:p.wait(timeout=15)
                                except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
                                raise TimeoutError('Owned Flute test did not exit')
                            time.sleep(.4)
                    finally:unregister(registered)
            events=[json.loads(line) for line in (profile/'hp1vr-native.jsonl').read_text().splitlines()]
            errors=[e for e in events if e.get('event') in ('fatal','init_error')]
            done=next((e for e in events if e.get('event')=='hp1_flute_test_complete'),None)
            effects=[e for e in events if e.get('event')=='hp1_flute_test_effect']
            meshes=[e for e in events if e.get('event')=='hp1_flute_mesh']
            if p.returncode or errors or not done or not done['passed'] or len(effects)!=3 or not all(e['passed'] for e in effects) or {e['eye'] for e in meshes}!={0,1}:
                raise ValueError(dict(exit_code=p.returncode,errors=errors,complete=done,effects=effects,meshes=meshes))
            for eye in ('a','b'):
                for label in ('flute-eye','flute-tilt'):
                    if not (profile/f'{label}-{eye}.bmp').is_file():raise ValueError('Missing stereo flute capture')
            if digest_files(source)!=before or inside('config/play-session.json').read_bytes()!=state_bytes:raise ValueError('Real progress changed')
            verify_originals()
            report=dict(profile=profile.relative_to(ROOT).as_posix(),log=log.relative_to(ROOT).as_posix(),build=build,exit_code=p.returncode,complete=done,effects=effects,meshes=meshes,source_saves_unchanged=True,state_unchanged=True,originals_verified=True,headset_test=False)
            write_text(log.with_suffix('.report.json'),json.dumps(report,indent=2)+'\n')
            print(json.dumps(report),flush=True)
        finally:
            if digest_files(source)!=before or inside('config/play-session.json').read_bytes()!=state_bytes:raise ValueError('Real progress changed during private diagnostic')
            verify_originals()


if __name__=='__main__':main()
