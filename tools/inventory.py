"""One explicit inventory for runtime, SDK, staging and release verification."""
from pathlib import Path
BINARIES=('AmiHASSExample','AmiHASSPrefs','AmiHASSService','AmiHASSFetch','AmiHASSBrowser','AmiHASSDashboard','StartInstaller','SetupStartup')
RUNTIME=('AmiHASSDashboard','AmiHASSDashboard.info','AmiHASSBrowser','AmiHASSBrowser.info','AmiHASSPrefs','AmiHASSPrefs.info','AmiHASSService','AmiHASSFetch','StartInstaller','SetupStartup','Install','Install.info','README.txt','README.MD','CHANGELOG.MD','LICENSE','LICENSE.MD','NOTICE','QUICKSTART.txt','REQUIREMENTS.txt')

def validate(root, inventory):
    for name,source in inventory.items():
        for text in (name,source):
            p=Path(text)
            if p.is_absolute() or '..' in p.parts:raise ValueError('Invalid package member: '+name)
        p=root/source
        if p.is_symlink() or not p.is_file() or any(q.is_symlink() for q in (p,*p.parents) if q==root or root in q.parents):
            raise ValueError('Missing or linked package input: '+source)
    return dict(sorted(inventory.items()))

def runtime_inventory(root):
    result={name:name for name in RUNTIME}
    for name in ('USAGE.md','SDK.md','SELECTED_UPDATES.md','AMI2HA-LICENSE.txt'):
        result['Docs/'+name]='docs/'+name
    result['Docs/RELEASE-LICENSE.txt']='tools/RELEASE-LICENSE.txt'
    result['AmiHASS.guide']='docs/AmiHASS.guide'
    for language in ('svenska','deutsch'):
        result['Catalogs/'+language+'/amihass.catalog']='Catalogs/'+language+'/amihass.catalog'
    for name in ('README.MD','Status.rexx','Discover.rexx','SampleLight.rexx','WaitEvent.rexx'):
        result['ARexx/'+name]='ARexx/'+name
    return validate(root,result)

def sdk_inventory(root):
    names=('amihass.h','amihass_core.h','utf8_display.h','amihass_config.h','amihass_client.c','quote.c','example.c','nd_reply.h','nd_interaction.h','README.MD','Makefile')
    result={name:'SDK/'+name for name in names}
    result.update({'REFERENCE.md':'docs/SDK.md','AmiHASSExample':'AmiHASSExample','LICENSE':'LICENSE','NOTICE':'NOTICE','RELEASE-LICENSE.txt':'tools/RELEASE-LICENSE.txt'})
    return validate(root,result)
