"""Offline release regression tests. No credentials, network or native build needed."""
import importlib.util
import hashlib
import json
import copy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import release_support as support
import release
rel=release
ReleaseError=support.ReleaseError
sha=support.sha

class GithubAPI:
    def __init__(self):
        self.r = None
        self.assets = []
        self.events = []
        self.bad_digest = False
        self.fail_upload = None
    def pages(self, url):
        return copy.deepcopy(self.assets if '/assets?' in url or url.endswith('/assets') else ([self.r] if self.r else []))
    def json(self, url, method='GET', data=None, file=None, missing=False):
        self.events.append((method, url))
        if method == 'GET':
            if url.endswith('/releases/latest'):
                return copy.deepcopy(self.r)
            return {'html_url': 'https://github.com/retrocs-org/amihass', 'permissions': {'push': True}}
        if file:
            if self.fail_upload == file.name:
                raise ReleaseError('Injected interrupted upload')
            a = {'name': file.name, 'digest': None if self.bad_digest else 'sha256:' + sha(file),
                 'state': 'uploaded', 'size': file.stat().st_size}
            self.assets.append(a)
            return copy.deepcopy(a)
        if method == 'POST':
            self.r = dict(data, id=1)
            return copy.deepcopy(self.r)
        if method == 'PATCH':
            self.r.update(data)
            return copy.deepcopy(self.r)
        raise AssertionError((method, url))

class GitlabAPI:
    def __init__(self, url):
        self.url, self.r, self.files, self.events = url, None, {}, []
        self.corrupt = False
    def json(self, url, method='GET', data=None, file=None, missing=False):
        self.events.append((method, url))
        if method == 'PUT' and '/releases/' in url:
            self.r.update(data)
            self.r['upcoming_release'] = False
            return self.r
        if method == 'PUT':
            self.files[url] = file.read_bytes() + (b'bad' if self.corrupt else b'')
            return {'message': '201 Created'}
        if method == 'POST':
            self.r = data
            return data
        if '/releases/' in url:
            return self.r
        return {'web_url': self.url, 'permissions': {'group_access': {'access_level': 40}}}
    def download_hash(self, url, size):
        self.events.append(('VERIFY', url))
        data = self.files.get(url)
        return hashlib.sha256(data).hexdigest() if data is not None else None

def module(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/(name+'.py'))
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result

