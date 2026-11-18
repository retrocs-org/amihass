#!/usr/bin/env python3
"""Force a complete native build, then bind code, toolchain and executable hashes."""
import argparse,hashlib,importlib.util,os,shlex,shutil,subprocess
from pathlib import Path
from release_support import ReleaseError,version
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler',default=os.environ.get('AMIHASS_VC','vc +aos68k'));parser.add_argument('--flags',default=os.environ.get('AMIHASS_CFLAGS',os.environ.get('CFLAGS','-O')));args=parser.parse_args()
    command=shlex.split(args.compiler);flags=shlex.split(args.flags)
    if not command or not shutil.which(command[0]):raise ReleaseError('Set AMIHASS_VC to the configured Amiga vbcc command')
    if (ROOT/'local.mk').exists():raise ReleaseError('Use external release env settings instead of local.mk for release builds')
    version(ROOT)
    spec=importlib.util.spec_from_file_location('native_provenance',ROOT/'tools/build-provenance.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    before=mod.code_hash(ROOT);config=hashlib.sha256()
    for item in command[1:]:
        if item.startswith('+') and Path(item[1:]).is_file():config.update(Path(item[1:]).read_bytes())
        else:config.update(item.encode()+b'\0')
    env={k:v for k,v in os.environ.items() if not k.upper().endswith(('_TOKEN','_PASSWORD','_SECRET'))}
    subprocess.run(['make','-B','all','VC='+shlex.join(command),'CFLAGS='+shlex.join(flags)],cwd=ROOT,env=env,check=True)
    if mod.code_hash(ROOT)!=before:raise ReleaseError('Native inputs changed during build')
    result=subprocess.run([command[0],'-version'],env=env,capture_output=True,text=True)
    output=(result.stdout+result.stderr).strip().splitlines()
    mod.record(ROOT,command[0],output[0] if output else 'unreported',shlex.join(flags),config.hexdigest());print('Recorded all seven m68000 native executables.')
if __name__=='__main__':
    try:main()
    except (ReleaseError,subprocess.CalledProcessError) as error:raise SystemExit(str(error))
