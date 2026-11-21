"""Suppress user questions for isolated native QA; retain real Installer actions."""
from pathlib import Path
import sys,argparse
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from installer_parser import parse,tokenize,head
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=ROOT/'Install');parser.add_argument('--destination',type=Path,default=ROOT/'build/qa');args=parser.parse_args();args.destination.mkdir(parents=True,exist_ok=True)
nodes=parse(tokenize(args.source.read_text()));qa_level=1
def emit(n,action,remove):
 if isinstance(n,tuple):return '"'+n[2]+'"' if n[0]=='str' else n[2]
 h=head(n);v=n['items'][1:]
 if h in ('welcome','message'):return '(set #qa-message 0)'
 if h=='set' and v[0][2]=='#level':return '(set #level '+str(qa_level)+')'
 if h=='user':return '(user 0)'
 if h=='if' and 'set #src' in raw(n):return '(set #src "QATests:")'
 if h=='askchoice':
  prompt=next(x for x in v if isinstance(x,dict) and head(x)=='prompt')['items'][1][2]
  return str(remove if prompt.startswith('Keep or remove') else action)
 if h=='askbool':
  prompt=next(x for x in v if isinstance(x,dict) and head(x)=='prompt')['items'][1][2]
  if prompt.startswith('Remove AmiHASS'):return '1'
  if prompt.startswith('Remove saved'):return str(remove)
 if h in ('askdir','askbool'):
  return emit(next(x for x in v if isinstance(x,dict) and head(x)=='default')['items'][1],action,remove)
 parts=[emit(x,action,remove) for x in n['items']]
 line='('+' '.join(parts)+')'
 return line if len(line)<160 and '\n' not in line else '('+'\n'.join(parts)+')'
def raw(n):
 if isinstance(n,tuple):return n[2]
 return ' '.join(raw(x) for x in n['items'])
for level in (None,0,1,2):
 qa_level=1 if level is None else level
 for name,action,remove in [('Update',0,0),('Keep',1,0),('Remove',1,1)]:
  suffix='' if level is None else '-'+str(level)
  prefix='(user 0)\n(onerror (textfile (dest "AutoRun:installer-error.txt") (append @error-msg)) (exit (quiet)))\n'
  (args.destination/('Install-QA-'+name+suffix)).write_text(prefix+'\n'.join(emit(n,action,remove) for n in nodes)+'\n')
