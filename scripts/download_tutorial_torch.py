"""Bounded HTTP range download for the local build-only TTS CUDA runtime."""
import concurrent.futures
import hashlib
import re
import urllib.request
from project_paths import inside

filename='torch-2.8.0%2Bcu126-cp313-cp313-win_amd64.whl'
index=urllib.request.urlopen('https://download.pytorch.org/whl/cu126/torch/',timeout=30).read().decode()
match=re.search(re.escape(filename)+r'#sha256=([a-f0-9]{64})',index)
if not match:raise ValueError('Official wheel hash missing')
expected=match[1];size=2915433376;chunk=16*1024*1024
folder=inside('cache/tutorial-torch-download');folder.mkdir(exist_ok=True)
def get(start):
    end=min(size,start+chunk)-1;target=folder/str(start)
    if target.exists() and target.stat().st_size==end-start+1:return target
    url='https://download.pytorch.org/whl/cu126/'+filename+'?hp1_chunk='+str(start)
    with urllib.request.urlopen(urllib.request.Request(url,headers={'Range':f'bytes={start}-{end}'}),timeout=60) as response:
        if response.status!=206 or response.headers['Content-Range']!=f'bytes {start}-{end}/{size}':raise ValueError('Unexpected wheel range')
        data=response.read()
    if len(data)!=end-start+1:raise ValueError('Short range')
    target.write_bytes(data);print(f'{end+1}/{size}',flush=True);return target
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    chunks=list(pool.map(get,range(0,size,chunk)))
dest=inside('cache/torch-2.8.0+cu126-cp313-cp313-win_amd64.whl');digest=hashlib.sha256()
with dest.open('wb') as out:
    for path in chunks:
        data=path.read_bytes();out.write(data);digest.update(data)
if digest.hexdigest()!=expected:raise ValueError('Official wheel SHA-256 differs')
print('verified '+expected,flush=True)
