from pathlib import Path
import hashlib,subprocess,sys,tempfile,zipfile,time,datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from release_support import version
VERSION=version(ROOT)
PREFIX='AmiHASS-'+VERSION
with tempfile.TemporaryDirectory() as tmp:
 out=Path(tmp)
 subprocess.run([sys.executable,str(ROOT/'tools/package.py'),'--output',str(out)],check=True,stdout=subprocess.PIPE)
 archive=out/(PREFIX+'.zip')
 with zipfile.ZipFile(archive) as z:
  names=z.namelist();assert z.testzip() is None
  for name in (PREFIX+'.info',PREFIX+'/QUICKSTART.txt',PREFIX+'/REQUIREMENTS.txt',PREFIX+'/SDK/AmiHASSExample',PREFIX+'/SDK/example.c',PREFIX+'/ARexx/SampleLight.rexx',PREFIX+'/MANIFEST.sha256'):assert name in names,name
  assert 'AmiHASS-0.info' not in names
  assert not any(n.endswith(('.o','.asm','.uaem','.log')) or '/qa/' in n or '/profile/' in n or n.endswith(('AmiHASS.cfg','Dashboard.cfg','credentials')) for n in names)
  for line in z.read(PREFIX+'/MANIFEST.sha256').decode().splitlines():
   digest,name=line.split('  ',1);assert hashlib.sha256(z.read(PREFIX+'/'+name)).hexdigest()==digest
  for name in ('AmiHASSPrefs','AmiHASSService','AmiHASSBrowser','AmiHASSDashboard','SDK/AmiHASSExample'):
   member=PREFIX+'/'+name;assert z.read(member)[:4]==b'\0\0\x03\xf3';assert z.getinfo(member).external_attr>>16&0o111
 with zipfile.ZipFile(archive) as z:
  first={n:z.read(n) for n in z.namelist()}
  now=datetime.datetime.now()
  for entry in z.infolist():
   timestamp=datetime.datetime(*entry.date_time)
   assert abs((now-timestamp).total_seconds())<120,entry.filename
 subprocess.run([sys.executable,str(ROOT/'tools/package.py'),'--output',str(out)],check=True,stdout=subprocess.PIPE)
 with zipfile.ZipFile(archive) as z:assert {n:z.read(n) for n in z.namelist()}==first
 assert (out/(PREFIX+'.zip.sha256')).read_text().startswith(hashlib.sha256(archive.read_bytes()).hexdigest())
 print('Release: clean paths, correct drawer icon, guides, SDK/ARexx examples, manifest, executable files current dates and consistent ZIP payloads passed.')
