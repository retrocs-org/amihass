"""Locale: generated header and catalogs are current, IFF CTLG layout, Latin-1 text and format-code safety."""
from pathlib import Path
import os,struct,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(root/'tools/make_catalogs.py'),'--check'],check=True)
sys.path.insert(0,str(root/'tools'));import make_catalogs as mc
entries=mc.parse_cd((root/'locale/amihass.cd').read_text())
rows=mc.parse_tsv((root/'locale/texts.tsv').read_text(encoding='utf-8'))
# Every literal in window code is translated or deliberately listed as not.
skip={l for l in (root/'locale/untranslated.txt').read_text().splitlines() if l and not l.startswith('#')}
new=[t for t in mc.ui_strings() if t not in {r[0] for r in rows} and t not in skip and '\t' not in t and '\n' not in t]
assert not new,'untranslated window text: %r (add it to locale/texts.tsv or locale/untranslated.txt)'%new[:5]
for language,sample in (('svenska','AmiHASS-inställningar'),('deutsch','Schließen')):
    data=(root/'Catalogs'/language/'amihass.catalog').read_bytes()
    assert data[:4]==b'FORM' and data[8:12]==b'CTLG' and struct.unpack('>I',data[4:8])[0]==len(data)-8
    p=12;chunks={}
    while p<len(data):
        kind=data[p:p+4];n=struct.unpack('>I',data[p+4:p+8])[0];chunks[kind]=data[p+8:p+8+n];p+=8+n+(n&1)
    assert chunks[b'LANG']==language.encode()+b'\0' and chunks[b'FVER'].startswith(b'$VER: amihass.catalog ')
    strs=chunks[b'STRS'];q=0;found={}
    while q<len(strs):
        ident,n=struct.unpack('>II',strs[q:q+8]);text=strs[q+8:q+8+n];assert text.endswith(b'\0') and b'\0' not in text[:-1];found[ident]=text[:-1].decode('latin-1');q+=8+n+((4-n%4)%4)
    assert set(found)=={i for _,i,_ in entries}|{1000+k for k in range(len(rows))} and sample in found.values()
try:
    mc.catalog(entries,{'version':'$VER: x 1.0','language':'x'},{'MSG_ATTR_COUNT':'%s attribut'},'x');raise SystemExit('format change not refused')
except ValueError:pass
program=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ah_locale.h"
int main(void){const char *l[3];static const long ids[2]={MSG_NEVER,MSG_CLOSE};static const char *const t[2]={MSG_NEVER_STR,MSG_CLOSE_STR};
 assert(ah_same_format("%d notifications","%d aviseringar")&&ah_same_format("%ld x %s","a %ld b %s")&&!ah_same_format("%ld x","%s x")&&!ah_same_format("%d","%d %d")&&ah_same_format("100%% %d","%d 100%%")==0&&ah_same_format("a %% b","c %% d"));
 ah_tr_list(l,ids,t,2);assert(l[0]==t[0]&&l[2]==NULL);
 {const char *src[3]={"Close","not a known text",NULL};const char **out=ah_tx_labels(src);assert(ah_tx("Close")&&!strcmp(ah_tx("Close"),"Close")&&!strcmp(ah_tx("zz unknown"),"zz unknown")&&out[0]&&!strcmp(out[1],"not a known text")&&!out[2]&&ah_tx(NULL)==NULL);}assert(AH_TR(MSG_CLOSE)==MSG_CLOSE_STR);puts("Catalogs: every window text translated or listed, current, valid IFF CTLG, complete Swedish and German, format codes guarded passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'t.c').write_text(program)
    cmd=[os.environ.get('CC','cc'),'-std=c99','-DAH_HOST_TEST','-I'+str(root/'include'),str(p/'t.c'),str(root/'src/ah_locale.c'),'-o',str(p/'t')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'t')],check=True)
