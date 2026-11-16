"""Opt-in 90-second original spell lesson, with real controller poses and input."""
import argparse
import json
import os
import subprocess
from project_paths import ROOT,inside,write_text
from play_sessions import SessionLock,current_progress,digest_files,prepare,stamp
from prepare_target import verify_originals
from probe_session import register,unregister
from tutorial_audio import verify_assets


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--build',required=True);args=parser.parse_args()
    verify_assets()
    with SessionLock():
        state=current_progress();source=inside(state['profile']);before=digest_files(source);state_bytes=inside('config/play-session.json').read_bytes()
        profile=prepare('VR',diagnostic=True)
        write_text(profile/'test-tutorial-live.flag','observe original tutorial; no injected speech\n')
        write_text(profile/'start-lesson.flag','start the native spell-learning trigger after the intro\n')
        build=json.loads(inside(args.build).read_text());log=inside('logs/tutorial-headset-'+stamp()+'.jsonl')
        env=os.environ.copy();env['TEMP']=env['TMP']=str(inside('cache/tmp'))
        print(json.dumps(dict(profile=profile.relative_to(ROOT).as_posix(),log=log.relative_to(ROOT).as_posix())),flush=True)
        try:
            with log.open('xb') as out,subprocess.Popen([str(inside(build['executable'])),str(inside('cache/runtime/System/HP.exe')),str(profile),'90','Lev_Tut1.unr','native-vr'],cwd=ROOT,env=env,stdout=out,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW) as process:
                registration=register(profile.relative_to(ROOT).as_posix(),process.pid)
                try:process.wait(timeout=125)
                finally:
                    if process.poll() is None:
                        write_text(profile/'stop-session.flag','private headset timeout\n',exclusive=False)
                        try:process.wait(timeout=10)
                        except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
                    unregister(registration)
            native=profile/'hp1vr-native.jsonl'
            events=[json.loads(line) for line in native.read_text().splitlines()] if native.is_file() else []
            record=dict(profile=profile.relative_to(ROOT).as_posix(),build=build,exit_code=process.returncode,
                samples=[e for e in events if e.get('event')=='hp1_tutorial_replaced'],
                lesson=[e for e in events if e.get('event') in ('hp1_lesson_begin','hp1_lesson_phase','hp1_tutorial_player_turn')],
                controller_events=[e for e in events if e.get('event','').startswith(('hp1_tutorial_model','hp1_tutorial_driver','hp1_tutorial_visual','hp1_tutorial_hint_used'))],
                errors=[e for e in events if e.get('event') in ('fatal','init_error')]+([] if native.is_file() else [{'event':'startup_incomplete','native_started':False}]),
                shutdown=next((e for e in events if e.get('event')=='hp1_shutdown'),None),user_acceptance_pending=True)
            write_text(log.with_suffix('.report.json'),json.dumps(record,indent=2)+'\n');print(json.dumps(record),flush=True)
        finally:
            if digest_files(source)!=before or inside('config/play-session.json').read_bytes()!=state_bytes:raise ValueError('Real progress changed')
            verify_originals()


if __name__=='__main__':main()
