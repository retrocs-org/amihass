"""Exercise the offline provider independently of AmigaOS and the bridge."""
from pathlib import Path
import os,subprocess,tempfile,json
ROOT=Path(__file__).resolve().parents[1]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "browser_data.h"
int main(void){unsigned int ids[AH_BROWSER_MAX],sentinel[2]={999,888},n,i;char domain[96];const struct AHBrowserEntity *e;
 n=ah_browser_filter(NULL,NULL,ids,AH_BROWSER_MAX);assert(n==ah_browser_count()&&n==22);
 assert(!ah_browser_entity(n));assert(!ah_browser_domain(NULL,domain,sizeof(domain)));
 assert(ah_browser_filter("light","",ids,AH_BROWSER_MAX)==2);
 assert(ah_browser_filter("light","DESK",ids,AH_BROWSER_MAX)==1);assert(!strcmp(ah_browser_entity(ids[0])->id,"light.desk"));
 assert(ah_browser_filter("","TEMPERATURE",ids,AH_BROWSER_MAX)==1);assert(!strcmp(ah_browser_entity(ids[0])->state,"21.5"));
 assert(ah_browser_filter("","unavailable",ids,AH_BROWSER_MAX)==1);
 assert(ah_browser_filter("sensor","no-match",ids,AH_BROWSER_MAX)==0);
 assert(ah_browser_filter("retro_bridge","",ids,AH_BROWSER_MAX)==1);assert(ah_browser_domain(ah_browser_entity(ids[0]),domain,sizeof(domain))&&!strcmp(domain,"retro_bridge"));
 assert(!ah_browser_domain(ah_browser_entity(ids[0]),domain,2));
 assert(ah_browser_filter("","",sentinel,1)==22 && sentinel[0]==0 && sentinel[1]==888);
 assert(ah_browser_filter("","",NULL,0)==22);
 for(i=0;i<n;++i){e=ah_browser_entity(i);assert(e&&strlen(e->attributes)<=350);puts(e->attributes);}
 return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(program)
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(ROOT/'include'),str(p/'test.c'),str(ROOT/'src/browser_data.c'),'-o',str(p/'test')],check=True)
 result=subprocess.run([str(p/'test')],capture_output=True,text=True,check=True)
 attrs=[json.loads(line) for line in result.stdout.splitlines()]
 assert any(isinstance(v,list) for a in attrs for v in a.values())
 assert any(isinstance(v,dict) for a in attrs for v in a.values())
 assert any(v is None for a in attrs for v in a.values())
 assert any(isinstance(v,bool) for a in attrs for v in a.values())
print('Browser fixtures: all domains, custom entities, typed attributes, search, empty results and bounded output passed.')
