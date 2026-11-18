#!/usr/bin/env python3
"""Explicit same-version replacement with preserved originals and scoped tag leases."""
import argparse,datetime,ftplib,hashlib,json,os,shutil,subprocess,sys,urllib.parse
from pathlib import Path
from release import Release,API,GitHub,GitLab,credential,config_read,git,load,need,canonical
from release_support import ReleaseError,atomic_json,sha
from release_readiness import require_ready
ROOT=Path(__file__).resolve().parents[1]

def package_inventory(adapter,version,expected):
    packages=adapter.api.pages(adapter.base+'/packages?package_type=generic&package_name=amihass')
    packages=[p for p in packages if p.get('name')=='amihass' and p.get('version')==version and p.get('package_type')=='generic']
    seen=[]
    for p in packages:
        ident=str(p['id']);need(ident.isdigit(),'Invalid package identity')
        files=adapter.api.pages(adapter.base+'/packages/'+ident+'/package_files')
        for f in files:
            name=f.get('file_name');need(name in expected,'Unrelated file in the selected GitLab package')
            need(f.get('file_sha256')==expected[name],'GitLab package file differs from the saved release')
            seen.append(name)
    need(sorted(seen)==sorted(expected),'Unexpected or duplicate GitLab package inventory')
    return [p['id'] for p in packages]

def delete(api,url):
    response=api.open(url,method='DELETE',missing=True)
    if response is not None:
        with response:need(response.status in (200,202,204),'Deletion was not acknowledged')

def tag_refs(remote,tag):
    rows=git(ROOT,'ls-remote','--tags',remote,'refs/tags/'+tag,'refs/tags/'+tag+'^{}').splitlines()
    return {line.split('\t')[1]:line.split('\t')[0] for line in rows}

def tag_object(manifest):
    who=git(ROOT,'var','GIT_COMMITTER_IDENT')
    body='object '+manifest['commit']+'\ntype commit\ntag '+manifest['tag']+'\ntagger '+who+'\n\n'+manifest['tag_message']+'\n'
    result=subprocess.run(['git','mktag'],cwd=ROOT,input=body,text=True,capture_output=True,check=True)
    value=result.stdout.strip();need(len(value)==40 and all(c in '0123456789abcdef' for c in value),'Invalid replacement tag')
    return value

