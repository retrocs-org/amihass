#!/usr/bin/env python3
"""Run an isolated Workbench suite against verified release bytes and retain its result."""
import argparse,contextlib,json,os,shlex,shutil,subprocess,sys,tempfile,uuid
from pathlib import Path
from release import Release,config_read
from release_support import ReleaseError,atomic_json
from memory_report import measurements
from release_readiness import SUITES,binding,record_qa,report
ROOT=Path(__file__).resolve().parents[1]
APP=Path('/Applications/Amiga Workbench.app/Contents/MacOS/AmigaWorkbench')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',type=Path,default=Path(os.environ.get('AMIHASS_RELEASE_CONFIG',str(Path.home()/'Development/tools/git-release/projects/amihass.json'))))
    parser.add_argument('--output',type=Path,default=Path('outputs/release-0.2'))
    parser.add_argument('--suite',choices=SUITES,required=True)
    parser.add_argument('--fixture',action='store_true',help='Use a public reserved-domain TLS transport fixture; HTTPS-memory only')
    parser.add_argument('--local-fixture',action='store_true',help='Use a disposable local TLS server when guest-to-host routing is available')
    parser.add_argument('--credentials',type=Path,help='Private file: server URL then token; HTTPS-memory only')
    args=parser.parse_args();tool=Release(ROOT,config_read(ROOT,args.config),output=args.output)
    tool.check();started=binding(tool)
    if not APP.is_file():raise ReleaseError('Amiga Workbench.app is not installed')
    processes=subprocess.run(['ps','-axww','-o','comm='],capture_output=True,text=True)
    if processes.returncode:raise ReleaseError('Cannot check emulator ownership; close existing sessions before QA')
    if any(Path(line.strip()).name.lower()=='fs-uae' for line in processes.stdout.splitlines()):raise ReleaseError('An emulator session is already running; it was left untouched')
    is_fixture=args.fixture or args.local_fixture
    if args.fixture and args.local_fixture:raise ReleaseError('Choose one TLS fixture')
    if is_fixture and (args.suite!='https-memory' or args.credentials):raise ReleaseError('--fixture is HTTPS-memory only and cannot use saved credentials')
    if args.suite=='https-memory' and not is_fixture and (not args.credentials or not args.credentials.is_file()):raise ReleaseError('HTTPS memory QA needs --credentials pointing to a private URL/token fixture')
    if args.credentials and args.suite!='https-memory':raise ReleaseError('Credentials apply only to HTTPS memory QA')
    env=dict(os.environ,**tool.config['env']);vc=env.get('AMIHASS_VC','vc +aos68k')
    subprocess.run(['make','qa','VC='+vc],cwd=ROOT,env=env,check=True)
    key=args.suite+'-'+uuid.uuid4().hex[:10]
    payload=ROOT/'build'/('qa-release-'+key)
    subprocess.run([sys.executable,str(ROOT/'tools/prepare_release_qa.py'),str(tool.out/'AmiHASS-0.2.lha'),str(payload)],cwd=ROOT,env=env,check=True)
    result=ROOT/'build/qa-results'/key
    command=[str(APP),'--workbench','workbench-3.2.1','--mount','QATests:'+str(payload)]
    with contextlib.ExitStack() as stack:
        fixture_public=None;credentials=args.credentials
        if is_fixture:
            from https_fixture import endpoint,transport_endpoint
            credentials,fixture_public=stack.enter_context(transport_endpoint() if args.fixture else endpoint())
        directory=stack.enter_context(tempfile.TemporaryDirectory(prefix='amihass-private-qa-'))
        if credentials:
            private=Path(directory);target=private/'credentials';shutil.copyfile(credentials,target);target.chmod(0o600)
            command+=['--mount','LiveCredentials:'+str(private)]
        if fixture_public:command+=['--mount','TLSFixture:'+str(fixture_public)]
        definition='qa/https-transport-memory.json' if args.fixture else SUITES[args.suite]
        command+=['--test',str(ROOT/definition),'--output',str(result)]
        run=subprocess.run(command,cwd=ROOT,env=env)
    logs=list((result/'profile').glob('Installations/*/CPUs/*/AutoRun/launch.log'))
    if logs:
        text=logs[0].read_text(errors='replace');(result/'native-output.log').write_text(text)
        if args.suite.endswith('memory'):
            try:
                measured=measurements(text,args.suite);measured['endpoint']='public TLS transport fixture (expected HTTP 404)' if args.fixture else 'disposable local TLS fixture' if args.local_fixture else 'configured server' if args.credentials else 'shared sample service'
                atomic_json(result/'resource-usage.json',measured)
            except ReleaseError:pass
    if (args.credentials or is_fixture) and (result/'profile').is_dir():shutil.rmtree(result/'profile')
    record_qa(tool,args.suite,result,started,run.returncode,definition=definition)
    state=report(tool)
    print('Readiness: '+str(tool.state/'readiness.md'))
    return run.returncode
if __name__=='__main__':
    try:raise SystemExit(main())
    except (ReleaseError,subprocess.CalledProcessError) as error:raise SystemExit(str(error))
