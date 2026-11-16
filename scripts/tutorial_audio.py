"""Build-only local voice assets and a minimal HP1 v76 sound package.

Only generated PCM audio is packed. The installed input is never modified.
Qwen generation is offline after downloading model weights. No game audio upload.
"""
import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
import uuid
import wave
from pathlib import Path
from project_paths import ROOT, inside
from unreal_package import Package, Reader


def compact(value):
    negative=value<0; value=abs(value)
    first=value&63; value>>=6
    out=bytearray([first|(128 if negative else 0)|(64 if value else 0)])
    while value:
        byte=value&127; value>>=7; out.append(byte|(128 if value else 0))
    return bytes(out)


LANGUAGE = os.environ.get("HP1VR_TUTORIAL_LANGUAGE", "de")
if LANGUAGE not in ("de", "en"): raise ValueError("Unsupported tutorial language")
PACKAGE = "HP1VRTutorial" + ("EN" if LANGUAGE == "en" else "")
FOLDER = "assets/tutorial/" + LANGUAGE

def catalogue():
    return json.loads(inside('config/tutorials-'+LANGUAGE+'.json').read_text(encoding='utf-8'))


def verify_assets():
    config=catalogue();count=0
    for line in config['lines']:
        for variant,labels in config['labels'].items():
            path=inside(FOLDER)/(line['id']+'_'+variant+'.wav')
            record=json.loads(path.with_suffix('.json').read_text(encoding='utf-8'))
            if record['text']!=line['text'].format(**labels) or not record['synthetic']:raise ValueError('Tutorial text/provenance differs')
            if hashlib.sha256(path.read_bytes()).hexdigest()!=record['sha256']:raise ValueError('Tutorial audio differs')
            with wave.open(str(path),'rb') as audio:
                if audio.getnchannels()!=1 or audio.getsampwidth()!=2 or abs(audio.getnframes()/audio.getframerate()-record['seconds'])>.001:raise ValueError('Tutorial PCM/duration differs')
            count+=1
    package=Package(inside('assets/tutorial/'+PACKAGE+'.uax'))
    if len(package.exports)!=count:raise ValueError('Tutorial package incomplete')
    for row in package.exports:
        reader=Reader(package.data,row['offset']);assert package.name(reader.compact())=='None';assert package.name(reader.compact())=='WAV'
        end=reader.unpack('<I')[0];raw=reader.take(reader.compact())
        if reader.pos!=end or end+28!=row['offset']+row['size'] or raw!=(inside(FOLDER)/(row['name']+'.wav')).read_bytes():raise ValueError('Packaged audio differs')
    return count


def prepare_references():
    import imageio_ffmpeg
    config=catalogue(); original=Package(inside('Harry Potter TM/Sounds/AllDialog.ger_uax'))
    texts=dict(line.split('=',1) for line in inside('Harry Potter TM/System/HPDialog.ger').read_text(encoding='cp1252').splitlines() if '=' in line)
    texts={k.lower():v.strip() for k,v in texts.items()}
    folder=inside('cache/tutorial-voices/reference');folder.mkdir(parents=True,exist_ok=True)
    for line in config['lines']:
        key=line['id'];row=next(r for r in original.exports if r['name'].lower()==key.lower())
        reader=Reader(original.data,row['offset'])
        assert original.name(reader.compact())=='None'
        assert original.name(reader.compact()).lower()=='mp2'
        end=reader.unpack('<I')[0];size=reader.compact();raw=reader.take(size)
        assert reader.pos==end==row['offset']+row['size']-28
        mp2=folder/(key+'.mp2');mp2.write_bytes(raw)
        wav=folder/(key+'.wav')
        subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),'-y','-hide_banner','-loglevel','error','-i',str(mp2),'-ar','24000','-ac','1',str(wav)],check=True,creationflags=subprocess.CREATE_NO_WINDOW)
        (folder/(key+'.txt')).write_text(texts[key.lower()],encoding='utf-8')


