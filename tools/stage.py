"""Stage only owned runtime/SDK files; retain drawer inodes and unrelated files."""
from pathlib import Path
import argparse,importlib.util,shutil
from inventory import runtime_inventory,sdk_inventory,BINARIES
ROOT=Path(__file__).resolve().parents[1]

def stage(destination):
    dest=Path(destination).absolute()
    spec=importlib.util.spec_from_file_location('stage_builds',ROOT/'tools/stage-builds.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    inventory=runtime_inventory(ROOT)
    inventory.update({'SDK/'+name:source for name,source in sdk_inventory(ROOT).items()})
    for name in BINARIES:
        if (ROOT/name).read_bytes()[:4]!=b'\0\0\3\xf3':raise ValueError('Missing native HUNK: '+name)
    # Validate every destination before changing any owned file.
    for name in inventory:
        path=dest/name
        for q in (path,*path.parents):
            if q.is_symlink() and q not in (Path('/tmp'),Path('/var'),Path('/etc')):raise ValueError('Linked staging path: '+name)
        if path.exists() and not path.is_file():raise ValueError('Staging drawer conflicts with file: '+name)
    dest=dest.resolve()
    outer=Path(str(dest)+'.info')
    if outer.is_symlink() or (outer.exists() and not outer.is_file()):raise ValueError('Invalid drawer icon destination')
    dest.mkdir(parents=True,exist_ok=True)
    import tempfile
    with tempfile.TemporaryDirectory() as temp:
        source=Path(temp)
        for name,original in inventory.items():
            path=source/name;path.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/original,path)
        mod.copy_files(source,dest,sorted(inventory))
    mod.remove_legacy_installer(dest)
    mod.remove_retired_apps(dest)
    outer=Path(str(dest)+'.info')
    if outer.is_symlink():raise ValueError('Linked drawer icon')
    if not outer.exists():
        # The shared copy routine gives the outer icon the same atomic update semantics.
        with tempfile.TemporaryDirectory() as temp:
            source=Path(temp);shutil.copy2(ROOT/'AmiHASS.drawer.info',source/outer.name);mod.copy_files(source,dest.parent,[outer.name])
    print('Staged AmiHASS in',dest)
    return dest
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('destination',nargs='?',default='build/AmiHASS');args=parser.parse_args();stage(args.destination)
