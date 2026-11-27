from pathlib import Path
import re,shlex
from installer_parser import tokenize,parse,walk,head,FUNCTIONS,PARAMETERS
ROOT=Path(__file__).resolve().parents[1]
tree=parse(tokenize((ROOT/'Install').read_text()))
procedures={n['items'][1][2].lower():n['items'][2:] for n in tree if head(n)=='procedure'}
for n in walk(tree):
 h=head(n)
 assert h is None or h in FUNCTIONS or h in PARAMETERS or h in procedures,(h,n['line'])
 if h=='complete':
  value=n['items'][1];assert isinstance(value,tuple) and value[0]!='str' and value[2].isdigit(),'COMPLETE takes a numeric percentage, not a message'
 if h in ('askbool','askchoice','askdir'):
  tags={head(x) for x in n['items'][1:] if isinstance(x,dict)}
  assert {'prompt','help','default'}<=tags,(h,n['line'])
  if h!='askdir':assert 'choices' in tags
def validate_copy_options(nodes):
 for node in walk(nodes):
  if head(node)!='copyfiles':continue
  tags={head(x):x for x in node['items'][1:] if isinstance(x,dict)}
  assert {'source','dest','prompt','help'}<=set(tags),('Missing copy option',node['line'])
  for flag in ('files','infos','all','safe','nogauge'):
   if flag in tags:assert len(tags[flag]['items'])==1,('Copy flag takes no filenames',flag,node['line'])
  for parameter in ('source','dest'):
   assert len(tags[parameter]['items'])==2,('Copy path must have one value',parameter,node['line'])
 # Native Installer also requires choices, all or pattern for a drawer source.
 # This package deliberately copies one explicit filename at a time.
validate_copy_options(tree)
try:
 validate_copy_options(parse(tokenize('(copyfiles (source "drawer") (dest "target") (files "AmiHASSPrefs") (prompt "Copy") (help "Help"))')))
