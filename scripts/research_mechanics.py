"""Read-only tagged HP1 actor properties for local mechanics fixtures."""
import json
import struct
import sys
from unreal_package import Package, Reader
from project_paths import inside


def properties(package, row):
    r = Reader(package.data[row['offset']:row['offset']+row['size']])
    if row['flags'] & 0x02000000:
        node = r.compact(); r.compact(); r.take(12)
        if node: r.compact()
    result = {}
    while True:
        key = package.name(r.compact())
        if key == 'None': break
        info = r.take(1)[0]; kind = info & 15; code = (info >> 4) & 7
        structure = package.name(r.compact()) if kind == 10 else ''
        size = [1, 2, 4, 12, 16][code] if code < 5 else r.unpack({5:'<B', 6:'<H', 7:'<I'}[code])[0]
        index = 0
        if info & 128 and kind != 3:
            index = r.take(1)[0]
            if index & 128:
                if index & 64: index = ((index & 63) << 24) | (r.take(1)[0] << 16) | (r.take(1)[0] << 8) | r.take(1)[0]
                else: index = ((index & 127) << 8) | r.take(1)[0]
        if kind == 3: value = bool(info & 128)
        else:
            data = r.take(size); sub = Reader(data)
            if kind in (5, 8): value = package.object_name(sub.compact())
            elif kind == 6: value = package.name(sub.compact())
            elif kind == 2: value = struct.unpack('<i', data)[0]
            elif kind == 4: value = struct.unpack('<f', data)[0]
            elif kind == 1: value = data[0]
            elif kind == 10 and structure in ('Vector', 'Rotator'): value = struct.unpack('<3f' if structure == 'Vector' else '<3i', data)
            else: value = data.hex()
        result[key + (f'[{index}]' if index else '')] = value
    return result


if __name__ == '__main__':
    for arg in sys.argv[1:]:
        p = Package(inside(arg))
        for row in p.exports:
            if row['class_name'] in ('Mover', 'WingardiumBlock', 'Trigger', 'SpellLearnTrigger', 'PlayerStart'):
                print(json.dumps(dict(map=p.path.name, name=row['name'], klass=row['class_name'], properties=properties(p,row))))