class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        (self.root/'VERSION').write_text('VERSION=0.1\nDATE=17.11.2026\n')
        self.config=release.config_read(self.root,self.root/'missing.json')

    def test_version_and_paths(self):
        tool=release.Release(self.root,self.config,output='outputs/release-0.1')
        self.assertEqual(tool.payload_names,['AmiHASS-0.1-sdk.zip','AmiHASS-0.1.lha'])
        for output in ('../outside','outputs/release','outputs/release/private'):
            with self.assertRaises(support.ReleaseError):release.Release(self.root,self.config,output=output)
        (self.root/'VERSION').write_text('VERSION=0.3\n')
        self.assertEqual(support.version(self.root),'0.3')
        (self.root/'VERSION').write_text('VERSION=0.4\n')
        with self.assertRaises(support.ReleaseError):support.version(self.root)

    @patch("release_support.time.time", return_value=1795163518)
    def test_current_archive_dates_and_corruption(self, clock):
        entries={'AmiHASS/hello':b'hello world','AmiHASS/empty':b''}
        a=self.root/'a.lha';b=self.root/'b.lha'
        support.lha_write(a,entries);support.lha_write(b,entries)
        self.assertEqual(a.read_bytes(),b.read_bytes());support.verify_lha(a,entries)
        import time, struct, zipfile
        stamp=time.localtime(clock.return_value)[:6]
        dos=struct.unpack_from('<I',a.read_bytes(),15)[0]
        self.assertEqual((1980+(dos>>25), (dos>>21)&15, (dos>>16)&31, (dos>>11)&31, (dos>>5)&63, (dos&31)*2), stamp[:5]+(stamp[5]//2*2,))
        good=a.read_bytes()
        for invalid in (good[:-1],good+b'extra',good[:-2]+b'x\0'):
            a.write_bytes(invalid)
            with self.assertRaises(support.ReleaseError):support.verify_lha(a,entries)
        a.write_bytes(good)
        with self.assertRaises(support.ReleaseError):support.verify_lha(a,{'other':b''})
        items=[(n,d,False) for n,d in entries.items()]
        z=self.root/'a.zip';z2=self.root/'b.zip'
        support.zip_write(z,items);support.zip_write(z2,items)
        self.assertEqual(z.read_bytes(),z2.read_bytes());support.verify_zip(z,entries)
        with zipfile.ZipFile(z) as archive:
            self.assertTrue(all(i.date_time == stamp[:5]+(stamp[5]//2*2,) for i in archive.infolist()))
        with self.assertRaises(support.ReleaseError):support.verify_zip(z,{'other':b''})

    def test_lha_paths_are_portable_for_amiga_extractors(self):
        import struct
        from types import SimpleNamespace
        entries={'AmiHASS/Docs/empty':b'', 'AmiHASS/SDK/nested/example.c':b'payload\xff\\bytes'}
        path=self.root/'paths.lha'
        def member_names(raw):
            at=0;names=[]
            while raw[at]:
                size=raw[at];body=raw[at+2:at+2+size]
                self.assertEqual(sum(body)&255,raw[at+1]);names.append(bytes(body[20:20+body[19]]))
                at+=2+size+struct.unpack_from('<I',body,5)[0]
            self.assertEqual(at+1,len(raw));return names
        support.lha_write(path,entries)
        self.assertEqual(member_names(path.read_bytes()),[n.encode() for n in sorted(entries)])
        support.verify_lha(path,entries,canonical_paths=True)
        def fake_compressor(*args,**kwargs):
            support.lha_write(path,entries)
            raw=bytearray(path.read_bytes());at=0
            while raw[at]:
                size=raw[at];body=bytearray(raw[at+2:at+2+size]);n=body[19]
                body[20:20+n]=body[20:20+n].replace(b'/',separator)
                raw[at+2:at+2+size]=body;raw[at+1]=sum(body)&255
                at+=2+size+struct.unpack_from('<I',body,5)[0]
            path.write_bytes(raw)
            with self.assertRaises(support.ReleaseError):support.verify_lha(path,entries,canonical_paths=True)
            return SimpleNamespace(returncode=0)
        for separator in (b'\xff',b'\\'):
            with patch.object(support.subprocess,'run',side_effect=fake_compressor):
                support.lha_write(path,entries,'fixture-compressor')
            self.assertEqual(member_names(path.read_bytes()),[n.encode() for n in sorted(entries)])
            support.verify_lha(path,entries,canonical_paths=True)
            before=path.read_bytes();support.lha_portable_paths(path);self.assertEqual(before,path.read_bytes())
        invalid=path.read_bytes()[:-1];path.write_bytes(invalid)
        with self.assertRaises(support.ReleaseError):support.lha_portable_paths(path)
        self.assertEqual(path.read_bytes(),invalid)

    def test_api_redirects_keep_credentials_on_the_same_read_origin(self):
        import urllib.request
        handler=release.SameOriginReadRedirect()
        req=urllib.request.Request('https://gitlab.example.net/api/v4/projects/team%2Fapp/releases/permalink/latest',headers={'PRIVATE-TOKEN':'fixture-only'})
        result=handler.redirect_request(req,None,302,'Found',{},'https://gitlab.example.net/api/v4/projects/57/releases/0.1')
        self.assertEqual(result.get_header('Private-token'),'fixture-only')
        for destination in ('https://evil.invalid/file','http://gitlab.example.net/file',urllib.parse.urlunsplit(('https','user:password@gitlab.example.net','/file','',''))):
            self.assertIsNone(handler.redirect_request(req,None,302,'Found',{},destination))
        for method in ('POST','PUT','DELETE'):
            request=urllib.request.Request(req.full_url,headers={'PRIVATE-TOKEN':'fixture-only'},method=method)
            self.assertIsNone(handler.redirect_request(request,None,302,'Found',{},'https://gitlab.example.net/elsewhere'))

    def test_replacement_requires_explicit_confirmation(self):
        replacement=module('replace-release')
        with self.assertRaises(support.ReleaseError):replacement.replace('unused','unused',{},False)

    def test_replacement_only_targets_matching_package_files(self):
        replacement=module('replace-release');digest='a'*64
        class PackageAPI:
            files=[{'file_name':'AmiHASS-0.1.lha','file_sha256':digest}]
            def pages(self,url):
                if url.endswith('/package_files'):return self.files
                return [{'id':7,'name':'amihass','version':'0.1','package_type':'generic'}, {'id':8,'name':'amihass','version':'0.2','package_type':'generic'}]
        api=PackageAPI();adapter=type('Adapter',(),{'base':'https://gitlab.example.net/api/v4/projects/57','api':api})()
        self.assertEqual(replacement.package_inventory(adapter,'0.1',{'AmiHASS-0.1.lha':digest}),[7])
        api.files=[{'file_name':'unrelated.zip','file_sha256':digest}]
        with self.assertRaises(support.ReleaseError):replacement.package_inventory(adapter,'0.1',{'AmiHASS-0.1.lha':digest})
        api.files=[{'file_name':'AmiHASS-0.1.lha','file_sha256':'b'*64}]
        with self.assertRaises(support.ReleaseError):replacement.package_inventory(adapter,'0.1',{'AmiHASS-0.1.lha':digest})
        api.files=[{'file_name':'AmiHASS-0.1.lha','file_sha256':digest}]*2
        with self.assertRaises(support.ReleaseError):replacement.package_inventory(adapter,'0.1',{'AmiHASS-0.1.lha':digest})

    def test_public_release_tag_and_change_headings(self):
        self.assertEqual(release.release_tag(None,'0.1'),'0.1')
        self.assertEqual(release.release_tag('0.1','0.1'),'0.1')
        for tag in ('v0.1','AmiHASS-0.1','0.2','../0.1'):
            with self.assertRaises(support.ReleaseError):release.release_tag(tag,'0.1')
        (self.root/'CHANGELOG.MD').write_text('# Changelog\n\n## AmiHASS 0.1\n\n### Features\n\n- A feature.\n\n### Enhancements\n\n- An enhancement.\n\n### Bug fixes\n\n- A bug corrected.\n')
        expected='### Features\n\n- A feature.\n\n### Enhancements\n\n- An enhancement.\n\n### Bugs\n\n- A bug corrected.'
        notes=release.changelog_notes(self.root,'0.1')
        self.assertEqual(notes,expected);release.validate_release_notes(self.root,'0.1',notes)
        for extra in ('# AmiHASS 0.1\n','An introduction.\n','### Downloads\n'):
            with self.assertRaises(support.ReleaseError):release.validate_release_notes(self.root,'0.1',extra+notes)
        for invalid in ('### Downloads\n\n- A file.','### Features\n\nInstallation instructions.','### Features\n- One.\n### Features\n- Two.'):
            (self.root/'CHANGELOG.MD').write_text('## AmiHASS 0.1\n\n'+invalid+'\n')
            with self.assertRaises(support.ReleaseError):release.changelog_notes(self.root,'0.1')
        (self.root/'CHANGELOG.MD').write_text('## AmiHASS 0.1\n\n### Features\n\n- First release.\n\n### Enhancements\n\n### Bugs\n')
        self.assertEqual(release.changelog_notes(self.root,'0.1'),'### Features\n\n- First release.')
        self.assertEqual((ROOT/'docs/RELEASE-NOTES.md').read_text().strip(),release.changelog_notes(ROOT,support.version(ROOT)))

    def test_aminet_readme_includes_full_changelog(self):
        mod=module('make-aminet-release')
        (self.root/'VERSION').write_text('VERSION=0.2\n')
        (self.root/'CHANGELOG.MD').write_text('# Changelog\n\n## AmiHASS 0.2\n\n### Features\n\n- Latest feature.\n\n### Enhancements\n\n- Current polish.\n\n## AmiHASS 0.1\n\n### Features\n\n- First version feature.\n')
        with patch.object(mod,'ROOT',self.root):
            text=mod.readme('Test Author','test@example.org','AmiHASS.lha').decode('ascii')
        self.assertIn('Version:      0.2',text)
        for line in ('Changelog','AmiHASS 0.2','- Latest feature.','- Current polish.','AmiHASS 0.1','- First version feature.'):
            self.assertIn(line,text)
        self.assertLess(text.index('AmiHASS 0.2'),text.index('AmiHASS 0.1'))
        self.assertNotIn('##',text)

    def test_aminet_upload_names_are_not_versioned(self):
        mod=module('make-aminet-release')
        self.assertIn(b'Unpack AmiHASS.lha',mod.readme('Test Author','test@example.org','AmiHASS.lha'))
        with self.assertRaises(support.ReleaseError):mod.readme('Test Author','test@example.org','AmiHASS-0.1.lha')
        names=['AmiHASS.lha','AmiHASS.readme'];uploads=[]
        for name in names:(self.root/name).write_bytes(name.encode())
        class FTP:
            def __init__(self,*args,**kwargs):pass
            def __enter__(self):return self
            def __exit__(self,*args):pass
            def login(self,*args):pass
            def cwd(self,*args):pass
            def nlst(self):return []
            def storbinary(self,command,reader):uploads.append((command,reader.read()))
        manifest={'aminet':True,'uploader':'test@example.org','aminet_files':{n:sha(self.root/n) for n in names}}
        adapter=release.Aminet(manifest,self.root,self.root/'receipt.json','a'*64,FTP)
        adapter.publish();self.assertEqual([command for command,data in uploads],['STOR AmiHASS.lha','STOR AmiHASS.readme'])
        adapter.publish();self.assertEqual(len(uploads),2)

    def test_stage_preserves_drawers_and_unrelated_files(self):
        stage=module('stage-builds');source=self.root/'source';target=self.root/'target'
        (source/'Docs').mkdir(parents=True);(target/'Docs').mkdir(parents=True)
        (source/'Docs/file').write_text('new');(target/'Docs/file').write_text('old')
        (target/'mine').write_text('keep');inode=(target/'Docs').stat().st_ino
        stage.copy_files(source,target,['Docs/file'])
        self.assertEqual((target/'Docs').stat().st_ino,inode)
        self.assertEqual((target/'Docs/file').read_text(),'new')
        self.assertEqual((target/'mine').read_text(),'keep')
        (target/'linked').symlink_to(target/'Docs',target_is_directory=True)
        with self.assertRaises(ValueError):stage.copy_files(source,target,['linked/file'])
        with self.assertRaises(ValueError):stage.copy_files(source,target,['../escape'])

    def test_native_provenance_rejects_changed_binary_and_source(self):
        native=module('build-provenance')
        for name in ('Makefile','tools/build-amiga.py','tools/build-provenance.py'):
            p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture')
        for name in native.BINARIES:
            (self.root/name).write_bytes(b'\0\0\3\xf3'+('$VER: '+name+' 0.1').encode())
        native.record(self.root,'vc','fixture','-O','0'*64);native.verify(self.root)
        binary=self.root/native.BINARIES[0];original=binary.read_bytes();binary.write_bytes(original+b'changed')
        with self.assertRaises(support.ReleaseError):native.verify(self.root)
        binary.write_bytes(original);(self.root/'Makefile').write_text('changed')
        with self.assertRaises(support.ReleaseError):native.verify(self.root)

    def test_prepared_copies_cannot_be_changed(self):
        tool=release.Release(self.root,self.config,output='outputs/release-0.1')
        stage=tool.state/'publish/outputs';stage.mkdir(parents=True)
        asset=stage/tool.names[0];asset.write_bytes(b'reviewed')
        manifest={'files':{asset.name:support.sha(asset)}}
        tool.prepared_check(manifest);asset.write_bytes(b'changed')
        with self.assertRaises(support.ReleaseError):tool.prepared_check(manifest)

    def test_config_refuses_credentials_in_build_env(self):
        path=self.root/'config.json';path.write_text(json.dumps({'env':{'GITHUB_TOKEN':'fixture'}}))
        with self.assertRaises(support.ReleaseError):release.config_read(self.root,path)
        path.write_text(json.dumps({'github':'https://example.invalid/repo'}))
        with self.assertRaises(support.ReleaseError):release.config_read(self.root,path)

    def test_github_app_mints_fresh_scoped_tokens_without_secret_output(self):
        import subprocess
        from types import SimpleNamespace
        helper=self.root/'create-repo-token.sh';helper.write_text('#!/bin/sh\nexit 99\n');helper.chmod(0o700)
        path=self.root/'app-config.json'
        spec={'provider':'github_app','helper':str(helper),'vault':'Fixture','item':'github.com'}
        path.write_text(json.dumps({'credentials':{'github':spec}}));config=release.config_read(self.root,path)
        responses=[SimpleNamespace(returncode=0,stdout='ghs_fixture_one\n',stderr='private helper output'),SimpleNamespace(returncode=0,stdout='ghs_fixture_two\n',stderr='')]
        with patch.object(release.subprocess,'run',side_effect=responses) as run:
            self.assertEqual(release.credential('github',config),'ghs_fixture_one')
            self.assertEqual(release.credential('github',config),'ghs_fixture_two')
        self.assertEqual(run.call_count,2)
        for call in run.call_args_list:
            self.assertEqual(call.args[0],[str(helper),'github','https://github.com','retrocs-org/amihass','rw'])
            self.assertEqual(call.kwargs['env']['GITHUB_OP_SAVE'],'0')
            self.assertEqual(call.kwargs['env']['GITHUB_WORKFLOWS'],'none')
            self.assertTrue(call.kwargs['capture_output'])
            self.assertEqual(call.kwargs['timeout'],180)
        for result in (SimpleNamespace(returncode=1,stdout='private-token',stderr='private-key'),SimpleNamespace(returncode=0,stdout='bad\nprivate-token',stderr='')):
            with patch.object(release.subprocess,'run',return_value=result):
                with self.assertRaises(support.ReleaseError) as error:release.credential('github',config)
            self.assertNotIn('private-token',str(error.exception));self.assertNotIn('private-key',str(error.exception))
        with patch.object(release.subprocess,'run',side_effect=subprocess.TimeoutExpired('helper',180)):
            with self.assertRaises(support.ReleaseError):release.credential('github',config)
        path.write_text(json.dumps({'credentials':{'gitlab':spec}}))
        with self.assertRaises(support.ReleaseError):release.config_read(self.root,path)

    def test_publish_requires_confirmation_before_network(self):
        tool=release.Release(self.root,self.config,output='outputs/release-0.1')
        with patch.object(tool,'manifest_check') as check:
            with self.assertRaises(support.ReleaseError):tool.publish(['github'],False)
            check.assert_not_called()

    def test_changelog_notes_and_runtime_inventory(self):
        notes=release.changelog_notes(ROOT,'0.1')
        release.validate_release_notes(ROOT,'0.1',notes)
        with self.assertRaises(support.ReleaseError):release.validate_release_notes(ROOT,'0.1','wrong notes')
        from inventory import runtime_inventory,sdk_inventory
        with patch('inventory.validate',side_effect=lambda root,items:items):
            runtime=runtime_inventory(ROOT);sdk=sdk_inventory(ROOT)
        for name in ('AmiHASSPrefs.info','AmiHASSBrowser.info','AmiHASSDashboard.info','Install'):
            self.assertIn(name,runtime)
        self.assertIn('AmiHASSExample',sdk);self.assertNotIn('AmiHASSExample',runtime)

    def publication_fixture(self):
        files={}
        for name in ('AmiHASS-0.1.lha','AmiHASS-0.1-sdk.zip'):
            path=self.root/name;path.write_bytes(name.encode());files[name]=support.sha(path)
        self.tool=type('Fixture',(),{'out':self.root})()
        return {'version':'0.1','tag':'0.1','commit':'a'*40,'notes':release.changelog_notes(ROOT,'0.1'),
                'github':self.config['github'],'files':files}

    def test_source_inventory_excludes_local_and_generated_inputs(self):
        required=('LICENSE','README.MD','Makefile','src/main.c','tools/release.py','public-web/index.php','public-web/inc/releases.php','SDK/README.MD')
        for name in required:
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('public')
        for name in ('work/private','outputs/archive.zip','src/main.o','AGENTS.md'):
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('local')
        files=support.source_files(self.root)
        self.assertIn('public-web/index.php',files)
        self.assertFalse(any(name in files for name in ('work/private','outputs/archive.zip','src/main.o','AGENTS.md')))
        (self.root/'credential.key').write_text('private')
        with self.assertRaises(support.ReleaseError):support.source_files(self.root)
        (self.root/'credential.key').unlink();(self.root/'linked').symlink_to(self.root/'README.MD')
        with self.assertRaises(support.ReleaseError):support.source_files(self.root)

    def test_github_draft_published_only_after_hashes(self):
            m = self.publication_fixture();api = GithubAPI();adapter = rel.GitHub(m, self.tool.out, api)
            adapter.preflight();adapter.publish()
            self.assertFalse(api.r['draft'])
            self.assertIs(api.r['prerelease'],False)
            self.assertEqual(api.r['make_latest'],'true')
            self.assertEqual(api.r['name'],'AmiHASS 0.1')
            self.assertEqual(api.r['tag_name'],'0.1')
            self.assertEqual(api.r.get('body',api.r.get('description')),m['notes'])
            patch_index = next(i for i, e in enumerate(api.events) if e[0] == 'PATCH')
            uploads = [i for i, e in enumerate(api.events) if 'uploads.github.com' in e[1]]
            self.assertEqual(len(uploads), len(m['files']))
            self.assertTrue(all(i < patch_index for i in uploads))
            adapter.publish()
            self.assertEqual(len([e for e in api.events if 'uploads.github.com' in e[1]]), len(m['files']))

    def test_github_existing_prerelease_promoted_after_verification(self):
            m=self.publication_fixture();api=GithubAPI();adapter=rel.GitHub(m,self.tool.out,api)
            adapter.publish()
            api.r.update(prerelease=True,make_latest='false')
            adapter.publish()
            self.assertFalse(api.r['prerelease'])
            self.assertEqual(api.r['make_latest'],'true')

    def test_gitlab_normal_release_has_current_date_and_latest_check(self):
            m=self.publication_fixture();m['gitlab']='https://gitlab.example.net/team/amihass'
            api=GitlabAPI(m['gitlab']);adapter=rel.GitLab(m,self.tool.out,api)
            adapter.publish()
            self.assertIn('released_at',api.r)
            self.assertEqual(api.r['name'],'AmiHASS 0.1')
            self.assertEqual(api.r['tag_name'],'0.1')
            self.assertEqual(api.r.get('body',api.r.get('description')),m['notes'])
            self.assertIn(('GET',adapter.base+'/releases/permalink/latest'),api.events)
            api.r['upcoming_release']=True
            adapter.publish()
            self.assertFalse(api.r['upcoming_release'])

    def test_unknown_digest_and_conflict_keep_draft(self):
            m = self.publication_fixture();api = GithubAPI();api.bad_digest = True
            with self.assertRaises(ReleaseError): rel.GitHub(m, self.tool.out, api).publish()
            self.assertTrue(api.r['draft'])
            with self.assertRaises(ReleaseError): rel.GitHub(m, self.tool.out, api).preflight()
            self.assertFalse(any(e[0] == 'PATCH' for e in api.events))

    def test_matching_github_assets_reused_after_interruption(self):
            m = self.publication_fixture();api = GithubAPI();api.fail_upload = list(m['files'])[1]
            with self.assertRaises(ReleaseError): rel.GitHub(m, self.tool.out, api).publish()
            self.assertEqual(len(api.assets), 1)
            api.fail_upload = None
            rel.GitHub(m, self.tool.out, api).publish()
            self.assertEqual(len(api.assets), len(m['files']))

    def test_gitlab_downloads_before_advertising_and_refuses_conflict(self):
            m = self.publication_fixture();m['gitlab'] = 'https://gitlab.example.net/team/nested/amihass'
            api = GitlabAPI(m['gitlab']);adapter = rel.GitLab(m, self.tool.out, api)
            adapter.preflight();adapter.publish()
            post = next(i for i, e in enumerate(api.events) if e[0] == 'POST')
            for name in m['files']:
                self.assertTrue(any(i < post and e == ('VERIFY', adapter.package_url(name)) for i,e in enumerate(api.events)))
            api.files[adapter.package_url(next(iter(m['files'])))] = b'conflict'
            with self.assertRaises(ReleaseError): adapter.preflight()

    def test_gitlab_corruption_never_creates_release(self):
            m = self.publication_fixture();m['gitlab'] = 'https://gitlab.example.net/team/amihass'
            api = GitlabAPI(m['gitlab']);api.corrupt = True
            with self.assertRaises(ReleaseError): rel.GitLab(m, self.tool.out, api).publish()
            self.assertIsNone(api.r)

if __name__=='__main__':unittest.main()