except AssertionError:pass
else:raise AssertionError('Old broken COPYFILES form was not rejected')
class End(Exception):pass
class Scenario:
 def __init__(self,installed=False,action=0,remove=0,level=0,running=False,stop_fail=False,restart_fail=False,partial=False,open_prefs=False):
  self.open_prefs=open_prefs
  self.env={'@icon':'Bin:Package/Install','@user-level':level,'@copyfiles-help':'Help'}
  self.files={};self.stopped=False;self.action=action;self.remove=remove;self.running=running;self.stop_fail=stop_fail;self.restart_fail=restart_fail;self.messages=[];self.message_levels=[];self.opened=False;self.restarted=False;self.copied_files=[];self.questions=[];self.result=''
  self.paths={'Service':'WORK:Apps/AmiHASS' if installed else 'SYS:AmiHASS','Prefs':'WORK:Prefs' if installed else 'SYS:Prefs','Startup':'WORK:Startup' if installed else 'SYS:WBStartup','Boot':'1'}
  if installed:
   d=self.paths['Service'];p=self.paths['Prefs'];b=self.paths['Startup']
   self.files.update({'LOCALE:Catalogs/svenska/amihass.catalog':'catalog','SYS:Utilities/AmiHASSDock':'dock','SYS:Utilities/AmiHASSDock.info':'icon',b+'/AmiHASSDock':'dock',b+'/AmiHASSDock.info':'icon','SYS:Utilities/AmiHASSDashboard':'dashboard','SYS:Utilities/AmiHASSDashboard.info':'icon','SYS:Utilities/AmiHASSBrowser':'browser','SYS:Utilities/AmiHASSBrowser.info':'icon',d+'/AmiHASSBrowser':'browser',d+'/AmiHASSBrowser.info':'browser icon',d+'/AmiHASS':'legacy',d+'/unrelated':'keep',p+'/AmiHASSPrefs':'old prefs',p+'/AmiHASSPrefs.info':'old icon',b+'/AmiHASS':'startup',b+'/AmiHASS.info':'icon','ENVARC:AmiHASS/CameraRefresh.cfg':'camera policy','ENVARC:AmiHASS/Favourites.cfg':'favourites','ENVARC:AmiHASS/AmiHASS.cfg':'all settings','ENVARC:AmiHASS/AmiHASS.cfg.old':'recovery settings','ENVARC:AmiHASS/Private/Save-recovery/old':'token','ENV:AmiHASS':'live','SYS:Utilities/NovaDock':'preserve NovaDock','WORK:Startup/NovaDock':'preserve NovaDock'})
   if not partial:self.files[d+'/AmiHASSService']='old service'
 def has(self,p):return 1 if p in self.files else (2 if any(x.startswith(p.rstrip('/')+'/') for x in self.files) else 0)
 def eval(self,n):
  if isinstance(n,tuple):
   if n[0]=='str':return n[2].replace('\\"','"').replace('\\n','\n')
   if n[2] in self.env:return self.env[n[2]]
   try:return int(n[2])
   except ValueError:return ''
  h=head(n);v=n['items'][1:]
  if h is None:
   out=0
   for x in n['items']:out=self.eval(x)
   return out
  if h=='procedure':return 0
  if h in procedures:
   for x in procedures[h]:self.eval(x)
   return 0
  if h=='set':self.env[v[0][2]]=self.eval(v[1]);return self.env[v[0][2]]
  if h=='if':
   condition=self.eval(v[0]);return self.eval(v[1]) if condition else (self.eval(v[2]) if len(v)>2 else 0)
  if h in ('=','<>'):
   same=self.eval(v[0])==self.eval(v[1]);return int(same if h=='=' else not same)
  if h=='user':self.env['@user-level']=self.eval(v[0]);return 0
  if h=='and':return int(all(self.eval(x) for x in v))
  if h=='or':return int(any(self.eval(x) for x in v))
  if h=='cat':return ''.join(str(self.eval(x)) for x in v)
  if h=='tackon':
   parent=self.eval(v[0]).rstrip('/');return parent+('' if parent.endswith(':') else '/')+self.eval(v[1])
  if h=='pathonly':
   value=self.eval(v[0]);return value.rsplit('/',1)[0] if '/' in value else value.split(':',1)[0]+':'
  if h=='fileonly':return self.eval(v[0]).rsplit('/',1)[-1].rsplit(':',1)[-1]
  if h=='expandpath':return self.eval(v[0]).rstrip('/')
  if h=='exists':return self.has(self.eval(v[0]))
  if h=='getenv':return self.paths[self.eval(v[0]).split('Installer')[1]]
  if h in ('welcome','working','makedir'):return 0
  if h=='complete':assert isinstance(self.eval(v[0]),int);return 0
  if h=='message':self.message_levels.append(self.env['@user-level']);self.messages.append(''.join(str(self.eval(x)) for x in v));return 0
  if h in ('exit','abort'):raise End(h)
  tags={head(x):x['items'][1:] for x in v if isinstance(x,dict)}
  if h in ('askchoice','askbool','askdir'):
   prompt=self.eval(tags['prompt'][0]);self.questions.append(prompt)
   if self.env['@user-level']==0:return self.eval(tags['default'][0])
   if prompt=='Open AmiHASS Preferences now?':return int(self.open_prefs)
   if h=='askchoice':return self.remove if prompt.startswith('Keep or remove') else self.action
   if h=='askdir':return {'Choose a parent drawer for shared data':'DATA:Shared','Choose the Preferences drawer':'DATA:Preferences','Choose the Workbench startup drawer':'DATA:Boot'}[prompt]
   if 'Remove AmiHASS' in prompt:return 1
   if 'Remove saved' in prompt:return self.remove
   return 1
  if h=='run':
   args=shlex.split(self.eval(v[0]));mode=args[1]
   if mode=='EXPORT':return 0
   if mode=='CLEAN':return 0
   if mode=='STATE':return 0 if self.running else 5
   if mode=='STOP':
    if self.stop_fail:return 20
    self.stopped=True;self.running=False;return 0
   assert self.stopped,args
   if mode=='PURGE':self.files={k:v for k,v in self.files.items() if not k.startswith('ENVARC:AmiHASS/')};return 0
   if mode=='OPEN':self.opened=True;assert args[2]==self.env['#prefs']+'/AmiHASSPrefs';return 0
   if mode=='RESTART':
    if self.restart_fail:return 20
    self.running=self.restarted=True;return 0
   assert mode.endswith('/AmiHASSService'),args
   self.files['ENVARC:AmiHASS/AmiHASS.cfg']='all settings'
   old=self.paths['Startup'];self.files.pop(old+'/AmiHASS',None);self.files.pop(old+'/AmiHASS.info',None)
   if args[2]=='ON':self.files[args[4]+'/AmiHASS']='startup';self.files[args[4]+'/AmiHASS.info']='icon'
   return 0
  if h=='copyfiles':
   assert self.stopped
   dest=self.eval(tags['dest'][0]);source=self.eval(tags['source'][0]);file=source.removeprefix('Bin:Package/')
   assert ('/' not in file or (file.startswith('Catalogs/') and file.endswith('/amihass.catalog'))) and (file in ('AmiHASSPrefs','AmiHASSService','AmiHASSFetch','AmiHASSBrowser','AmiHASSDashboard','StartInstaller','SetupStartup') or file=='AmiHASS.guide' or (ROOT/file).is_file()),source
   self.files[dest+'/'+file.rsplit('/',1)[-1]]='installed';self.copied_files.append(file);return 0
  if h=='delete':
   assert self.stopped
   path=self.eval(v[0]);assert path!='ENV:AmiHASS';self.files.pop(path,None);return 0
  raise AssertionError(h)
 def run(self):
  try:
   for n in tree:self.eval(n)
  except End as e:self.result=str(e)
  return self
