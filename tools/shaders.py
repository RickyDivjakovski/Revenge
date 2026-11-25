#!/usr/bin/env python3
"""Read-only DXBC inventory and parallel AOT compilation of supplied PC HLSL.
Does not decode Xbox GPU ISA or convert console containers to executable DXIL.
"""
import argparse, concurrent.futures, hashlib, json, mmap, os, pathlib, re, struct, subprocess, tempfile
P=pathlib.Path

def digest(data): return hashlib.sha256(data).hexdigest()
def container(buf, pos):
    if pos+32>len(buf): return None
    _, length, count=struct.unpack_from('<III',buf,pos+20)
    if not 0<count<=1024 or not 32+4*count<=length<=min(len(buf)-pos,128*1024*1024): return None
    chunks=[]; spans=[]
    for i in range(count):
        off=struct.unpack_from('<I',buf,pos+32+4*i)[0]
        if off<32+4*count or off+8>length: return None
        tag,n=struct.unpack_from('<4sI',buf,pos+off)
        if any(c<32 or c>126 for c in tag) or off+8+n>length: return None
        spans.append((off,off+8+n)); chunks.append(tag.decode('ascii'))
    spans.sort()
    if any(a[1]>b[0] for a,b in zip(spans,spans[1:])): return None
    return length,chunks

def scan(root):
    files=[root] if root.is_file() else sorted(root.rglob('*'))
    seen={}; errors=[]
    for p in files:
        try:
            if p.is_symlink() or not p.is_file() or p.stat().st_size<32: continue
            with p.open('rb') as f, mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
                at=0
                while (at:=m.find(b'DXBC',at))!=-1:
                    v=container(m,at)
                    if not v: at+=4; continue
                    length,chunks=v; key=digest(m[at:at+length]); entry=seen.setdefault(key,{'sha256':key,'bytes':length,'chunks':chunks,'locations':[]}); entry['locations'].append({'file':str(p),'offset':at}); at+=length
        except (OSError,ValueError) as e: errors.append({'file':str(p),'error':str(e)})
    return {'unique_containers':len(seen),'containers':list(seen.values()),'errors':errors,'note':'Structural inventory only; neither authenticity nor PC compatibility validated.'}

def run(args):
    result=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180,check=False)
    if result.returncode: raise RuntimeError(result.stdout.decode(errors='replace')[-4000:])

def compile_all(manifest, dxc, cache, jobs):
    manifest=manifest.resolve(); root=manifest.parent; data=json.loads(manifest.read_text(encoding='utf-8')); cache.mkdir(parents=True,exist_ok=True)
    exe=P(dxc).resolve(); compiler=digest(exe.read_bytes()); compiler_id=hashlib.sha256(compiler.encode());
    for name in ('dxcompiler.dll','dxil.dll','libdxcompiler.so','libdxil.so'):
        sibling=exe.parent/name
        if sibling.is_file(): compiler_id.update(sibling.read_bytes())
    identity=compiler_id.hexdigest()
    def one(item):
        try:
            src=(root/item['file']).resolve(); entry=item['entry']; target=item['target'];
            if not src.is_relative_to(root) or not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*',entry) or not re.fullmatch(r'(vs|ps|cs|gs|hs|ds|ms|as)_6_[0-9]',target): raise ValueError('invalid source, entry, or target')
            with tempfile.TemporaryDirectory(dir=cache) as td:
                pp=P(td)/'pre.hlsl'; out=P(td)/'shader.dxil'; common=['-E',entry,'-T',target]
                run([str(exe),str(src),*common,'-I',str(src.parent),'-P',str(pp)])
                key=digest(json.dumps(['revenge-hlsl-v1',identity,entry,target,'-O3',digest(pp.read_bytes())]).encode()); final=cache/(key+'.dxil'); meta=cache/(key+'.sha256')
                if final.is_file() and meta.is_file() and digest(final.read_bytes())==meta.read_text().strip(): return {'file':str(src),'state':'hit','key':key}
                run([str(exe),str(pp),*common,'-O3','-Fo',str(out)])
                raw=out.read_bytes(); info=container(raw,0)
                if not info or info[0]!=len(raw) or 'DXIL' not in info[1]: raise ValueError('compiler output is not a structurally valid DXIL container')
                run([str(exe),'-dumpbin',str(out)])
                hashfile=P(td)/'hash'; hashfile.write_text(digest(raw)); os.replace(out,final); os.replace(hashfile,meta)
                return {'file':str(src),'state':'compiled','key':key}
        except Exception as e: return {'file':str(item),'state':'error','error':str(e)}
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool: return list(pool.map(one,data['shaders']))

def main():
    a=argparse.ArgumentParser(description=__doc__); sub=a.add_subparsers(dest='cmd',required=True); s=sub.add_parser('scan');s.add_argument('path',type=P);s.add_argument('--out',type=P,required=True); c=sub.add_parser('compile');c.add_argument('manifest',type=P);c.add_argument('--dxc',required=True);c.add_argument('--cache',type=P,default=P('cache/hlsl'));c.add_argument('--jobs',type=int,default=min(6,os.cpu_count() or 1));args=a.parse_args()
    if args.cmd=='scan':
        if not args.path.exists():a.error('scan input does not exist')
        report=scan(args.path);args.out.write_text(json.dumps(report,indent=2));print(f"{report['unique_containers']} unique containers; {len(report['errors'])} read errors");return bool(report['errors'])
    if not 1<=args.jobs<=32:a.error('jobs must be 1..32')
    report=compile_all(args.manifest,args.dxc,args.cache,args.jobs);print(json.dumps(report,indent=2));return any(x['state']=='error' for x in report)
if __name__=='__main__': raise SystemExit(main())
