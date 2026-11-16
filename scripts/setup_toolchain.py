"""Download the pinned compiler into this checkout; never install globally."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile
from project_paths import inside


def main():
    lock = json.loads(inside('config/toolchain.lock.json').read_text())
    archive = inside('cache/' + lock['folder'] + '.zip')
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        with urllib.request.urlopen(lock['url'], timeout=120) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != lock['sha256']:
            raise ValueError('Compiler download checksum mismatch')
        archive.write_bytes(data)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != lock['sha256']:
        raise ValueError('Compiler archive checksum mismatch')
    with zipfile.ZipFile(archive) as source:
        for member in source.infolist():
            name = Path(member.filename)
            if name.is_absolute() or '..' in name.parts or ':' in member.filename or name.parts[0] != lock['folder']:
                raise ValueError('Unexpected compiler archive path')
            target = inside(Path('tools') / name)
            if member.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            data = source.read(member)
            if target.exists():
                if target.read_bytes() != data:
                    raise ValueError('Existing compiler file differs: ' + member.filename)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
    print('Pinned Zig toolchain verified.')


if __name__ == '__main__':
    main()
