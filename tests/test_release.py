from pathlib import Path
import hashlib,subprocess,sys,tempfile,zipfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 out=Path(tmp)
 subprocess.run([sys.executable,str(ROOT/'tools/package.py'),'--output',str(out)],check=True,stdout=subprocess.PIPE)
 archive=out/'AmiHASS-0.1.zip'
 with zipfile.ZipFile(archive) as z:
  names=z.namelist();assert z.testzip() is None
  for name in ('AmiHASS-0.1.info','AmiHASS-0.1/QUICKSTART.txt','AmiHASS-0.1/REQUIREMENTS.txt','AmiHASS-0.1/SDK/AmiHASSExample','AmiHASS-0.1/SDK/example.c','AmiHASS-0.1/ARexx/SampleLight.rexx','AmiHASS-0.1/MANIFEST.sha256'):assert name in names,name
  assert 'AmiHASS-0.info' not in names
  assert not any(n.endswith(('.o','.asm','.uaem','.log')) or '/qa/' in n or '/profile/' in n or n.endswith(('AmiHASS.cfg','Dashboard.cfg','credentials')) for n in names)
  for line in z.read('AmiHASS-0.1/MANIFEST.sha256').decode().splitlines():
   digest,name=line.split('  ',1);assert hashlib.sha256(z.read('AmiHASS-0.1/'+name)).hexdigest()==digest
  for name in ('AmiHASSPrefs','AmiHASSService','AmiHASSBrowser','AmiHASSDashboard','SDK/AmiHASSExample'):
   member='AmiHASS-0.1/'+name;assert z.read(member)[:4]==b'\0\0\x03\xf3';assert z.getinfo(member).external_attr>>16&0o111
 first=archive.read_bytes();subprocess.run([sys.executable,str(ROOT/'tools/package.py'),'--output',str(out)],check=True,stdout=subprocess.PIPE);assert archive.read_bytes()==first
 assert (out/'AmiHASS-0.1.zip.sha256').read_text().startswith(hashlib.sha256(first).hexdigest())
 print('Release: clean paths, correct drawer icon, guides, SDK/ARexx examples, manifest, executable files and reproducible ZIP passed.')
