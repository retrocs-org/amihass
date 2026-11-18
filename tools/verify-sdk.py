#!/usr/bin/env python3
"""Compile the SDK from its actual release ZIP, outside the source tree."""
import argparse,os,shlex,subprocess,tempfile,zipfile
from pathlib import Path
from release_support import ReleaseError,sdk_entries,verify_zip
ROOT=Path(__file__).resolve().parents[1]
def verify(archive):
    expected={name:data for name,data,_ in sdk_entries(ROOT)}
    verify_zip(archive,expected)
    compiler=shlex.split(os.environ.get('AMIHASS_VC','vc +aos68k'))
    flags=shlex.split(os.environ.get('AMIHASS_CFLAGS',os.environ.get('CFLAGS','-O')))
    with tempfile.TemporaryDirectory(prefix='amihass-sdk-') as directory:
        base=Path(directory)
        with zipfile.ZipFile(archive) as source:
            for name in expected:
                target=base/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(source.read(name))
        kit=base/'AmiHASS-SDK'
        subprocess.run(['make','-B','VC='+shlex.join(compiler),'CFLAGS='+shlex.join(flags)],cwd=kit,check=True)
        data=(kit/'AmiHASSExample').read_bytes()
        if data[:4]!=b'\0\0\3\xf3' or b'$VER: AmiHASSExample 0.1' not in data:raise ReleaseError('SDK example is not a native 0.1 executable')
    print('Standalone release SDK compiled successfully.')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('archive',type=Path);args=parser.parse_args();verify(args.archive)
