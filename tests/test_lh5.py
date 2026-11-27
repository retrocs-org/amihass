"""Built-in LHA -lh5- compression: round trips, the release writer/verifier, and an independent decoder when available."""
from pathlib import Path
import random,sys,tempfile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
import lh5
from release_support import lha_write,verify_lha,ReleaseError
random.seed(7)
cases={'empty':b'','one':b'A','zeros':b'\0'*70000,'noise':bytes(random.getrandbits(8) for _ in range(20000)),'text':(root/'src/core.c').read_bytes(),
       'repeat':b'AmiHASS '*5000,'long-run':b'x'*300+b'y'*300,'binary':(root/'Catalogs/deutsch/amihass.catalog').read_bytes()}
for name,data in cases.items():
    assert lh5.decompress(lh5.compress(data),len(data))==data,name
with tempfile.TemporaryDirectory() as d:
    path=Path(d)/'t.lha';entries={'AmiHASS/'+k:v for k,v in cases.items()}
    lha_write(path,entries);verify_lha(path,entries)
    raw=path.read_bytes();assert raw.count(b'-lh5-')>=4 and len(raw)<sum(len(v) for v in cases.values())//2
    try:
        import lhafile  # independent decoder (C), when installed
        f=lhafile.LhaFile(str(path))
        for k,v in entries.items():assert f.read(k)==v,k
        independent='checked with lhafile'
    except ImportError:independent='lhafile not installed'
    bad=bytearray(raw);bad[len(bad)//2]^=0x55;(Path(d)/'bad.lha').write_bytes(bytes(bad))
    try:verify_lha(Path(d)/'bad.lha',entries);raise SystemExit('damaged archive accepted')
    except ReleaseError:pass
print('LHA -lh5-: round trips, release writer and verifier, damaged data refused; '+independent+'.')
