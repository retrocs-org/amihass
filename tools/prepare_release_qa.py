"""Extract and verify the final release payload, then add isolated QA drivers."""
from pathlib import Path
import argparse,hashlib,shutil,subprocess,sys,zipfile
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('archive',type=Path);p.add_argument('destination',type=Path);a=p.parse_args();dest=a.destination.resolve();assert not dest.exists(),'Use a fresh QA directory'
if a.archive.suffix.lower()=='.lha':
 from release_support import runtime_entries,sdk_entries,verify_lha,verify_zip,sha
 import os
 runtime=runtime_entries(ROOT);verify_lha(a.archive,runtime,os.environ.get('LHA') or shutil.which('lha'),canonical_paths=True)
 assert a.archive.with_name(a.archive.name+'.sha256').read_text()==sha(a.archive)+'  '+a.archive.name+'\n'
 sdk=a.archive.with_name('AmiHASS-0.3-sdk.zip');sdk_members={n:d for n,d,_ in sdk_entries(ROOT)};verify_zip(sdk,sdk_members)
 assert sdk.with_name(sdk.name+'.sha256').read_text()==sha(sdk)+'  '+sdk.name+'\n'
 dest.mkdir(parents=True)
 for name,data in runtime.items():
  if not name.startswith('AmiHASS/'):continue
  target=dest/name[len('AmiHASS/'):];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 with zipfile.ZipFile(sdk) as archive:
  for name in archive.namelist():
   target=dest/'SDK'/name[len('AmiHASS-SDK/'):];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(archive.read(name))
else:
 with zipfile.ZipFile(a.archive) as z:
  names=z.namelist();assert all(not Path(n).is_absolute() and '..' not in Path(n).parts for n in names)
  dest.mkdir(parents=True)
  for name in names:
   parts=Path(name).parts
   if not parts or parts[0]!='AmiHASS-0.3' or len(parts)<2:continue
   target=dest.joinpath(*parts[1:])
   if name.endswith('/'):target.mkdir(parents=True,exist_ok=True);continue
   target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(z.read(name));target.chmod(z.getinfo(name).external_attr>>16&0o777)
 for line in (dest/'MANIFEST.sha256').read_text().splitlines():
  digest,name=line.split('  ',1);assert hashlib.sha256((dest/name).read_bytes()).hexdigest()==digest,name
(dest/'QA-ARCHIVE.sha256').write_text(hashlib.sha256(a.archive.read_bytes()).hexdigest()+'  '+a.archive.name+'\n')
for helper in ('AmiHASSConnectionCheck','AmiHASSInstallerWelcome'):
 shutil.copy2(ROOT/'build/qa'/helper,dest/helper)
subprocess.run([sys.executable,str(ROOT/'tools/prepare_installer_qa.py'),'--source',str(dest/'Install'),'--destination',str(dest)],check=True)
print('Verified release payload prepared for native QA:',dest)
