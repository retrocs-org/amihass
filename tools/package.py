"""Build a clean ZIP release from explicit application inputs."""
from pathlib import Path
import argparse,hashlib,shutil,stat,subprocess,sys,tempfile,zipfile
ROOT=Path(__file__).resolve().parents[1]
def package(output,web=None):
 values=dict(line.split('=',1) for line in (ROOT/'VERSION').read_text().splitlines() if '=' in line)
 version=values['VERSION'];assert version=='0.2','Version changes require maintainer approval'
 name='AmiHASS-'+version;output=Path(output).resolve();output.mkdir(parents=True,exist_ok=True)
 from release_support import archive_timestamp
 _,stamp=archive_timestamp()
 with tempfile.TemporaryDirectory(prefix='amihass-release-') as tmp:
  staging=Path(tmp);app=staging/name
  subprocess.run([sys.executable,str(ROOT/'tools/stage.py'),str(app)],check=True)
  for doc in ('QUICKSTART.txt','REQUIREMENTS.txt','README.MD','CHANGELOG.MD','LICENSE.MD'):
   shutil.copy2(ROOT/doc,app/doc)
  docs=app/'Docs';docs.mkdir(exist_ok=True)
  for source,dest in [('docs/USAGE.md','USAGE.md'),('docs/SDK.md','SDK.md'),('docs/SELECTED_UPDATES.md','SELECTED_UPDATES.md')]:shutil.copy2(ROOT/source,docs/dest)
  for path in app.rglob('*'):
   if path.is_file() and (path.name.startswith('.') or path.suffix.lower() in ('.o','.asm','.uaem','.pyc','.log')):path.unlink()
  members=sorted(p for p in app.rglob('*') if p.is_file())
  assert members and (staging/(name+'.info')).is_file()
  assert not any(p.name in ('AmiHASS.cfg','Dashboard.cfg','credentials','connection') for p in members)
  manifest=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(app).as_posix()+'\n' for p in members)
  (app/'MANIFEST.sha256').write_text(manifest)
  archive=output/(name+'.zip');fresh=output/(name+'.zip.new')
  with zipfile.ZipFile(fresh,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9,allowZip64=False) as z:
   for p in sorted(staging.rglob('*')):
    member=p.relative_to(staging).as_posix();info=zipfile.ZipInfo(member+('/' if p.is_dir() else ''),stamp);info.create_system=3
    execute=p.is_dir() or p.name in ('AmiHASSPrefs','AmiHASSService','AmiHASSBrowser','AmiHASSDashboard','AmiHASSExample','StartInstaller','SetupStartup')
    info.external_attr=((stat.S_IFDIR if p.is_dir() else stat.S_IFREG)|(0o755 if execute else 0o644))<<16
    if p.is_dir():info.external_attr|=0x10
    info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,b'' if p.is_dir() else p.read_bytes())
  with zipfile.ZipFile(fresh) as z:assert z.testzip() is None
  fresh.replace(archive)
  checksum=output/(name+'.zip.sha256');checksum.write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
  if web:
   web=Path(web).resolve();web.mkdir(parents=True,exist_ok=True);shutil.copy2(archive,web/archive.name);shutil.copy2(checksum,web/checksum.name)
  print('Release:',archive);print('SHA-256:',checksum);return archive
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--output',default=str(ROOT/'build/releases'));parser.add_argument('--web-directory');args=parser.parse_args();package(args.output,args.web_directory)