def replace(output,previous,config,confirmed=False):
    need(confirmed,'Replacement requires --confirm-replace')
    tool=Release(ROOT,config,output=output);m=tool.manifest_check();require_ready(tool);stage=tool.prepared_check(m)
    previous=Path(previous).resolve();need(previous!=tool.out and ROOT/'outputs' in previous.parents,'Use distinct output directories under outputs/')
    prior_state=previous/'release';old=load(prior_state/'manifest.json');receipt=load(prior_state/'receipt.json')
    need(receipt['manifest_sha256']==sha(prior_state/'manifest.json') and set(receipt['completed'])=={'github','gitlab'} and receipt['current'] is None,'Previous repository publication is incomplete')
    need(old['tag']==m['tag']==m['version']==old['version']=='0.1','Replacement is limited to the existing 0.1 release')
    need(set(old['files'])==set(m['files']) and old['commit']!=m['commit'],'Replacement must preserve archive names and use the updated source commit')
    for service in ('github','gitlab'):need(m[service]==old[service] and m[service+'_git']==old[service+'_git'],'Repository identity changed')
    old_stage=prior_state/'publish/outputs'
    for name,digest in old['files'].items():need(sha(old_stage/name)==digest,'Original published backup changed')
    adapters={service:kind(m,stage,token=credential(service,config)) for service,kind in [('github',GitHub),('gitlab',GitLab)]}
    state_path=tool.state/'replacement-receipt.json';backup=tool.state/'replacement-backup'
    if state_path.exists():
        state=load(state_path);need(state['manifest_sha256']==sha(tool.manifest),'Replacement belongs to another build')
    else:
        gh,gl=adapters['github'],adapters['gitlab'];gh_old=GitHub(old,old_stage,api=gh.api);gl_old=GitLab(old,old_stage,api=gl.api)
        old_gh=gh_old.release();need(old_gh and not old_gh.get('immutable',False),'The existing GitHub release is missing or immutable')
        gh_old.preflight();gh_old.verify(old_gh,True);old_gl=gl_old.release();need(old_gl,'The existing GitLab release is missing');gl_old.preflight();gl_old.verify(old_gl);gl_old.packages(True)
        if config['aminet']:
            with ftplib.FTP('main.aminet.net',timeout=60) as ftp:
                ftp.login('anonymous',config['uploader']);ftp.cwd('/new')
                need(not ({Path(n).name for n in ftp.nlst()} & {'AmiHASS.lha','AmiHASS.readme'}),'Aminet has an incoming submission; inspect before replacing')
        ids=package_inventory(gl,old['version'],old['files'])
        need(sorted((x['name'],x['url']) for x in gl.links())==sorted((x['name'],x['url']) for x in gl_old.links()),'Replacement changes stable GitLab download links')
        refs=[tag_refs(m[s+'_git'],m['tag']) for s in ('github','gitlab')]
        direct='refs/tags/'+m['tag'];peeled=direct+'^{}';old_tag=refs[0].get(direct)
        need(old_tag and all(r.get(direct)==old_tag and r.get(peeled)==old['commit'] for r in refs),'Existing release tags differ from the saved publication')
        need(git(ROOT,'rev-parse',direct)==old_tag,'Local release tag differs from the published tag')
        need(not backup.exists(),'Inspect existing replacement backup before restarting')
        backup.mkdir()
        shutil.copy2(prior_state/'manifest.json',backup/'manifest.json');shutil.copy2(prior_state/'receipt.json',backup/'receipt.json')
        for name in old['files']:shutil.copy2(old_stage/name,backup/name)
        atomic_json(backup/'github-release.json',old_gh);atomic_json(backup/'gitlab-release.json',old_gl)
        state={'schema':1,'manifest_sha256':sha(tool.manifest),'previous_manifest_sha256':sha(prior_state/'manifest.json'),
               'old_tag':old_tag,'new_tag':tag_object(m),'github_release_id':old_gh['id'],'old_github_assets':{a['name']:a['id'] for a in gh_old.assets(old_gh)},
               'gitlab_packages':ids,'completed':[],'current':None,'gitlab_removed':False,'aminet_acknowledged':[]}
        atomic_json(state_path,state)
    def save():atomic_json(state_path,state)
    direct='refs/tags/'+m['tag'];peeled=direct+'^{}'
    for service in ('github','gitlab'):
        remote=m[service+'_git'];refs=tag_refs(remote,m['tag'])
        need(refs.get(direct) in (state['old_tag'],state['new_tag']),'Remote tag changed concurrently')
        if refs[direct]==state['old_tag']:
            git(ROOT,'push','--no-follow-tags','--force-with-lease='+direct+':'+state['old_tag'],remote,state['new_tag']+':'+direct)
        refs=tag_refs(remote,m['tag']);need(refs.get(direct)==state['new_tag'] and refs.get(peeled)==m['commit'],'Replacement tag verification failed')
    backup_ref='refs/backup/amihass/0.1/'+state['old_tag'];git(ROOT,'update-ref',backup_ref,state['old_tag'])
    local=git(ROOT,'rev-parse',direct);need(local in (state['old_tag'],state['new_tag']),'Local release tag changed concurrently')
    if local!=state['new_tag']:git(ROOT,'update-ref',direct,state['new_tag'],state['old_tag'])
    gh=adapters['github'];ident=str(state['github_release_id'])
    if 'github' not in state['completed']:
        state['current']='github';save();r=gh.release();need(r and str(r['id'])==ident and not r.get('immutable',False),'GitHub release identity changed')
        need(r.get('body') in (old['notes'],m['notes']),'GitHub notes changed concurrently')
        gh.api.json(gh.base+'/releases/'+ident,'PATCH',{'draft':True})
        existing={a['name']:a for a in gh.assets(r)};need(set(existing)<=set(m['files']),'Unexpected GitHub assets')
        for name,digest in m['files'].items():
            a=existing.get(name)
            if a and a.get('digest')=='sha256:'+digest and a.get('size')==(stage/name).stat().st_size:continue
            if a:
                need(a['id']==state['old_github_assets'][name] and a.get('digest')=='sha256:'+old['files'][name],'GitHub asset changed concurrently')
                delete(gh.api,gh.base+'/releases/assets/'+str(a['id']))
            result=gh.api.json(gh.upload+'/releases/'+ident+'/assets?name='+urllib.parse.quote(name),'POST',file=stage/name)
            need(result.get('digest')=='sha256:'+digest and result.get('state')=='uploaded','GitHub replacement upload did not verify')
        gh.api.json(gh.base+'/releases/'+ident,'PATCH',{'name':'AmiHASS '+m['version'],'body':m['notes'],'draft':False,'prerelease':False,'make_latest':'true'})
        gh.verify(gh.release(),True);need(gh.api.json(gh.base+'/releases/latest')['tag_name']==m['tag'],'GitHub latest tag mismatch')
        state['completed'].append('github');state['current']=None;save();print('GitHub replacement verified.',flush=True)
    gl=adapters['gitlab']
    if 'gitlab' not in state['completed']:
        state['current']='gitlab';save()
        if not state['gitlab_removed']:
            for ident in state['gitlab_packages']:
                current=gl.api.json(gl.base+'/packages/'+str(ident),missing=True)
                if current:
                    need(current.get('name')=='amihass' and current.get('version')==m['version'],'GitLab package identity changed')
                    files=gl.api.pages(gl.base+'/packages/'+str(ident)+'/package_files')
                    need(all(f['file_name'] in old['files'] and f.get('file_sha256')==old['files'][f['file_name']] for f in files),'GitLab package changed concurrently')
                    delete(gl.api,gl.base+'/packages/'+str(ident))
            state['gitlab_removed']=True;save()
        present=gl.packages()
        for name in m['files']:
            if name not in present:gl.api.json(gl.package_url(name),'PUT',file=stage/name)
        gl.packages(True)
        current=gl.release();need(current and current.get('description') in (old['notes'],m['notes']),'GitLab notes changed concurrently')
        gl.api.json(gl.base+'/releases/'+urllib.parse.quote(m['tag'],safe=''),'PUT',{'name':'AmiHASS '+m['version'],'description':m['notes'],'released_at':datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')})
        gl.verify(gl.release());need(gl.api.json(gl.base+'/releases/permalink/latest')['tag_name']==m['tag'],'GitLab latest tag mismatch')
        state['completed'].append('gitlab');state['current']=None;save();print('GitLab replacement verified.',flush=True)
    if 'aminet' not in state['completed']:
        need(config['aminet'],'Aminet is not configured')
        folder=tool.state/'aminet-update';folder.mkdir(exist_ok=True)
        source=tool.state/'aminet';archive=folder/'AmiHASS.lha';readme=folder/'AmiHASS.readme'
        if not archive.exists():shutil.copy2(source/'AmiHASS.lha',archive)
        if not readme.exists():
            text=(source/'AmiHASS.readme').read_text();text=text.replace('Distribution: Aminet\n','Distribution: Aminet\nReplaces:     util/wb/AmiHASS.lha\n');readme.write_text(text,encoding='ascii')
        need(sha(archive)==sha(stage/('AmiHASS-'+m['version']+'.lha')),'Aminet archive differs from verified runtime')
        hashes={p.name:sha(p) for p in (archive,readme)}
        if state.get('aminet_files'):need(state['aminet_files']==hashes,'Aminet replacement inputs changed')
        state['aminet_files']=hashes;state['current']='aminet';save()
        with ftplib.FTP('main.aminet.net',timeout=60) as ftp:
            ftp.login('anonymous',config['uploader']);ftp.cwd('/new')
            incoming={Path(name).name for name in ftp.nlst()}
            for name in hashes:
                if name in state['aminet_acknowledged']:continue
                need(name not in incoming,'Aminet incoming file already exists; inspect pending submission before retry')
                from release import HashingReader
                with (folder/name).open('rb') as stream:
                    reader=HashingReader(stream);ftp.storbinary('STOR '+name,reader)
                    need(reader.digest.hexdigest()==hashes[name],'Aminet upload input changed')
                state['aminet_acknowledged'].append(name);save()
        state['aminet_state']='submitted_awaiting_moderation';state['completed'].append('aminet');state['current']=None;save()
        print('Aminet replacement submitted as AmiHASS.lha and AmiHASS.readme; awaiting moderation.',flush=True)
    need(set(state['completed'])=={'github','gitlab','aminet'},'Replacement is incomplete')
    print('Replacement completed; originals retained in '+str(backup),flush=True)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True);p.add_argument('--previous',required=True)
    p.add_argument('--config',type=Path,default=Path(os.environ.get('AMIHASS_RELEASE_CONFIG',str(Path.home()/'Development/tools/git-release/projects/amihass.json'))))
    p.add_argument('--confirm-replace',action='store_true');a=p.parse_args()
    replace(a.output,a.previous,config_read(ROOT,a.config),a.confirm_replace)
if __name__=='__main__':
    try:main()
    except (ReleaseError,subprocess.CalledProcessError,ftplib.Error) as e:
        print('Replacement stopped: '+(str(e) if isinstance(e,ReleaseError) else type(e).__name__),file=sys.stderr);sys.exit(1)