for level in range(3):
 fresh=Scenario(level=level).run();assert fresh.result=='exit' and not fresh.opened
 assert fresh.files[fresh.env['#prefs']+'/AmiHASSPrefs.info']=='installed'
 assert fresh.files[fresh.env['#dest']+'/AmiHASSService']=='installed'
 assert fresh.files['SYS:Utilities/AmiHASSDashboard']=='installed' and fresh.files['SYS:Utilities/AmiHASSDashboard.info']=='installed' and not fresh.has('SYS:Utilities/AmiHASSDock') and not fresh.has('SYS:Utilities/AmiHASSDock.info') and fresh.files['LOCALE:Catalogs/svenska/amihass.catalog']=='installed' and fresh.files['LOCALE:Catalogs/deutsch/amihass.catalog']=='installed'
 assert fresh.files['SYS:Utilities/AmiHASSBrowser']=='installed' and fresh.files['SYS:Utilities/AmiHASSBrowser.info']=='installed'
 assert fresh.env['#startup']==1 and 'configure Server Settings and start' in fresh.messages[-1]
 assert fresh.message_levels[-1]>=1 and 'Open Preferences manually' in fresh.messages[-1]
 assert fresh.env['#dest'] in fresh.messages[-1] and 'SYS:Utilities/' in fresh.messages[-1] and 'ENVARC:AmiHASS/' in fresh.messages[-1]
 assert 'AmiHASSBrowser' in fresh.messages[-1] and 'AmiHASSDashboard' in fresh.messages[-1]
 assert fresh.env['#prefs']+'/AmiHASSPrefs' in fresh.messages[-1]
 for live in (False,True):
  s=Scenario(installed=True,level=level,running=live).run();assert s.result=='exit' and s.restarted==live and not s.opened
  assert ('was restarted' if live else 'left stopped') in s.messages[-1]
  assert s.message_levels[-1]>=1 and s.env['#dest'] in s.messages[-1] and s.env['#prefs']+'/AmiHASSPrefs' in s.messages[-1]
  assert 'Open Preferences manually' in s.messages[-1] and 'AmiHASSDashboard' in s.messages[-1]
  assert s.files['SYS:Utilities/AmiHASSDashboard']=='installed' and s.files['SYS:Utilities/AmiHASSDashboard.info']=='installed'
  assert s.files['SYS:Utilities/AmiHASSBrowser']=='installed' and s.files['SYS:Utilities/AmiHASSBrowser.info']=='installed'
  assert not s.has('WORK:Apps/AmiHASS/AmiHASSBrowser')
  assert 'Choose update or uninstall' in s.questions[0] and s.files['WORK:Apps/AmiHASS/unrelated']=='keep'
  assert s.files[s.env['#prefs']+'/AmiHASSPrefs.info']=='installed' and 'ENVARC:AmiHASS/AmiHASS.cfg' in s.files
  assert s.files['SYS:Utilities/NovaDock']=='preserve NovaDock' and s.files['WORK:Startup/NovaDock']=='preserve NovaDock'
  assert not s.has('SYS:Utilities/AmiHASSDock') and not s.has('WORK:Startup/AmiHASSDock')
  if level:assert not s.has('WORK:Prefs/AmiHASSPrefs') and not s.has('WORK:Startup/AmiHASS')
  if level==2:assert not s.has('WORK:Apps/AmiHASS/AmiHASSService')
 for remove in (False,True):
  s=Scenario(installed=True,level=level,action=1,remove=remove,running=True).run()
  assert s.result=='exit' and s.stopped and not s.running and not s.has('WORK:Prefs/AmiHASSPrefs') and not s.has('WORK:Apps/AmiHASS/AmiHASSService')
  assert s.files['WORK:Apps/AmiHASS/unrelated']=='keep' and s.files['ENV:AmiHASS']=='live'
  assert not s.has('WORK:Apps/AmiHASS/AmiHASSBrowser') and not s.has('WORK:Apps/AmiHASS/AmiHASSBrowser.info')
  assert not s.has('SYS:Utilities/AmiHASSDashboard') and not s.has('SYS:Utilities/AmiHASSDashboard.info')
  assert s.files['SYS:Utilities/NovaDock']=='preserve NovaDock' and s.files['WORK:Startup/NovaDock']=='preserve NovaDock'
  assert not s.has('SYS:Utilities/AmiHASSDock') and not s.has('WORK:Startup/AmiHASSDock') and not s.has('LOCALE:Catalogs/svenska/amihass.catalog')
  assert not s.has('SYS:Utilities/AmiHASSBrowser') and not s.has('SYS:Utilities/AmiHASSBrowser.info')
  assert len(s.questions)==2 and s.questions[1].startswith('Keep or remove')
  assert ('Settings were removed' if remove else 'Settings were kept') in s.messages[-1]
  assert ('ENVARC:AmiHASS/CameraRefresh.cfg' in s.files)==(not remove)
  assert ('ENVARC:AmiHASS/Favourites.cfg' in s.files)==(not remove)
  assert ('ENVARC:AmiHASS/AmiHASS.cfg' in s.files)==(not remove)
  assert ('ENVARC:AmiHASS/Private/Save-recovery/old' in s.files)==(not remove)
  assert ('ENVARC:AmiHASS/AmiHASS.cfg.old' in s.files)==(not remove)
