#!/usr/bin/env python3
"""Run isolated local-fixture recovery/image QA without interrupting other emulators."""
import argparse,json,struct,subprocess,time,zlib
from pathlib import Path
from ws_fixture import endpoint
ROOT=Path(__file__).resolve().parents[1]
APP='/Applications/Amiga Workbench.app/Contents/MacOS/AmigaWorkbench'
def png(width=160,height=96):
    def chunk(kind,data):return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
    rows=b''.join(b'\0'+b''.join(bytes((50+(x//40%2)*100,(y//32)*80,96)) for x in range(width)) for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')
def busy():
    return any(s.endswith(('/fs-uae','/AmigaWorkbench')) for s in subprocess.check_output(['ps','-axo','comm'],text=True).splitlines())
def run(name,qa,output,limit,image=None,fixture=True,workbench="workbench-3.2.1",network=None):
    deadline=time.monotonic()+limit;last_notice=0
    for attempt in range(1,1000):
        while busy():
            if time.monotonic()>deadline:raise RuntimeError('Existing emulator preserved; QA not started')
            if time.monotonic()-last_notice>60:print('Waiting for the existing emulator session.',flush=True);last_notice=time.monotonic()
            time.sleep(2)
        target=output/(name+'-'+str(attempt))
        def launch(folder=None):
            command=[APP,'--workbench',workbench,'--mount','QATests:'+str(ROOT/'build/qa')]
            if network:command+=['--network',network]
            if folder:command+=['--mount','TLSFixture:'+str(folder)]
            command+=['--test',str(ROOT/'qa'/qa),'--output',str(target)]
            return subprocess.run(command,cwd=ROOT).returncode
        counts=None
        if fixture:
            with endpoint(image) as (folder,counts):code=launch(folder)
        else:code=launch()
        report=target/'junit.xml';text=report.read_text() if report.exists() else ''
        if 'Close the existing FS-UAE session' in text:
            if time.monotonic()>deadline:raise RuntimeError('Emulator acquired by another project; existing session preserved')
            time.sleep(2);continue
        if counts is not None:
            (target/'fixture-counts.json').write_text(json.dumps(counts,indent=2)+'\n')
            print(name+': '+json.dumps(counts),flush=True)
        if code:raise RuntimeError(name+' failed; inspect '+str(target))
        if counts is not None:
            assert not counts['actions'] and not counts['errors'],counts
            if image is None:assert len(counts['subscriptions'])>=2,counts
            else:assert counts['camera_images']>=4 and counts['camera_sizes'].get('320x200',0)>=3 and counts['camera_sizes'].get('96x60',0)>=1,counts
        print('Passed '+str(target),flush=True)
        return
    raise RuntimeError('Emulator remained busy')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--wait-seconds',type=int,default=900)
    parser.add_argument('--workbench',default='workbench-3.2.1')
    parser.add_argument('--network',choices=('nat','bridged'))
    parser.add_argument('--checks',default='push,camera,sample,dashboard,diagnostics')
    args=parser.parse_args();selected=args.checks.split(',')
    if any(n not in ('push','camera','sample','dashboard','diagnostics','installer','server','preferences') for n in selected):parser.error('Unknown check')
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    common={'workbench':args.workbench,'network':args.network}
    if 'installer' in selected:run('installer','installer.json',output,args.wait_seconds,fixture=False,**common)
    if 'preferences' in selected:run('preferences','preferences.json',output,args.wait_seconds,fixture=False,**common)
    if 'push' in selected:run('push-reconnect','push-reconnect.json',output,args.wait_seconds,**common)
    if 'camera' in selected:run('png-camera','camera-fixture.json',output,args.wait_seconds,{'320x200':png(),'96x60':png(96,60)},**common)
    if 'sample' in selected:run('sample-reliability','six-reliability.json',output,args.wait_seconds,fixture=False,**common)
    if 'dashboard' in selected:run('dashboard','dashboard-choice.json',output,args.wait_seconds,fixture=False,**common)
    if 'diagnostics' in selected:run('diagnostics','diagnostics.json',output,args.wait_seconds,fixture=False,**common)
    if 'server' in selected:run('server-settings','token-input.json',output,args.wait_seconds,fixture=False,**common)
if __name__=='__main__':main()
