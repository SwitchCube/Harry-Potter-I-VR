"""Verify real HP1 callbacks and captured pixels from the private stereo fixture."""
import json
import struct
from project_paths import inside


def check(profile,events=None):
    profile=inside(profile)
    if events is None:events=[json.loads(s) for s in (profile/'hp1vr-native.jsonl').read_text().splitlines()]
    if any(e.get('event') in ('fatal','init_error') for e in events):raise ValueError('Native fixture error')
    cleanup=[e for e in events if e.get('event')=='hp1_destroy_fixture']
    if len(cleanup)!=1 or not cleanup[0].get('original_cleanup_once'):raise ValueError('Missing exact original cleanup')
    if not any(e.get('event')=='hp1_status_fixture_restored' and e.get('passed') for e in events):raise ValueError('Status did not restore original values')
    status=next((e for e in events if e.get('event')=='hp1_status_fixture'),{})
    if not status.get('gpu_readback_verified') or status.get('cards',0)<1:raise ValueError('Missing status texture/card integration')
    views=[e for e in events if e.get('event')=='hp1_room_eye']
    if len(views)!=2 or {v['eye'] for v in views}!={0,1} or any(v['source_zone']==v['eye_zone'] for v in views):raise ValueError('Did not exercise stale source-camera zones in both eyes')
    images=[]
    for filename in ('room-eye-a.bmp','room-eye-b.bmp'):
        data=(profile/filename).read_bytes()
        if data[:2]!=b'BM':raise ValueError('Missing BMP image')
        offset=struct.unpack_from('<I',data,10)[0]
        width,height=struct.unpack_from('<ii',data,18)
        if width!=2048 or abs(height)!=2048 or struct.unpack_from('<H',data,28)[0]!=24:raise ValueError('Expected full 2048-square RGB fixture')
        stride=(width*3+3)&~3
        # Sample a regular grid, avoiding dependence on an image library.
        samples=[tuple(data[offset+y*stride+x*3:offset+y*stride+x*3+3]) for y in range(0,abs(height),16) for x in range(0,width,16)]
        lit=sum(max(pixel)>16 for pixel in samples)/len(samples)
        if lit<.5 or len(set(samples))<100:raise ValueError('Empty or corrupted room eye: '+filename)
        images.append(dict(file=filename,lit_fraction=round(lit,5),distinct_samples=len(set(samples))))
    if (profile/'room-eye-a.bmp').read_bytes()==(profile/'room-eye-b.bmp').read_bytes():raise ValueError('Identical stereo eyes')
    return dict(passed=True,cleanup=cleanup[0],status=status,views=views,images=images)


if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('profile');args=p.parse_args()
    print(json.dumps(check(args.profile),indent=2))