s=Scenario(installed=True,partial=True).run();assert 'Choose update or uninstall' in s.questions[0]
s=Scenario(installed=True,running=True,restart_fail=True).run();assert 'could not be restarted' in s.messages[-1] and not s.running
s=Scenario(installed=True,stop_fail=True).run();assert s.result=='abort' and not s.copied_files and s.files['WORK:Prefs/AmiHASSPrefs']=='old prefs'
choices=[n for n in walk(tree) if head(n)=='askchoice']
assert len(choices)==2
for node,labels in zip(choices,(['Update AmiHASS','Uninstall AmiHASS'],['Keep settings','Remove settings'])):
 choice=next(n for n in node['items'] if isinstance(n,dict) and head(n)=='choices')
 assert [x[2] for x in choice['items'][1:]]==labels
 default=next(n for n in node['items'] if isinstance(n,dict) and head(n)=='default')
 assert default['items'][1][2]=='0'
assert '"Cancel"' not in (ROOT/'Install').read_text()
print('Installer: all three modes, custom paths, prior running state, truthful results, final results in every mode, optional fresh-install Preferences launch and safe uninstall passed.')

s=Scenario(installed=True,level=0,running=False);s.paths['Boot']='0';s.run();assert 'Boot startup: Disabled' in s.messages[-1] and not s.opened and s.message_levels[-1]>=1
s=Scenario(level=0);s.paths['Boot']='0';s.run();assert 'Boot startup: Enabled' in s.messages[-1] and not s.opened

# Disable automatic Installer welcome before the first RUN, not only later UI.
assert "prefix='(user 0)\\n(onerror" in (ROOT/'tools/prepare_installer_qa.py').read_text()

for level in (0,1,2):
 s=Scenario(level=level,open_prefs=True).run()
 assert s.opened and s.result=='exit'
 assert s.questions[-1]=='Open AmiHASS Preferences now?'
 s=Scenario(installed=True,level=level,open_prefs=True).run()
 assert not s.opened and 'Open AmiHASS Preferences now?' not in s.questions

# Shared components are separate; only Expert chooses their parent.
for mode in (0,1,2):
 s=Scenario(level=mode).run()
 assert s.env['#dest']==('DATA:Shared/AmiHASS' if mode==2 else 'SYS:AmiHASS')
 assert s.files[s.env['#dest']+'/AmiHASSService']=='installed'
 assert any(q=='Choose a parent drawer for shared data' for q in s.questions)==(mode==2)
legacy=Scenario(installed=True)
legacy.paths['Service']='SYS:Utilities/AmiHASS'
legacy.files['SYS:Utilities/AmiHASS/AmiHASSService']='legacy service'
legacy.files['SYS:Utilities/AmiHASS/unrelated']='keep'
legacy.run()
assert legacy.env['#dest']=='SYS:AmiHASS'
assert not legacy.has('SYS:Utilities/AmiHASS/AmiHASSService')
assert legacy.files['SYS:Utilities/AmiHASS/unrelated']=='keep'
print('Shared components: dedicated default, Expert-only parent, legacy migration and unrelated files preserved.')
