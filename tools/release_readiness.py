"""One source-bound readiness report; stale or missing checks never count as passes."""
import hashlib,json,xml.etree.ElementTree as ET
from pathlib import Path
from release_support import ReleaseError,atomic_json,sha
REQUIRED=('installer','integration','sample-memory')
OPTIONAL=('https-memory','sample-stability')
SUITES={
 'installer':'qa/installer.json',
 'integration':'qa/sample-bridge.json',
 'sample-memory':'qa/sample-memory.json',
 'sample-stability':'qa/sample-stability.json',
 'https-memory':'qa/https-memory.json',
}
def binding(tool):
    record=json.loads(tool.record.read_text())
    return {'source_sha256':record['source_sha256'],'native_record_sha256':record['native_record_sha256'],
            'artifacts':record['artifacts']}
def junit_result(path):
    raw=Path(path).read_bytes()
    if b'<!DOCTYPE' in raw or len(raw)>2000000:raise ReleaseError('Invalid or oversized JUnit report')
    root=ET.fromstring(raw);cases=list(root.iter('testcase'))
    bad=any(any(child.tag in ('failure','error','skipped') for child in case) for case in cases)
    for suite in list(root.iter('testsuite'))+[root]:
        for key in ('failures','errors','skipped'):
            if int(suite.get(key,'0')):bad=True
    return 'passed' if cases and not bad else 'failed'
def record_qa(tool,suite,report,started_binding,returncode,definition=None):
    if suite not in SUITES:raise ReleaseError('Unknown native QA suite')
    tool.check()
    if binding(tool)!=started_binding:raise ReleaseError('Release changed during native QA; result is stale')
    definition=definition or SUITES[suite]
    if definition!=SUITES[suite] and not (suite=='https-memory' and definition=='qa/https-transport-memory.json'):raise ReleaseError('Unknown native test definition')
    path=Path(report)/'junit.xml'
    status=junit_result(path) if path.is_file() else 'not run'
    if returncode and status=='passed':status='failed'
    resource=Path(report)/'resource-usage.json'
    if suite.endswith('memory') and status=='passed':
        if not resource.is_file() or resource.is_symlink() or not json.loads(resource.read_text()).get('passed'):status='failed'
    record={'schema':1,'suite':suite,'status':status,'returncode':returncode,**started_binding,
            'definition':definition,'definition_sha256':sha(tool.root/definition)}
    if path.is_file():record['junit_sha256']=sha(path)
    if resource.is_file() and not resource.is_symlink():record['resource_sha256']=sha(resource)
    folder=tool.state/'qa';folder.mkdir(exist_ok=True)
    if path.is_file():
        target=folder/(suite+'.junit.xml')
        if target.is_symlink():raise ReleaseError('Linked JUnit destination')
        target.write_bytes(path.read_bytes())
    if resource.is_file() and not resource.is_symlink():
        target=folder/(suite+'.resource.json')
        if target.is_symlink():raise ReleaseError('Linked resource destination')
        target.write_bytes(resource.read_bytes())
    atomic_json(folder/(suite+'.json'),record)
    return record

def report(tool):
    checks=[];valid=True;record={}
    try:
        tool.check();record=json.loads(tool.record.read_text());current=binding(tool)
        checks.append({'check':'Source, native hashes and archive contents','status':'passed'})
    except (ReleaseError,OSError,ValueError,KeyError):
        valid=False;current={};checks.append({'check':'Source, native hashes and archive contents','status':'failed'})
    for key,label in (('portable','Portable tests'),('native_build','Native executable build'),('sdk','Standalone SDK build')):
        checks.append({'check':label,'status':record.get('checks',{}).get(key,'not run') if valid else 'stale'})
    for suite in (*REQUIRED,*OPTIONAL):
        status='not run';path=tool.state/'qa'/(suite+'.json')
        if path.is_file() and not path.is_symlink():
            try:
                item=json.loads(path.read_text());status=item.get('status','failed');definition=item.get('definition',SUITES[suite])
                if definition!=SUITES[suite] and not (suite=='https-memory' and definition=='qa/https-transport-memory.json'):raise ValueError('Unexpected definition')
                junit=path.with_suffix('.junit.xml')
                if not valid or any(item.get(k)!=v for k,v in current.items()) or item.get('definition_sha256')!=sha(tool.root/definition):status='stale'
                elif status=='passed' and (not junit.is_file() or junit.is_symlink() or sha(junit)!=item.get('junit_sha256') or junit_result(junit)!='passed'):status='failed'
                if status=='passed' and suite.endswith('memory'):
                    resource=path.with_suffix('.resource.json')
                    if not resource.is_file() or resource.is_symlink() or sha(resource)!=item.get('resource_sha256') or not json.loads(resource.read_text()).get('passed'):status='failed'
            except (ValueError,OSError,KeyError,ET.ParseError):status='failed'
        checks.append({'check':suite,'status':status,'required':suite in REQUIRED or status=='failed'})
    ready=all(c['status']=='passed' for c in checks if c.get('required',True))
    result={'schema':1,'version':tool.version,'ready':ready,'checks':checks,**current,
            'notes':['HTTPS memory records whether it used the public transport fixture, a local fixture or an explicitly configured server.',
                     'Free-RAM measurements are system-wide observations, not exact process allocation counts.']}
    tool.state.mkdir(parents=True,exist_ok=True);atomic_json(tool.state/'readiness.json',result)
    text='# AmiHASS '+tool.version+' release readiness\n\n'+('READY' if ready else 'NOT READY')+'\n\n| Check | Result |\n| --- | --- |\n'
    text+=''.join('| '+c['check']+' | '+c['status']+' |\n' for c in checks)
    text+='\n'+'\n'.join('- '+note for note in result['notes'])+'\n'
    target=tool.state/'readiness.md'
    if target.is_symlink():raise ReleaseError('Linked readiness report')
    target.write_text(text)
    return result

def require_ready(tool):
    if not report(tool)['ready']:raise ReleaseError('Release readiness checks are incomplete; inspect release/readiness.md')
