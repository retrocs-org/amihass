from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
code=r'''#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "sample.h"
int main(void){struct AHSample sample;struct AmiHASSMessage m;struct AHControls c;const char *ids[]={"fan.amihass_sample_bedroom","cover.amihass_sample_garage"};const char *actions[]={"Fan speed 65","Position 70"};int i;ah_sample_init(&sample);for(i=0;i<2;++i){memset(&m,0,sizeof(m));m.kind=AMIHASS_ACTION;strcpy(m.owner,"SDK.Test");strcpy(m.entity,ids[i]);strcpy(m.action,actions[i]);ah_sample_request(&sample,&m,10);assert(m.result==AMIHASS_WAIT);m.kind=AMIHASS_QUERY;ah_sample_request(&sample,&m,11);amihass_controls_items(m.items,m.count,&c);assert(m.result==AMIHASS_OK&&amihass_control_matches(&c,m.item.state,actions[i]));}strcpy(m.action,"Position 101");m.kind=AMIHASS_ACTION;ah_sample_request(&sample,&m,12);assert(m.result==AMIHASS_UNSUPPORTED);puts("Shared sample fan speed and cover position readback passed.");return 0;}'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'exec').mkdir();(p/'exec/ports.h').write_text('typedef unsigned long ULONG;typedef long LONG;struct MsgPort;struct Message{unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'check.c').write_text(code)
 cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(r/'SDK'),'-I'+str(r/'include'),str(p/'check.c'),str(r/'src/core.c'),str(r/'src/sample.c'),str(r/'src/browser_data.c'),'-o',str(p/'check')]
 if os.environ.get('SANITIZE')=='1':cmd+=['-g','-O1','-fsanitize=address,undefined']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'check')],check=True)