def generate(limit=0):
    # All runtime caches, including dependencies' temporary files, stay local.
    for key,relative in {'HF_HOME':'cache/huggingface','TORCH_HOME':'cache/torch','NUMBA_CACHE_DIR':'cache/numba','TEMP':'cache/tmp','TMP':'cache/tmp','MPLCONFIGDIR':'cache/matplotlib'}.items():
        path=inside(relative);path.mkdir(parents=True,exist_ok=True);os.environ[key]=str(path)
    os.environ['HF_HUB_DISABLE_TELEMETRY']='1'
    os.environ['PYTHONDONTWRITEBYTECODE']='1'
    import torch
    import soundfile as sf
    import numpy as np
    from qwen_tts import Qwen3TTSModel
    prepare_references()
    config=catalogue(); model_dir=inside('tools/tutorial-voice-model')
    from huggingface_hub import snapshot_download
    snapshot_download(config['voice_model'],local_dir=str(model_dir),max_workers=4)
    torch.set_num_threads(6)
    model=Qwen3TTSModel.from_pretrained(str(model_dir),device_map='cuda:0',dtype=torch.bfloat16,attn_implementation='sdpa')
    out=inside(FOLDER);out.mkdir(parents=True,exist_ok=True)
    produced=0
    for index,line in enumerate(config['lines']):
        reference=inside('cache/tutorial-voices/reference')/line['id']
        pending=[]
        for variant,labels in config['labels'].items():
            text=line['text'].format(**labels);filename=f'{line["id"]}_{variant}.wav';target=out/filename
            meta=target.with_suffix('.json')
            if target.exists() and meta.exists():
                previous=json.loads(meta.read_text(encoding='utf-8'))
                if previous.get('text')==text and hashlib.sha256(target.read_bytes()).hexdigest()==previous.get('sha256'):continue
            pending.append((variant,text,filename,target,meta))
        if not pending:continue
        if limit:pending=pending[:limit-produced]
        prompt=model.create_voice_clone_prompt(ref_audio=str(reference.with_suffix('.wav')),ref_text=reference.with_suffix('.txt').read_text(encoding='utf-8'),x_vector_only_mode=False)
        torch.manual_seed(20260926+index)
        print(json.dumps(dict(generating=line['id'],variants=len(pending))),flush=True)
        with torch.inference_mode():
            wavs,sr=model.generate_voice_clone(text=[p[1] for p in pending],language=['English' if LANGUAGE=='en' else 'German']*len(pending),voice_clone_prompt=prompt,max_new_tokens=1000)
        for (variant,text,filename,target,meta),generated in zip(pending,wavs,strict=True):
            audio=np.asarray(generated,dtype=np.float32)
            if not np.isfinite(audio).all() or not 1<len(audio)/sr<65:raise ValueError('Invalid generated tutorial audio '+filename)
            # Keep safe headroom; PCM is read by the original audio engine.
            reference_audio,_=sf.read(str(reference.with_suffix('.wav')),dtype='float32')
            rms=float(np.sqrt(np.mean(audio*audio)));reference_rms=float(np.sqrt(np.mean(reference_audio*reference_audio)))
            peak=float(np.max(np.abs(audio)));audio*=min(2,reference_rms/max(.001,rms),.89/max(.001,peak))
            sf.write(str(target),audio,sr,subtype='PCM_16')
            record=dict(id=line['id'],variant=variant,text=text,seconds=len(audio)/sr,model=config['voice_model'],synthetic=True,local_reference=True,sha256=hashlib.sha256(target.read_bytes()).hexdigest())
            meta.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            print(json.dumps(record,ensure_ascii=True),flush=True);produced+=1
            if limit and produced>=limit:return


