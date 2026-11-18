#!/usr/bin/env python3
"""Package portable source, native binaries and public-web together for handoff."""
import argparse,importlib.util,os,tempfile
from pathlib import Path
from release_support import binaries,source_files,version,zip_write,verify_zip,sha
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=ROOT/'outputs/project');args=parser.parse_args()
    for name in ('build-provenance','check-sensitive'):
        spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/(name+'.py'));mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        mod.verify() if name=='build-provenance' else mod.scan(ROOT)
    files=source_files(ROOT);entries=[('AmiHASS/'+name,(ROOT/name).read_bytes(),meta['executable']) for name,meta in files.items()]
    entries += [('AmiHASS/'+name,(ROOT/name).read_bytes(),True) for name in sorted(binaries(ROOT))]
    entries.append(('AmiHASS/BUILD-PROVENANCE.json',(ROOT/'BUILD-PROVENANCE.json').read_bytes(),False))
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True);path=output/('AmiHASS-'+version(ROOT)+'-project.zip')
    fd,temp=tempfile.mkstemp(prefix='.amihass-project-',suffix='.zip',dir=output);os.close(fd)
    try:
        zip_write(Path(temp),entries);verify_zip(Path(temp),{n:d for n,d,_ in entries});os.replace(temp,path)
    finally:
        if os.path.exists(temp):os.unlink(temp)
    path.with_name(path.name+'.sha256').write_text(sha(path)+'  '+path.name+'\n');print('Complete source and public-web project:',path)
if __name__=='__main__':main()
