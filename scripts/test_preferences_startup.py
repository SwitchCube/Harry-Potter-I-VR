"""Private next-launch resolution and language integration, without real preferences."""
import json
import os
import subprocess
import time
from unittest.mock import patch
from project_paths import ROOT, inside, write_text
import play_sessions as sessions
from prepare_target import verify_originals


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', required=True)
    args = parser.parse_args()
    with sessions.SessionLock():
        state = sessions.current_progress()
        source = inside(state['profile'])
        before = sessions.digest_files(source)
        state_bytes = inside('config/play-session.json').read_bytes()
        profile = sessions.prepare('VR', state, diagnostic=True)
        write_text(profile/'test-vr-settings.flag', 'private startup preferences\n')
        write_text(profile/'test-passive.flag', 'no synthetic gameplay input\n')
        pref = write_text(profile/'vr-preferences.ini', '[VR]\nLanguage=2\nRenderSize=1024\nSpatialScenes=1\n')
        original = (profile/'HP.ini').read_text(encoding='utf-8-sig')
        with patch.object(sessions, 'inside', side_effect=lambda path: pref if str(path)=='config/vr-preferences.ini' else inside(path)):
            modified = sessions.profile_ini(original, profile, 'VR')
            assert 'WindowedViewportX=1024' in modified
            assert 'WindowedViewportX=2048' in sessions.profile_ini(original, profile, 'Flat')
        for name in ('HP.ini', 'Default.ini'):
            write_text(profile/name, modified, exclusive=False)
        now=time.time()
        os.utime(profile/'Default.ini',(now-5,now-5))
        os.utime(profile/'HP.ini',(now,now))
        build=json.loads(inside(args.build).read_text())
        log=inside('logs/preferences-startup-'+sessions.stamp()+'.jsonl')
        env=os.environ.copy();env['TEMP']=env['TMP']=str(inside('cache/tmp'))
        try:
            with log.open('xb') as output:
                result=subprocess.run([str(inside(build['executable'])),str(inside('cache/runtime/System/HP.exe')),str(profile),'15','Startup.unr','native-gameplay'],cwd=ROOT,env=env,stdout=output,stderr=subprocess.STDOUT,timeout=40,creationflags=subprocess.CREATE_NO_WINDOW)
            result.check_returncode()
            events=[json.loads(line) for line in (profile/'hp1vr-native.jsonl').read_text().splitlines()]
            assert any(e.get('event')=='hp1_stereo_frame' and e.get('width')==1024 and e.get('height')==1024 for e in events)
            assert any(e.get('event')=='hp1_preferences_loaded' and e.get('english') for e in events)
            assert not any(e.get('event') in ('fatal','init_error','hp1_native_fault') for e in events)
            report=dict(profile=profile.relative_to(ROOT).as_posix(),stereo_1024=True,flat_resolution_preserved=True,private_preferences=True,originals_verified=True,headset_test=False)
            write_text(log.with_suffix('.report.json'),json.dumps(report,indent=2)+'\n')
            print(json.dumps(report))
        finally:
            assert before==sessions.digest_files(source)
            assert state_bytes==inside('config/play-session.json').read_bytes()
            verify_originals()


if __name__=='__main__':main()