def pack():
    config=catalogue();folder=inside(FOLDER);out=bytearray(64)
    names=['None','WAV','Package','Class','Engine','Core','Sound']
    rows=[]
    for line in config['lines']:
        for variant,labels in config['labels'].items():
            symbol=line['id']+'_'+variant;path=folder/(symbol+'.wav');data=path.read_bytes()
            with wave.open(str(path),'rb') as wav:
                assert wav.getnchannels()==1 and wav.getsampwidth()==2
                seconds=wav.getnframes()/wav.getframerate()
            rows.append(dict(line=line,variant=variant,symbol=symbol,text=line['text'].format(**labels),data=data,seconds=seconds));names.append(symbol)
    for name in names:
        data=name.encode('ascii')+b'\0';out+=compact(len(data))+data+struct.pack('<I',0x70010)
    for index,row in enumerate(rows):
        row['offset']=len(out);data=row.pop('data');prefix=b'\0\1'+b'\0'*4+compact(len(data))
        # HP1 USound v76 serializes seven extra DWORDs after TLazyArray Data.
        # Verified from Engine!USound::Serialize and the original Menu_sfx WAV:
        # flags, streaming, PCM start, PCM byte count, loaded, decoded length, stereo.
        pos=12;pcm_start=pcm_size=None
        while pos+8<=len(data):
            name=data[pos:pos+4];length=struct.unpack_from('<I',data,pos+4)[0]
            if name==b'data':pcm_start=pos+8;pcm_size=length;break
            pos+=8+length+(length&1)
        if pcm_start is None or pcm_start+pcm_size>len(data):raise ValueError('PCM WAV data chunk missing')
        lazy_size=len(prefix)+len(data);row['size']=lazy_size+28;prefix=prefix[:2]+struct.pack('<I',len(out)+lazy_size)+prefix[6:]
        out+=prefix+data+struct.pack('<7I',0,0,pcm_start,pcm_size,1,0,0)
    imports_offset=len(out)
    out+=compact(5)+compact(3)+struct.pack('<i',-2)+compact(6)
    out+=compact(5)+compact(2)+struct.pack('<i',0)+compact(4)
    exports_offset=len(out)
    for index,row in enumerate(rows):
        out+=compact(-1)+compact(0)+struct.pack('<i',0)+compact(7+index)+struct.pack('<I',0xf0004)+compact(row['size'])+compact(row['offset'])
    struct.pack_into('<9I',out,0,0x9e2a83c1,76,1,len(names),64,len(rows),exports_offset,2,imports_offset)
    out[36:52]=uuid.UUID('0a9a9635-3d7c-4e89-bf7c-2ea96ae1c3e0' if LANGUAGE=='en' else '70d8cf65-0c5a-47b8-a026-d2a4a06403db').bytes_le
    struct.pack_into('<3I',out,52,1,len(rows),len(names))
    destination=inside('assets/tutorial/'+PACKAGE+'.uax');destination.write_bytes(out)
    assert len(Package(destination).exports)==len(rows)
    def w(text):return 'L'+json.dumps(text,ensure_ascii=True)
    lines=['// Generated by scripts/tutorial_audio.py --pack. Synthetic VR dialogue.','#pragma once',
           'struct Hp1TutorialLine {const wchar_t* id; const wchar_t* text[2]; const wchar_t* sound[2]; float seconds[2]; const char* actions[2];};',
           'static const Hp1TutorialLine tutorialLines[]={']
    for index,line in enumerate(config['lines']):
        a,b=rows[index*2:index*2+2];actions=line['actions']+['']*(2-len(line['actions']))
        lines.append(' {%s,{%s,%s},{%s,%s},{%.6ff,%.6ff},{%s,%s}},'%(w(line['id']),w(a['text']),w(b['text']),w(PACKAGE+'.'+a['symbol']),w(PACKAGE+'.'+b['symbol']),a['seconds'],b['seconds'],json.dumps(actions[0]),json.dumps(actions[1])))
    if LANGUAGE=='en':
        lines=[line for line in lines if not line.startswith('struct Hp1TutorialLine')]
        lines=[line.replace('tutorialLines[]','tutorialLinesEn[]') for line in lines]
    lines+=['};'];inside('src/hp1_tutorial_catalogue'+('_en' if LANGUAGE=='en' else '')+'.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(json.dumps(dict(package=destination.relative_to(ROOT).as_posix(),sounds=len(rows),bytes=len(out))))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--verify',action='store_true');parser.add_argument('--generate',action='store_true');parser.add_argument('--pack',action='store_true');parser.add_argument('--limit',type=int,default=0);args=parser.parse_args()
    if args.verify:print(verify_assets())
    if args.generate:generate(args.limit)
    if args.pack:pack()
