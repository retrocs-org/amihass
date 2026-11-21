from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
code=r'''#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "sample.h"
int main(void){struct AHSample sample;struct AmiHASSMessage m;struct AHControls c;const char *ids[]={"fan.amihass_sample_bedroom","cover.amihass_sample_garage"};const char *actions[]={"Fan speed 65","Position 70"};int i;ah_sample_init(&sample);for(i=0;i<2;++i){memset(&m,0,sizeof(m));m.kind=AMIHASS_ACTION;strcpy(m.owner,"SDK.Test");strcpy(m.entity,ids[i]);strcpy(m.action,actions[i]);ah_sample_request(&sample,&m,10);assert(m.result==AMIHASS_WAIT);m.kind=AMIHASS_QUERY;ah_sample_request(&sample,&m,11);amihass_controls_items(m.items,m.count,&c);assert(m.result==AMIHASS_OK&&amihass_control_matches(&c,m.item.state,actions[i]));}strcpy(m.action,"Position 101");m.kind=AMIHASS_ACTION;ah_sample_request(&sample,&m,12);assert(m.result==AMIHASS_UNSUPPORTED);
{char path[96],body[180];struct AmiHASSEntity items[16];struct AHControls copy;unsigned int count;
 amihass_controls("{\"entity_id\":\"lock.front\",\"state\":\"locked\",\"attributes\":{\"supported_features\":1,\"code_format\":null}}",&c);
 assert(c.lock&&c.unlock&&c.lock_open);count=amihass_control_items(&c,items,16);assert(count==3);amihass_controls_items(items,count,&copy);assert(copy.lock&&copy.unlock&&copy.lock_open);
 assert(amihass_action("lock.front","Unlock",path,body)&&!strcmp(path,"/api/services/lock/unlock")&&!strcmp(body,"{\"entity_id\":\"lock.front\"}"));
 assert(amihass_action("lock.front","Lock",path,body)&&!strcmp(path,"/api/services/lock/lock"));assert(amihass_action("lock.front","Open lock",path,body)&&!strcmp(path,"/api/services/lock/open"));
 assert(!amihass_action("lock.front","Toggle",path,body));assert(!amihass_action("binary_sensor.front_door","Unlock",path,body));assert(!amihass_action("lock.front","Unlock 1234",path,body));
 assert(amihass_control_matches(&c,"locked","Lock")&&!amihass_control_matches(&c,"locking","Lock"));assert(!amihass_control_matches(&c,"jammed","Unlock"));assert(!amihass_control_matches(&c,"unlocked","Open lock"));
 amihass_controls("{\"entity_id\":\"lock.front\",\"state\":\"locked\",\"attributes\":{\"supported_features\":0}}",&c);assert(c.lock&&c.unlock&&!c.lock_open&&!amihass_control_allowed(&c,"Open lock"));
 amihass_controls("{\"entity_id\":\"lock.front\",\"state\":\"locked\",\"attributes\":{\"code_format\":\"[0-9]+\"}}",&c);assert(!c.lock&&!c.unlock&&!c.lock_open&&!amihass_control_allowed(&c,"Unlock"));
}
{const char *commands[]={"Unlock","Lock","Open lock"};const char *states[]={"unlocked","locked","open"};for(i=0;i<3;++i){memset(&m,0,sizeof(m));m.kind=AMIHASS_ACTION;strcpy(m.owner,"LockClient1");strcpy(m.entity,"lock.amihass_sample_front_door");strcpy(m.action,commands[i]);ah_sample_request(&sample,&m,20+i);assert(m.result==AMIHASS_WAIT);m.kind=AMIHASS_QUERY;strcpy(m.owner,"LockClient2");ah_sample_request(&sample,&m,21+i);amihass_controls_items(m.items,m.count,&c);assert(m.result==AMIHASS_OK&&!strcmp(m.item.state,states[i])&&amihass_control_matches(&c,m.item.state,commands[i]));}}
{const char *commands[]={"Open cover","Stop cover","Close cover"};for(i=0;i<3;++i){memset(&m,0,sizeof(m));m.kind=AMIHASS_ACTION;strcpy(m.owner,"DoorClient1");strcpy(m.entity,"cover.amihass_sample_garage");strcpy(m.action,commands[i]);ah_sample_request(&sample,&m,30+i);assert(m.result==AMIHASS_WAIT);m.kind=AMIHASS_QUERY;strcpy(m.owner,"DoorClient2");ah_sample_request(&sample,&m,31+i);amihass_controls_items(m.items,m.count,&c);assert(amihass_control_matches(&c,m.item.state,commands[i]));}}
puts("Shared sample fan speed and cover position readback passed.");return 0;}'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'exec').mkdir();(p/'exec/ports.h').write_text('typedef unsigned long ULONG;typedef long LONG;struct MsgPort;struct Message{unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'check.c').write_text(code)
 cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(r/'SDK'),'-I'+str(r/'include'),str(p/'check.c'),str(r/'src/core.c'),str(r/'src/sample.c'),str(r/'src/browser_data.c'),'-o',str(p/'check')]
 if os.environ.get('SANITIZE')=='1':cmd+=['-g','-O1','-fsanitize=address,undefined']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'check')],check=True)
