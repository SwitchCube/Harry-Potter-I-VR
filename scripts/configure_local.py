"""Copy only verified original inputs into the ignored development directory."""
import argparse
import hashlib
import json
from pathlib import Path
from project_paths import inside, no_links


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-directory', required=True)
    args = parser.parse_args()
    source = no_links(Path(args.game_directory).absolute())
    manifest = json.loads(inside('config/target-fingerprint.json').read_text(encoding='utf-8-sig'))
    for row in manifest['files']:
        relative = Path(row['path']).relative_to('Harry Potter TM')
        data = no_links(source / relative).read_bytes()
        if len(data) != row['bytes'] or hashlib.sha256(data).hexdigest() != row['sha256']:
            raise ValueError('Unsupported or modified game input: ' + relative.as_posix())
    for row in manifest['files']:
        relative = Path(row['path']).relative_to('Harry Potter TM')
        target = inside(row['path'])
        data = no_links(source / relative).read_bytes()
        if target.exists():
            if target.read_bytes() != data:
                raise ValueError('Existing development input differs: ' + relative.as_posix())
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    print('Verified game inputs copied; original installation and saves unchanged.')


if __name__ == '__main__':
    main()
