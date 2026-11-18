#!/usr/bin/env python3
"""Capture genuine app windows in an isolated sample-data Workbench guest."""
import argparse,datetime,os,shlex,subprocess,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
APP=Path('/Applications/Amiga Workbench.app/Contents/MacOS/AmigaWorkbench')
NAMES=('preferences','device-browser','room-selector','dashboard','about')
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--compiler',default=os.environ.get('AMIHASS_VC','vc +aos68k'))
 args=parser.parse_args()
 state=subprocess.run(['ps','-axww','-o','comm='],capture_output=True,text=True,check=True)
 if any(Path(line.strip()).name.lower()=='fs-uae' for line in state.stdout.splitlines()):raise SystemExit('An emulator session is running; leave it untouched and close it before capture.')
 subprocess.run(['make','qa','VC='+args.compiler],cwd=ROOT,check=True)
 core=next(line.split('=',1)[1].split() for line in (ROOT/'Makefile').read_text().splitlines() if line.startswith('CORE ='))
 subprocess.run(shlex.split(args.compiler)+['-O','-ISDK','-Iinclude','-o','build/qa/AmiHASSWebsiteCapture','tests/native_website_capture.c','src/dashboard_layout.c']+core+['-lamiga'],cwd=ROOT,check=True)
 result=ROOT/'outputs/website-captures'/('capture-'+datetime.datetime.now().strftime('%Y%m%d')+'-'+uuid.uuid4().hex[:8])
 subprocess.run([str(APP),'--workbench','workbench-3.2.1','--mount','QATests:'+str(ROOT/'build/qa'),'--test',str(ROOT/'qa/website-screenshots.json'),'--output',str(result)],cwd=ROOT,check=True)
 sources=list((result/'profile').glob('Installations/*/CPUs/*/AutoRun/screenshots'))
 if len(sources)!=1:raise SystemExit('Missing guest screenshot output; inspect '+str(result))
 target=ROOT/'public-web/assets/screenshots';target.mkdir(parents=True,exist_ok=True)
 for name in NAMES:
  source=sources[0]/(name+'.ppm')
  if not source.is_file():raise SystemExit('Missing screenshot '+name)
  subprocess.run(['/usr/bin/sips','-s','format','png',str(source),'--out',str(target/(name+'.png'))],check=True)
 print('Original guest PPM captures and QA reports:',result)
 print('Lossless website PNG files:',target)
if __name__=='__main__':main()
