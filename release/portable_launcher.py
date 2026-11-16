"""Portable HP1 setup and session entry point; bundled Python, no pip needed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

VERSION = '0.1.1'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_relative(name):
    path = Path(name)
    if path.is_absolute() or '..' in path.parts or ':' in name or not path.parts:
        raise ValueError('Invalid package path')
    return path


def verify_package(package):
    from project_paths import no_links
    manifest = json.loads(no_links(package / 'manifest.json').read_text(encoding='utf-8'))
    if manifest['version'] != VERSION:
        raise ValueError('Package version mismatch')
    for name, expected in manifest['files'].items():
        if sha(no_links(package / safe_relative(name)).read_bytes()) != expected:
            raise ValueError('Package file changed: ' + name)
    return manifest


def check_game(package, game):
    from project_paths import no_links
    fingerprint = json.loads((package / 'config/target-fingerprint.json').read_text(encoding='utf-8-sig'))
    for row in fingerprint['files']:
        relative = safe_relative(row['path']).relative_to('Harry Potter TM')
        try:
            data = no_links(game / relative).read_bytes()
        except FileNotFoundError:
            raise ValueError('Required game file missing: ' + relative.as_posix()) from None
        if len(data) != row['bytes'] or sha(data) != row['sha256']:
            raise ValueError('Unsupported or modified game file: ' + relative.as_posix())
    return fingerprint


def setup_data(package, game, data, manifest, fingerprint):
    from project_paths import inside, no_links
    inside(data).mkdir(parents=True, exist_ok=True)
    for folder in ('logs', 'cache/tmp', 'config'):
        inside(folder).mkdir(parents=True, exist_ok=True)
    # Keep the tested HP1 runtime layout. Only fingerprinted stock files are
    # copied; personal saves and settings are never imported or overwritten.
    for row in fingerprint['files']:
        relative = safe_relative(row['path']).relative_to('Harry Potter TM')
        target = inside(row['path'])
        if target.exists():
            if sha(target.read_bytes()) != row['sha256']:
                raise ValueError('Private original-game copy changed: ' + relative.as_posix())
            continue
        content = no_links(game / relative).read_bytes()
        if sha(content) != row['sha256']:
            raise ValueError('Game input changed during setup')
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as handle:
            handle.write(content)
    for name in manifest['runtime_files']:
        relative = safe_relative(name)
        content = no_links(package / relative).read_bytes()
        if sha(content) != manifest['files'][name]:
            raise ValueError('Package changed during setup')
        target = inside(relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists() and target.stat().st_nlink != 1:
            raise ValueError('Hardlinked runtime file')
        if not target.exists() or target.read_bytes() != content:
            target.write_bytes(content)
    preferences = inside('config/vr-preferences.ini')
    if not preferences.exists():
        preferences.write_bytes((package / 'defaults/vr-preferences.ini').read_bytes())
    build = {
        'executable': 'build/release/hp1vr-observer.exe',
        'native_dll': 'build/release/hp1vr-native.dll',
        'sha256': manifest['files']['build/release/hp1vr-observer.exe'],
        'target': 'x86-windows-gnu',
        'version': VERSION,
    }
    from play_sessions import atomic_json
    atomic_json('config/last-build.json', build)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-directory', type=Path)
    parser.add_argument('--data-root', type=Path)
    parser.add_argument('--mode', choices=['Flat', 'VR'])
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--stop', action='store_true')
    args = parser.parse_args()
    package = Path(__file__).absolute().parent.parent
    game = (args.game_directory or package.parent).absolute()
    key = sha(str(game).casefold().encode())[:12]
    data = (args.data_root or Path(os.environ['LOCALAPPDATA']) / 'HP1VR' / key).absolute()
    if data == game or data == package or game.is_relative_to(data):
        raise ValueError('Runtime data must be separate from the original game')
    os.environ['HP1VR_DATA_ROOT'] = str(data)
    # Set before importing project modules: their root is fixed at import time.
    from project_paths import no_links
    no_links(game); no_links(data); no_links(package)
    if args.stop:
        from probe_session import request_stop
        print(json.dumps({'stop_requested': request_stop()}))
        return 0
    manifest = verify_package(package)
    fingerprint = check_game(package, game)
    if args.check:
        print(json.dumps({'version': VERSION, 'package_verified': True,
                          'game_files_verified': len(fingerprint['files']), 'read_only': True}))
        return 0
    if not args.mode and not args.prepare_only:
        parser.error('Choose --mode Flat or --mode VR')
    from play_sessions import SessionLock
    with SessionLock():
        setup_data(package, game, data, manifest, fingerprint)
    if args.prepare_only:
        print(json.dumps({'prepared': True, 'version': VERSION}))
        return 0
    from play_sessions import run
    result = run(args.mode)
    return 0 if result['exit_code'] == 0 else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
