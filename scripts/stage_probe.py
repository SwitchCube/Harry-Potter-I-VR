"""Create a private HP1 runtime/profile without touching original game files."""
import json
import shutil
import argparse
import os
import time
from datetime import datetime
from project_paths import inside, walk_files, write_text
from prepare_target import verify_originals


def stage(resolution=800):
    verify_originals()
    source = inside('Harry Potter TM')
    runtime = inside('cache/runtime/System')
    runtime.mkdir(parents=True, exist_ok=True)
    for path in walk_files(source / 'System'):
        dest = inside(runtime / path.relative_to(source / 'System'))
        dest.parent.mkdir(parents=True, exist_ok=True)
        if not dest.exists():
            shutil.copyfile(path, dest)
    profile = inside('cache/profiles/probe-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    profile.mkdir(parents=True)
    for folder in ('Save/Slot1', 'Cache'):
        (profile / folder).mkdir(parents=True)
    text = (source / 'System/Default.ini').read_text(encoding='cp1252')
    values = dict(Reconfig='0',FirstRun='433',ForceSoftware='0',
        GameRenderDevice='D3DDrv.D3DRenderDevice',WindowedRenderDevice='D3DDrv.D3DRenderDevice',
        RenderDevice='D3DDrv.D3DRenderDevice',WindowedViewportX=str(resolution),WindowedViewportY=str(600 if resolution==800 else resolution),
        WindowedColorBits='32',FullscreenColorBits='32',CaptureMouse='False',
        StartupFullscreen='False',UseJoystick='False',CacheSizeMegs='64',
        SavePath=str(profile / 'Save'),CachePath=str(profile / 'Cache'))
    lines=[]
    for line in text.splitlines():
        key, sep, value = line.partition('=')
        if key in values:
            line = key + '=' + values[key]
        elif key == 'Paths':
            line = 'Paths=' + str(source / value.removeprefix('../'))
        elif key == 'ServerActors':
            continue  # Private offline probe; no public discovery services.
        lines.append(line)
    text = '\n'.join(lines) + '\n'
    write_text(profile / 'Default.ini', text)
    write_text(runtime / 'Default.ini', text, exclusive=False)
    write_text(profile / 'HP.ini', text)  # Active INI must be newest.
    user = (source / 'System/DefUser.ini').read_text(encoding='cp1252')
    for name in ('User.ini', 'DefUser.ini'):
        write_text(profile / name, user)
    active_time=time.time()
    for path in (profile/'Default.ini',runtime/'Default.ini',profile/'DefUser.ini'):
        os.utime(path,(active_time-3,active_time-3))
    for path in (profile/'HP.ini',profile/'User.ini'):
        os.utime(path,(active_time,active_time))
    record = dict(profile=profile.relative_to(inside('.')).as_posix(),
                  executable='cache/runtime/System/HP.exe', originals_unchanged=True)
    write_text('config/last-probe.json', json.dumps(record, indent=2)+'\n', exclusive=False)
    print(json.dumps(record))


if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--resolution',type=int,choices=[800,1280,1536,2048],default=800)
    stage(parser.parse_args().resolution)
