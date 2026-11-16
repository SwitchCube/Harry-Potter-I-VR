"""Compile/run self-contained native references with all output inside HP1."""
import json
import os
import subprocess
from datetime import datetime
from project_paths import ROOT, inside, write_text


def main():
    stamp=datetime.now().strftime('%Y%m%d-%H%M%S')
    folder=inside('build/tests-'+stamp)
    folder.mkdir(parents=True)
    env=os.environ.copy()
    for key,path in [('TEMP','cache/tmp'),('TMP','cache/tmp'),('ZIG_GLOBAL_CACHE_DIR','cache/zig-global'),('ZIG_LOCAL_CACHE_DIR','cache/zig-local')]:
        env[key]=str(inside(path))
        inside(path).mkdir(parents=True,exist_ok=True)
    report=[]
    for test in ['hp1_preferences_tests','vr_math_tests','vr_pixels_tests','vr_hud_capture_tests','vr_bridge_tests','vr_bridge_direct_tests','hp1_letter_panel_tests','hp1_menu_raster_tests','menu_pointer_tests','hp1_hand_stereo_tests','hp1_lesson_tests','hp1_flight_tests','hp1_levitation_tests','hp1_troll_tests']:
        exe=folder/(test+'.exe')
        source='vr_bridge_tests' if test=='vr_bridge_direct_tests' else test
        command=[str(inside('tools/zig-x86_64-windows-0.15.2/zig.exe')),'c++','-target','x86-windows-gnu','-std=c++17','-O2','-msse2','-I'+str(inside('src')),'-I'+str(inside('external/openvr/headers')),str(inside('tests/'+source+'.cpp')),'-o',str(exe)]
        if test=='vr_hud_capture_tests':
            command+=['-lddraw','-ldxguid']
        if test in ('vr_bridge_tests','vr_bridge_direct_tests'):
            command+=['-ld3d11','-ldxgi','-ldxguid','-lpsapi']
        if test=='vr_bridge_direct_tests':
            command+=['-DHP1VR_DIRECT_BGRX=1']
        if test=='hp1_letter_panel_tests':
            command+=['-ld3d11','-ldxgi','-ldxguid','-lpsapi','-lgdi32','-luser32']
        if test=='hp1_menu_raster_tests':
            command+=['-lgdi32','-luser32']
        compiled=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True)
        write_text(folder/(test+'-build.txt'),compiled.stdout+compiled.stderr)
        compiled.check_returncode()
        result=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True,text=True,timeout=30)
        write_text(folder/(test+'-run.txt'),result.stdout+result.stderr)
        print(result.stdout+result.stderr,end='')
        report.append(dict(test=test,exit_code=result.returncode))
        result.check_returncode()
    write_text('logs/native-tests-'+stamp+'.json',json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    main()
