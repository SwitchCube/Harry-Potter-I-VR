"""Persistent shared Flat/VR progress; diagnostics never activate user progress."""
import argparse
import configparser
import csv
import hashlib
import json
import msvcrt
import os
import re
import shutil
import subprocess
import time
import uuid
from datetime import datetime
from pathlib import Path
from project_paths import ROOT, inside, walk_files, write_text
from mod_locale import tr
from prepare_target import verify_originals, prepare_lobby
from probe_session import register, unregister
from unreal_package import Package

STATE = 'config/play-session.json'


def stamp():
    return datetime.now().strftime('%Y%m%d-%H%M%S-') + uuid.uuid4().hex[:6]


def atomic_json(path, value):
    path = inside(path)
    if path.exists() and path.stat().st_nlink != 1:
        raise ValueError('Hardlinked state file')
    tmp = write_text(str(path) + '.' + uuid.uuid4().hex + '.tmp', json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    os.replace(tmp, path)


def processes(name):
    result = subprocess.run(['tasklist.exe', '/FI', 'IMAGENAME eq ' + name, '/FO', 'CSV', '/NH'], capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW, check=True)
    return [row for row in csv.reader(result.stdout.decode('oem', errors='replace').splitlines()) if row and row[0].casefold() == name.casefold()]


class SessionLock:
    def __enter__(self):
        path = inside('cache/play-session.lock'); path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists() and path.stat().st_nlink != 1:
            raise ValueError('Hardlinked session lock')
        self.file = path.open('a+b'); self.file.seek(0)
        if path.stat().st_size == 0:
            self.file.write(b'0'); self.file.flush()
        self.file.seek(0)
        try:
            msvcrt.locking(self.file.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError:
            self.file.close()
            raise ValueError(tr('Eine Spielsitzung wird bereits gestartet oder läuft noch.', 'A game session is already starting or running.'))
        try:
            if processes('HP.exe'):
                raise ValueError(tr('Harry Potter läuft bereits. Bitte die laufende Sitzung erst beenden.', 'Harry Potter is already running. Please close the current session first.'))
        except Exception:
            self.__exit__(None, None, None)
            raise
        return self

    def __exit__(self, *args):
        self.file.seek(0); msvcrt.locking(self.file.fileno(), msvcrt.LK_UNLCK, 1); self.file.close()


def current_progress():
    path = inside(STATE)
    if not path.exists():
        return None
    state = json.loads(path.read_text(encoding='utf-8-sig'))
    profile = inside(state['profile'])
    if profile.parent != inside('cache/profiles') or not profile.name.startswith('play-') or not (profile/'play-profile.json').is_file():
        raise ValueError(tr('Ungültiger Verweis auf den letzten Spieldurchgang.', 'Invalid reference to the previous play session.'))
    return state


def progress_files(profile):
    profile = inside(profile)
    result = []
    for path in walk_files(profile):
        rel = path.relative_to(profile); name = path.name.casefold()
        if name.endswith(('.tmp', '.temp')):
            continue
        if path.stat().st_nlink != 1:
            raise ValueError('Hardlinked profile file')
        if path.suffix.casefold() == '.usa' or name.startswith('gamesaveinfo') or (rel.parts[0].casefold() == 'save' and path.suffix.casefold() == '.bmp'):
            result.append(path)
    return sorted(result)


def digest_files(profile):
    return {p.relative_to(profile).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in progress_files(profile)}


def checked_copy(source, destination):
    source = inside(source); destination = inside(destination)
    if destination.exists() and destination.stat().st_nlink != 1:
        raise ValueError('Hardlinked destination')
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def snapshot(profile, reason):
    """Copy only stable completed files, never a transient Save.tmp."""
    profile = inside(profile); before = digest_files(profile)
    if not before:
        return None
    for rel in before:
        if rel.casefold().endswith('.usa'):
            Package(profile / rel)  # Reject truncated package tables/data.
    destination = inside('backups/play/' + stamp()); destination.mkdir(parents=True)
    for rel in before:
        checked_copy(profile/rel, destination/rel)
    after = digest_files(profile)
    copied = digest_files(destination)
    if before != after or before != copied:
        atomic_json(destination/'incomplete.json', {'reason': 'save_changed_during_snapshot'})
        return None
    atomic_json(destination/'manifest.json', {'schema': 1, 'profile': profile.relative_to(ROOT).as_posix(), 'reason': reason, 'files': before})
    return destination


def profile_ini(text, profile, mode):
    """Keep native settings but rebase all writable/search paths per session."""
    profile = inside(profile); source = inside('Harry Potter TM')
    values = dict(SavePath=str(profile/'Save'), CachePath=str(profile/'Cache'), Reconfig='0', FirstRun='433')
    if mode == 'VR':
        prefs = configparser.ConfigParser()
        prefs.read(inside('config/vr-preferences.ini'), encoding='utf-8-sig')
        try:
            size = prefs.getint('VR', 'RenderSize', fallback=2048)
        except ValueError:
            size = 2048
        if size not in (1024, 1280, 1536, 2048):
            size = 2048
        values.update(WindowedViewportX=str(size), WindowedViewportY=str(size), WindowedColorBits='32', StartupFullscreen='False',
                      GameRenderDevice='D3DDrv.D3DRenderDevice', WindowedRenderDevice='D3DDrv.D3DRenderDevice', RenderDevice='D3DDrv.D3DRenderDevice', UseJoystick='False', CaptureMouse='False')
    lines = []
    for line in text.splitlines():
        key, sep, value = line.partition('=')
        if key == 'ServerActors':
            continue
        if key in values:
            line = key+'='+values[key]
        elif key == 'Paths':
            extension = Path(value.replace('\\', '/')).name
            folder = {'*.u': 'System', '*.unr': 'Maps', '*.utx': 'Textures', '*.uax': 'Sounds', '*.umx': 'Music'}.get(extension.casefold())
            if extension.casefold() == '*.usa':
                line = 'Paths='+str(profile/'Save/*.usa')
            elif folder:
                line = 'Paths='+str(source/folder/extension)
            else:
                raise ValueError(tr('Ungeprüfter Suchpfad in den Spieleinstellungen: ', 'Unverified search path in the game settings: ')+value)
        lines.append(line)
    return '\n'.join(lines)+'\n'


def prepare(mode, previous=None, diagnostic=False):
    if mode == 'VR':prepare_lobby()
    verify_originals()
    source = inside('Harry Potter TM'); runtime = inside('cache/runtime/System'); runtime.mkdir(parents=True, exist_ok=True)
    for path in walk_files(source/'System'):
        target = runtime/path.relative_to(source/'System')
        if not target.exists():
            checked_copy(path, target)
    # The original slot menu reads its bundled thumbnail artwork relatively.
    for path in walk_files(source/'save'):
        target = inside('cache/runtime/Save')/path.relative_to(source/'save')
        if not target.exists():
            checked_copy(path, target)
    profile = inside('cache/profiles/' + ('launcher-test-' if diagnostic else 'play-') + stamp())
    profile.mkdir(parents=True); (profile/'Save').mkdir(); (profile/'Cache').mkdir()
    prev = inside(previous['profile']) if previous else None
    if prev:
        for path in progress_files(prev):
            if path.suffix.casefold() == '.usa':
                Package(path)
            checked_copy(path, profile/path.relative_to(prev))
    default = (source/'System/Default.ini').read_text(encoding='cp1252')
    # Ordinary renderer settings for the unmodified Flat game. VR keeps its own.
    flat = default
    initial = {'ForceSoftware':'0', 'GameRenderDevice':'D3DDrv.D3DRenderDevice', 'WindowedRenderDevice':'D3DDrv.D3DRenderDevice', 'RenderDevice':'D3DDrv.D3DRenderDevice',
               'WindowedViewportX':'1024', 'WindowedViewportY':'768', 'WindowedColorBits':'32', 'StartupFullscreen':'False', 'CacheSizeMegs':'64'}
    flat = '\n'.join((line.split('=',1)[0]+'='+initial[line.split('=',1)[0]]) if '=' in line and line.split('=',1)[0] in initial else line for line in flat.splitlines())+'\n'
    for option in ('Flat','VR'):
        old = prev/('HP.ini' if previous['mode'] == option else option+'.ini') if prev else None
        text = old.read_text(encoding='utf-8-sig') if old and old.is_file() else flat
        write_text(profile/(option+'.ini'), profile_ini(text, profile, option))
    # HP1 compares configuration timestamps during its original startup.
    # Create the active INI last so fresh defaults cannot invalidate it.
    checked_copy(profile/(mode+'.ini'), profile/'Default.ini')
    checked_copy(profile/'Default.ini', runtime/'Default.ini')
    checked_copy(profile/'Default.ini', profile/'HP.ini')
    for filename in ('User.ini','DefUser.ini'):
        if prev and (prev/filename).is_file(): checked_copy(prev/filename, profile/filename)
        else: write_text(profile/filename, (source/'System/DefUser.ini').read_text(encoding='cp1252'))
    # The 2001 engine compares whole-second ages. Copy order alone is not
    # sufficient when every file is created within the same second.
    active_time=time.time()
    for path in (profile/'Default.ini',runtime/'Default.ini',profile/'DefUser.ini'):
        os.utime(path,(active_time-3,active_time-3))
    for path in (profile/'HP.ini',profile/'User.ini'):
        os.utime(path,(active_time,active_time))
    atomic_json(profile/'play-profile.json', {'schema': 1, 'mode': mode, 'diagnostic': diagnostic, 'previous': previous})
    return profile


def run(mode, diagnostic=False, previous=None, seconds=0, scenario=None, load=False):
    if mode not in ('Flat','VR'):
        raise ValueError(tr('Ungültiger Spielmodus', 'Invalid game mode'))
    if not diagnostic and (seconds or scenario or load):
        raise ValueError(tr('Normale Spielsitzungen haben keine Testeingaben oder Zeitbegrenzung', 'Normal play sessions cannot use test input or a time limit'))
    with SessionLock():
        if mode == 'VR' and not diagnostic and not processes('vrserver.exe'):
            raise ValueError(tr('SteamVR ist nicht gestartet. Bitte SteamVR starten und das Headset verbinden. Flat funktioniert ohne SteamVR.', 'SteamVR is not running. Start SteamVR and connect your headset. Flat works without SteamVR.'))
        prior = previous if diagnostic else current_progress()
        if prior and not diagnostic:
            snapshot(inside(prior['profile']), 'before_next_session')
        profile = prepare(mode, prior, diagnostic)
        print(json.dumps({'event': 'play_profile_ready', 'profile': profile.relative_to(ROOT).as_posix(), 'mode': mode, 'diagnostic': diagnostic}), flush=True)
        build = json.loads(inside('config/last-build.json').read_text(encoding='utf-8-sig'))
        observer = inside(build['executable'])
        if hashlib.sha256(observer.read_bytes()).hexdigest() != build['sha256']:
            raise ValueError(tr('Starter-Build wurde verändert. Bitte neu bauen.', 'The launcher build has changed. Please rebuild it.'))
        if scenario:
            write_text(profile/'play-test.flag', scenario)
        log_path = inside('logs/play-'+stamp()+'.jsonl')
        state = {'schema': 1, 'profile': profile.relative_to(ROOT).as_posix(), 'mode': mode, 'log': log_path.relative_to(ROOT).as_posix(), 'status': 'started'}
        if not diagnostic:
            atomic_json(STATE, state)  # Completed book saves survive even an unclean exit.
        engine_mode = 'flat' if mode == 'Flat' else ('native-gameplay' if diagnostic else 'native-vr')
        cmd = [str(observer), str(inside('cache/runtime/System/HP.exe')), str(profile), str(seconds), 'save0.usa' if load else 'Startup.unr', engine_mode]
        env = os.environ.copy(); env['TEMP'] = env['TMP'] = str(inside('cache/tmp'))
        inside('cache/tmp').mkdir(parents=True, exist_ok=True)
        snapshots = 0; last = {}; candidate = None; stable = 0; warnings = []
        with log_path.open('xb') as out:
            with subprocess.Popen(cmd, cwd=ROOT, env=env, stdout=out, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW) as process:
                registered = register(state['profile'], process.pid)
                try:
                    while process.poll() is None:
                        time.sleep(.5)
                        try:
                            current = digest_files(profile)
                            stable = stable+1 if current == candidate else 0; candidate = current
                            if current and current != last and stable >= 3:
                                if snapshot(profile, 'completed_checkpoint'):
                                    last = current; snapshots += 1
                        except (OSError, ValueError):
                            stable = 0  # In-progress/locked/truncated saves are never backed up as complete.
                finally:
                    unregister(registered)
        try:
            checked_copy(profile/'HP.ini', profile/(mode+'.ini'))
            if snapshot(profile, 'session_exit'): snapshots += 1
        except (OSError, ValueError) as error:
            warnings.append(str(error))
        verify_originals()
        state.update(status='completed' if process.returncode == 0 else 'interrupted', exit_code=process.returncode, snapshots=snapshots, warnings=warnings, originals_verified=True)
        if not diagnostic: atomic_json(STATE, state)
        atomic_json(profile/'play-result.json', state)
        print(json.dumps(state, ensure_ascii=False), flush=True)
        return state


if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--mode', choices=['Flat','VR'], required=True)
    args = parser.parse_args()
    try:
        result = run(args.mode)
        raise SystemExit(0 if result['exit_code'] == 0 else 1)
    except Exception as error:
        print(json.dumps({'error': str(error)}, ensure_ascii=False), flush=True)
        raise SystemExit(1)
