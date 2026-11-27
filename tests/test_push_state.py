from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    exe=Path(directory)/'push'
    cmd=[os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'SDK'),'-I'+str(root/'include'),'-I'+str(root/'src/vendor/ami2ha'),str(root/'tests/test_push_state.c'),str(root/'src/push_state.c'),str(root/'src/core.c')]+[str(root/'src/vendor/ami2ha'/f'{name}.c') for name in ('ws','buf','sha1','base64')]+['-o',str(exe)]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
