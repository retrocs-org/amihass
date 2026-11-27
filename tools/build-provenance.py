#!/usr/bin/env python3
"""Verify the native build against the exact code and native executable hashes."""
import argparse,hashlib,json
from pathlib import Path
from inventory import BINARIES
from release_support import ReleaseError,atomic_json,version,sha
ROOT=Path(__file__).resolve().parents[1]

def code_hash(root):
    files=[root/'Makefile',root/'VERSION',root/'tools/build-amiga.py',root/'tools/build-provenance.py']
    for directory in ('src','include','SDK','tools'):
        files.extend(p for p in (root/directory).rglob('*') if p.suffix in ('.c','.h') and p.is_file())
    h=hashlib.sha256()
    for p in sorted(set(files)):
        if p.is_symlink():raise ReleaseError('Linked native source')
        h.update(p.relative_to(root).as_posix().encode()+b'\0'+sha(p).encode()+b'\n')
    return h.hexdigest()

def verify(root=ROOT):
    path=root/'BUILD-PROVENANCE.json'
    if not path.is_file() or path.is_symlink():raise ReleaseError('Rebuild using tools/build-amiga.py; no native provenance')
    record=json.loads(path.read_text())
    if record.get('version')!=version(root) or record.get('code_sha256')!=code_hash(root):raise ReleaseError('Native code changed; rebuild before packaging')
    if set(record.get('binaries',{}))!=set(BINARIES):raise ReleaseError('Incomplete native executable inventory')
    for name,digest in record['binaries'].items():
        p=root/name
        if p.is_symlink() or not p.is_file() or sha(p)!=digest or p.read_bytes()[:4]!=b'\0\0\3\xf3':raise ReleaseError('Native binary changed: '+name)
        if ('$VER: '+name+' '+record['version']).encode() not in p.read_bytes():raise ReleaseError('Wrong native version: '+name)
    return record

def record(root,compiler,compiler_version,flags,compiler_config_sha256):
    data={'schema':1,'version':version(root),'target':'m68000','compiler':Path(compiler).name,'compiler_version':compiler_version,'flags':flags,'compiler_config_sha256':compiler_config_sha256,'code_sha256':code_hash(root),'binaries':{n:sha(root/n) for n in BINARIES}}
    atomic_json(root/'BUILD-PROVENANCE.json',data);verify(root);return data

if __name__=='__main__':
    argparse.ArgumentParser(description=__doc__).parse_args();verify();print('Native code and executable hashes verified.')
