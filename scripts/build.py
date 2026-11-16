"""Local x86 toolchain; all compiler output/cache/temp stays inside HP1."""
import hashlib
import argparse
import json
import os
import subprocess
from datetime import datetime
from project_paths import ROOT, inside, write_text
from prepare_target import main as prepare
from inspect_pe import PE
from validate_actions import main as validate_actions


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--no-activate',action='store_true');args=parser.parse_args()
    from tutorial_audio import verify_assets, pack
    pack()
    verify_assets()
    import tutorial_audio
    tutorial_audio.LANGUAGE="en";tutorial_audio.FOLDER="assets/tutorial/en";tutorial_audio.PACKAGE="HP1VRTutorialEN"
    pack()
    verify_assets()
    tutorial_audio.LANGUAGE="de";tutorial_audio.FOLDER="assets/tutorial/de";tutorial_audio.PACKAGE="HP1VRTutorial"
    validate_actions()
    prepare()
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    build = inside('build/observer-' + stamp)
    build.mkdir(parents=True)
    env = os.environ.copy()
    for name, path in [('TEMP','cache/tmp'),('TMP','cache/tmp'),
                       ('ZIG_GLOBAL_CACHE_DIR','cache/zig-global'),('ZIG_LOCAL_CACHE_DIR','cache/zig-local')]:
        folder = inside(path)
        folder.mkdir(parents=True,exist_ok=True)
        env[name] = str(folder)
    zig = inside('tools/zig-x86_64-windows-0.15.2/zig.exe')
    objects=[]
    for source in ['buffer.c','hook.c','trampoline.c','hde/hde32.c']:
        out=build/(source.replace('/','-')+'.o')
        subprocess.run([str(zig),'cc','-target','x86-windows-gnu','-O2','-c',
            str(inside('external/minhook/src/'+source)),'-o',str(out)],cwd=ROOT,env=env,check=True)
        objects.append(str(out))
    dll=build/'hp1vr-native.dll'
    command=[str(zig),'c++','-target','x86-windows-gnu','-std=c++17','-msse2','-O2','-g0','-s','-shared',
        '-ffile-prefix-map='+str(ROOT)+'=.',
        '-DHP1VR_DIRECT_BGRX=1','-I'+str(inside('external/openvr/headers')),
        '-I'+str(inside('external/minhook/include')),str(inside('src/hp1vr_native.cpp')),
        *objects,'-lddraw','-ldxguid','-ld3d11','-ldxgi','-lpsapi','-lgdi32','-luser32','-o',str(dll)]
    run=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True)
    write_text('logs/build-native-'+stamp+'.txt',run.stdout+run.stderr)
    if run.returncode:
        print(run.stderr)
        raise SystemExit(run.returncode)
    pe=PE(dll.read_bytes())
    init=next(e for e in pe.exports() if 'HP1VR_Initialize' in e['name'])
    write_text('src/native_launcher_target.h','#pragma once\nstatic const char* kNativeDllSha="%s";\nstatic const unsigned long kNativeInitRva=%s;\n' % (hashlib.sha256(dll.read_bytes()).hexdigest(),init['rva']),exclusive=False)
    exe = build / 'hp1vr-observer.exe'
    command = [str(zig),'c++','-target','x86-windows-gnu','-std=c++17','-O2','-g0','-s','-municode',
               '-ffile-prefix-map='+str(ROOT)+'=.',
               str(inside('src/engine_observer.cpp')),'-lbcrypt','-lpsapi','-o',str(exe)]
    run = subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True)
    write_text('logs/build-observer-'+stamp+'.txt',run.stdout+run.stderr)
    if run.returncode:
        print(run.stderr)
        raise SystemExit(run.returncode)
    record=dict(executable=exe.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                target='x86-windows-gnu',compiler='zig 0.15.2',vr=False,
                native_dll=dll.relative_to(ROOT).as_posix())
    record['source_inputs'] = {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
        for folder in ('src', 'scripts', 'config/vr') for p in sorted(inside(folder).rglob('*'))
        if p.is_file() and p.suffix in ('.cpp', '.h', '.py', '.json', '.ps1') and '__pycache__' not in p.parts}
    write_text(build/'build.json',json.dumps(record,indent=2)+'\n')
    if not args.no_activate:write_text('config/last-build.json',json.dumps(record,indent=2)+'\n',exclusive=False)
    print(json.dumps(record))


if __name__ == '__main__':
    main()
