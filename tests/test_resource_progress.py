"""Exercise measured RAM accounting, readback stages and area-scoped sample discovery."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "amihass.h"
#include "sample.h"
#include "memory_stats.h"
#include "action_progress.h"
int main(void){struct AHMemoryStats ram={0};struct AHActionProgress progress;struct AmiHASSEntity value;struct AHControls c;struct AHSample sample;struct AmiHASSMessage m;char out[4096];
 ah_memory_sample(&ram,16000000);ah_memory_begin(&ram,15000000);ah_memory_sample(&ram,11000000);ah_memory_end(&ram,14000000);assert(ram.baseline==16000000&&ram.minimum==11000000&&ram.peak==4000000&&ram.requests==1&&ram.after==14000000);ah_memory_reclaimed(&ram,14000000,15500000);assert(ram.reclaimed==1500000&&ram.flushes==1);ah_memory_begin(&ram,15000000);ah_memory_end(&ram,16000000);assert(ram.peak==4000000&&ram.requests==2);ah_memory_reclaimed(&ram,16000000,15000000);assert(!ram.reclaimed);ah_memory_end(&ram,0);assert(ram.requests==2);
 memset(&value,0,sizeof(value));memset(&c,0,sizeof(c));strcpy(value.state,"off");ah_action_begin(&progress,"Toggle",&value,1,100);assert(!strcmp(ah_action_label(&progress),"Sending"));ah_action_ack(&progress,1,0);assert(!strcmp(ah_action_label(&progress),"Waiting for confirmation"));assert(!ah_action_read(&progress,2,&value,&c));strcpy(value.state,"on");assert(!ah_action_read(&progress,2,&value,&c));assert(ah_action_read(&progress,3,&value,&c));assert(!strcmp(ah_action_label(&progress),"Confirmed"));
 ah_action_begin(&progress,"Turn off",&value,3,0);ah_action_ack(&progress,0,AMIHASS_BUSY);assert(progress.stage==AH_ACTION_FAILED&&progress.error==AMIHASS_BUSY);
 c.brightness=c.level_known=1;c.level=25;ah_action_begin(&progress,"Brightness 50",&value,3,0);ah_action_ack(&progress,1,0);assert(!ah_action_read(&progress,4,&value,&c));c.level=50;c.level_known=0;assert(!ah_action_read(&progress,5,&value,&c));ah_action_tick(&progress,450);assert(progress.stage==AH_ACTION_FAILED&&progress.timed_out);c.level_known=1;assert(!ah_action_read(&progress,6,&value,&c));
 memset(&m,0,sizeof(m));ah_sample_init(&sample);m.kind=AMIHASS_AREAS;ah_sample_request(&sample,&m,100);assert(m.result==AMIHASS_OK&&m.total==4&&m.count==4&&!strcmp(m.items[3].id,"study")&&!strcmp(m.items[3].name,"Study"));m.offset=16;ah_sample_request(&sample,&m,100);assert(m.result==AMIHASS_OK&&m.total==4&&!m.count);
 ah_sample_init(&sample);memset(&m,0,sizeof(m));m.kind=AMIHASS_DEVICES;strcpy(m.domain,"light");strcpy(m.item.id,"Living room");ah_sample_request(&sample,&m,100);assert(m.result==AMIHASS_OK&&m.total==1&&!strcmp(m.items[0].name,"Sample living room"));
 memset(&m,0,sizeof(m));m.kind=AMIHASS_LIST;strcpy(m.domain,"light");strcpy(m.item.id,"study");ah_sample_request(&sample,&m,100);assert(m.total==1&&strstr(m.items[0].id,"desk"));
 memset(&m,0,sizeof(m));m.kind=AMIHASS_LIST;strcpy(m.item.id,"No such room");ah_sample_request(&sample,&m,100);assert(m.result==AMIHASS_OK&&!m.total);
 assert(amihass_devices_area_request("Kitchen's lights","light","cabinet",0,out,sizeof(out)));assert(strstr(out,"area_devices")&&strstr(out,"Kitchen\\\\'s lights")&&strstr(out,"cabinet"));assert(amihass_device_area_page_request("","living_room","light","",16,out,sizeof(out))&&strstr(out,"area_entities")&&strstr(out,"ns.count>16 and ns.count<=32"));assert(!amihass_devices_area_request("bad\nroom","light","",0,out,sizeof(out)));
 puts("Resources/progress: bounded RAM metrics, failed/delayed/unknown readback, action stages and scoped room discovery passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
    p=Path(t);(p/'exec').mkdir();(p/'exec/ports.h').write_text('typedef unsigned long ULONG;typedef long LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'test.c').write_text(program)
    cmd=[os.environ.get('CC','cc'),'-std=c99','-I'+str(p),'-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(p/'test.c'),str(ROOT/'src/core.c'),str(ROOT/'src/sample.c'),str(ROOT/'src/browser_data.c'),'-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
