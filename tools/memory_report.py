"""Read only numeric memory lines from native QA; never retain connection details."""
import re
from release_support import ReleaseError

def measurements(text,suite):
    result={'schema':1,'suite':suite,'units':'bytes','scope':'System-wide Fast RAM; sampled values, not process allocations'}
    if suite=='sample-memory':
        warm=re.search(r'MEMORY sample warm: (\d+) Fast bytes free',text)
        end=re.search(r'MEMORY sample ending: (\d+); retained change (-?\d+) bytes',text)
        if not warm or not end:raise ReleaseError('Missing native sample memory measurements')
        result.update(before=int(warm[1]),after=int(end[1]),retained=int(warm[1])-int(end[1]),limit=65536)
        result['passed']=result['retained']<=result['limit']
    elif suite=='https-memory':
        pattern=r'MEMORY HTTPS cycle (\d+): before (\d+), minimum (\d+), after request (\d+), after reclaim (\d+) bytes'
        cycles=[]
        for values in re.findall(pattern,text):
            number,before,minimum,after,reclaimed=map(int,values)
            if minimum>before:raise ReleaseError('Invalid native memory minimum')
            cycles.append({'cycle':number,'before':before,'minimum':minimum,'after_request':after,'after_reclaim':reclaimed,
                           'observed_drop':before-minimum,'reclaimed':max(0,reclaimed-after)})
        if [c['cycle'] for c in cycles]!=list(range(1,9)):raise ReleaseError('Eight ordered HTTPS memory cycles are required')
        result.update(cycles=cycles,observed_peak=max(c['observed_drop'] for c in cycles),
                      retained=cycles[1]['after_reclaim']-cycles[-1]['after_reclaim'],limit=131072)
        result['passed']=result['retained']<=result['limit']
    else:raise ReleaseError('Unknown resource suite')
    return result
