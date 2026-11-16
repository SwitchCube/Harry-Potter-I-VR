"""Package only this mod and pinned runtimes; no original game or user files."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
from project_paths import ROOT, inside

VERSION = '0.1.1'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', required=True)
    parser.add_argument('--output', default='dist/HP1VR-0.1.1.zip')
    args = parser.parse_args()
    build = inside(args.build)
    record = json.loads((build / 'build.json').read_text())
    for name, expected in record['source_inputs'].items():
        if sha(inside(name).read_bytes()) != expected:
            raise ValueError('Source changed after compilation: ' + name)
    payload = {}
    runtime = []

    def add(name, data, staged=False):
        if name in payload:
            raise ValueError('Duplicate package file: ' + name)
        payload[name] = data
        if staged:
            runtime.append(name)

    for name in ('hp1vr-observer.exe', 'hp1vr-native.dll'):
        add('build/release/' + name, (build / name).read_bytes(), True)
    if sha(payload['build/release/hp1vr-observer.exe']) != record['sha256']:
        raise ValueError('Observer build hash mismatch')
    for name in ('portable_launcher.py',):
        add('scripts/' + name, inside('release/' + name).read_bytes())
    for name in ('play_sessions.py', 'project_paths.py', 'mod_locale.py', 'prepare_target.py',
                 'inspect_pe.py', 'unreal_package.py', 'probe_session.py'):
        add('scripts/' + name, inside('scripts/' + name).read_bytes())
    add('config/target-fingerprint.json', inside('config/target-fingerprint.json').read_bytes(), True)
    for path in sorted(inside('config/vr').glob('*.json')):
        add('config/vr/' + path.name, path.read_bytes(), True)
    for name in ('HP1VRTutorial.uax', 'HP1VRTutorialEN.uax'):
        add('assets/tutorial/' + name, inside('assets/tutorial/' + name).read_bytes(), True)
    openvr = json.loads(inside('config/research-sources.lock.json').read_text())
    for row in openvr['files']:
        if row['local_path'] == 'external/openvr/bin/win32/openvr_api.dll':
            data = inside(row['local_path']).read_bytes()
            if sha(data) != row['sha256']:
                raise ValueError('OpenVR checksum mismatch')
            add(row['local_path'], data, True)
    assert 'external/openvr/bin/win32/openvr_api.dll' in payload
    runtime_lock = json.loads(inside('config/release-dependencies.lock.json').read_text())['files'][0]
    archive = inside(runtime_lock['path']).read_bytes()
    if sha(archive) != runtime_lock['sha256']:
        raise ValueError('Python runtime checksum mismatch')
    import io
    with zipfile.ZipFile(io.BytesIO(archive)) as python:
        for name in python.namelist():
            if name.endswith('/'):
                continue
            data = python.read(name)
            if name.endswith('._pth'):
                data = b'python313.zip\n.\n../scripts\n'
            add('python/' + name, data)
    for destination, source in (
        ('LICENSE', 'LICENSE'),
        ('LICENSING.md', 'LICENSING.md'),
        ('Launcher.ps1', 'release/Launcher.ps1'),
        ('defaults/vr-preferences.ini', 'release/vr-preferences.ini'),
        ('README-DE.txt', 'release/README-DE.txt'),
        ('README-EN.txt', 'release/README-EN.txt'),
        ('THIRD-PARTY.txt', 'release/THIRD-PARTY.txt'),
        ('licenses/OpenVR-LICENSE.txt', 'external/openvr/LICENSE'),
        ('licenses/MinHook-LICENSE.txt', 'external/minhook/LICENSE.txt')):
        add(destination, inside(source).read_bytes())
    for name, path in (
        ('Zig.txt', 'LICENSE'), ('LLVM-libcxx.txt', 'lib/libcxx/LICENSE.TXT'),
        ('LLVM-libcxxabi.txt', 'lib/libcxxabi/LICENSE.TXT'),
        ('LLVM-libunwind.txt', 'lib/libunwind/LICENSE.TXT'),
        ('MinGW.txt', 'lib/libc/mingw/COPYING')):
        add('licenses/' + name, inside('tools/zig-x86_64-windows-0.15.2/' + path).read_bytes())
    manifest = {'version': VERSION, 'files': {n: sha(d) for n, d in sorted(payload.items())},
                'runtime_files': sorted(runtime), 'original_game_included': False}
    add('manifest.json', (json.dumps(manifest, indent=2) + '\n').encode())
    zip_files = {'HP1VR/' + n: d for n, d in payload.items()}
    for name in ('Start-HP1VR.cmd', 'Stop-HP1VR.cmd', 'Check-HP1VR.cmd'):
        zip_files[name] = inside('release/' + name).read_bytes()
    allowed_sounds = {'HP1VR/assets/tutorial/HP1VRTutorial.uax', 'HP1VR/assets/tutorial/HP1VRTutorialEN.uax'}
    for name, data in zip_files.items():
        suffix = Path(name).suffix.lower()
        if suffix in ('.u', '.utx', '.unr', '.usa', '.pdb', '.obj', '.o') or (suffix == '.uax' and name not in allowed_sounds):
            raise ValueError('Unexpected original/build asset: ' + name)
        if Path(name).name.lower() in ('hp.exe', 'default.ini', 'defuser.ini'):
            raise ValueError('Original game file in package: ' + name)
        for token in (str(Path.home()), ROOT.parts[2] if len(ROOT.parts) > 2 and ROOT.parts[1].lower() == 'users' else ''):
            if token and any(token.casefold().encode(encoding) in data.lower() for encoding in ('utf-8', 'utf-16-le', 'utf-16-be')):
                raise ValueError('Personal path in package file: ' + name)
    target = inside(args.output)
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        raise ValueError('Refusing to overwrite an existing release ZIP')
    with zipfile.ZipFile(target, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as output:
        for name, data in sorted(zip_files.items()):
            output.writestr(name, data)
    digest = sha(target.read_bytes())
    target.with_suffix(target.suffix + '.sha256').write_text(digest + '  ' + target.name + '\n')
    receipt = {'version': VERSION, 'files': len(zip_files), 'bytes': target.stat().st_size,
               'sha256': digest, 'build': build.relative_to(ROOT).as_posix()}
    target.with_suffix(target.suffix + '.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt))


if __name__ == '__main__':
    main()
