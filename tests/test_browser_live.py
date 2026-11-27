"""Exercise the production live Browser provider with a scripted SDK service."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
stubs=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "browser_live.h"
static int stopped,remote_sample,remote_connected=1,list_wait=1,query_wait=1,list_fail,sends,closed,queries;
static char last_filter[64],last_domain[24];static int last_offset;
void FreeVec(void *p){free(p);}
const char *amihass_problem(int result){return result==AMIHASS_FAILED ? "Server request failed":"Bridge error";}
void amihass_client_close(struct AmiHASSClient *c){++closed;if(c->pending)free(c->pending);memset(c,0,sizeof(*c));}
int amihass_send(struct AmiHASSClient *c,int kind,const char *owner,const char *entity,const char *action,int offset,int refresh,const char *domain,const char *filter){struct AmiHASSMessage *m;int i;(void)owner;(void)refresh;assert(kind==AMIHASS_STATUS||kind==AMIHASS_LIST||kind==AMIHASS_DEVICES||kind==AMIHASS_QUERY);assert(!action);++sends;if(stopped)return 0;assert(!c->pending);m=calloc(1,sizeof(*m));m->kind=kind;m->result=AMIHASS_OK;
 if(kind==AMIHASS_STATUS){m->status.connected=remote_connected;strcpy(m->status.server,"https://fixture.invalid");strcpy(m->item.state,remote_sample ? AMIHASS_SERVER_SAMPLE:remote_connected ? AMIHASS_SERVER_CONNECTED:AMIHASS_SERVER_RETRY_WAIT);}
 if(kind==AMIHASS_LIST||kind==AMIHASS_DEVICES){strcpy(last_filter,filter);strcpy(last_domain,domain);last_offset=offset;m->offset=offset;if(list_fail){m->result=AMIHASS_FAILED;if(list_fail==2)strcpy(m->item.name,"Discovery failed - Refresh or see server logs");}else if(list_wait){--list_wait;m->result=AMIHASS_WAIT;}else {m->total=!strcmp(filter,"empty") ? 0:35;m->count=m->total-offset;if(m->count>16)m->count=16;if(m->count<0)m->count=0;for(i=0;i<m->count;++i){if(kind==AMIHASS_DEVICES)sprintf(m->items[i].id,"%032x",offset+i+1);else sprintf(m->items[i].id,"sensor.sample_%d",offset+i);sprintf(m->items[i].name,"Entity %d",offset+i);strcpy(m->items[i].state,"10");}}}
 if(kind==AMIHASS_QUERY){assert(refresh==AMIHASS_QUERY_ONCE);++queries;if(query_wait){--query_wait;m->result=AMIHASS_WAIT;}else{strcpy(m->item.id,entity);strcpy(m->item.state,"21");strcpy(m->item.unit,"C");m->age=2;}}
 c->pending=m;return 1;}
int amihass_send_scope(struct AmiHASSClient *c,int kind,const char *owner,const char *entity,int offset,int refresh,const char *domain,const char *filter,const char *area,const char *label){assert(area&&label);return amihass_send(c,kind,owner,entity,NULL,offset,refresh,domain,filter);}
struct AmiHASSMessage *amihass_reply(struct AmiHASSClient *c){struct AmiHASSMessage *m=c->pending;c->pending=NULL;return m;}
static void run(struct AHBrowserLive *p,int ticks){int i;for(i=0;i<ticks;++i)ah_browser_live_step(p);}
int main(void){struct AHBrowserLive p;int before;ah_browser_live_init(&p);assert(ah_browser_live_page(&p,"","",0,1));run(&p,40);assert(p.connected&&p.count==16&&p.total==35&&!p.loading);
 assert(ah_browser_live_page(&p,p.domain,p.filter,16,0));run(&p,20);assert(p.offset==16&&p.count==16&&last_offset==16);
 assert(ah_browser_live_page(&p,"my_type2","find-me",32,0));run(&p,20);assert(!strcmp(last_domain,"my_type2")&&!strcmp(last_filter,"find-me")&&p.count==3);
 ah_browser_live_details(&p,1);run(&p,30);assert(!p.detail_loading&&p.detail_result==AMIHASS_OK&&!strcmp(p.detail.id,"sensor.sample_33")&&!strcmp(p.detail.state,"21")&&p.age==2&&queries>=2);ah_browser_live_cancel_details(&p);
 assert(ah_browser_live_page(&p,"","empty",0,0));run(&p,20);assert(!p.count&&!p.total&&!p.wanted);
 assert(!ah_browser_live_page(&p,"bad;domain","",0,0));assert(!ah_browser_live_page(&p,"sensor","",-1,0));
 assert(ah_browser_live_page(&p,"","old",0,0));run(&p,1);before=closed;assert(p.client.pending);assert(ah_browser_live_page(&p,"","new",0,0));assert(closed>before);run(&p,20);assert(!strcmp(last_filter,"new"));
 remote_connected=0;run(&p,70);assert(!p.connected&&strstr(p.error,"retry"));remote_connected=1;assert(ah_browser_live_page(&p,"","",0,0));run(&p,20);assert(p.connected&&p.count==16);
 stopped=1;run(&p,60);assert(!p.running&&!p.connected&&strstr(p.error,"stopped"));stopped=0;assert(ah_browser_live_page(&p,"","",0,0));run(&p,20);assert(p.running&&p.count==16);
 assert(ah_browser_live_devices(&p,"device",0,0));run(&p,20);assert(p.devices&&amihass_device_id(p.items[0].id));assert(ah_browser_live_device_page(&p,p.items[0].id,"sensor","",0,0));run(&p,20);assert(!p.devices&&p.device[0]&&p.count==16);assert(!ah_browser_live_device_page(&p,"bad-id","","",0,0));
 remote_sample=1;run(&p,80);assert(p.sample_mode&&p.devices&&amihass_device_id(p.items[0].id));remote_sample=0;run(&p,80);assert(!p.sample_mode&&p.devices);
 assert(ah_browser_live_devices_type(&p,"light","",0,0));run(&p,40);assert(!strcmp(last_domain,"light")&&!strcmp(p.domain,"light"));
 p.page_size=27;assert(ah_browser_live_devices(&p,"",0,0));run(&p,40);assert(p.count==27&&p.offset==0&&last_offset==16);
 assert(ah_browser_live_devices(&p,"",27,0));run(&p,40);assert(p.count==8&&p.offset==27&&last_offset==27);
 p.page_size=16;
 list_fail=1;assert(ah_browser_live_page(&p,"","",0,0));run(&p,80);assert(strstr(p.error,"failed"));
 list_fail=2;assert(ah_browser_live_devices(&p,"",0,1));run(&p,80);assert(p.connected&&p.terminal_error&&!p.wanted&&!p.loading&&!strcmp(p.error,"Discovery failed - Refresh or see server logs"));list_fail=0;assert(ah_browser_live_devices(&p,"",0,1));run(&p,40);assert(p.count==16&&!p.terminal_error&&!p.error[0]);
 ah_browser_live_close(&p);assert(!p.client.pending);puts("Live Browser: paging, generic types, search, state details, cancellation, offline status and read-only requests passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'exec').mkdir();(p/'proto').mkdir()
 (p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n')
 (p/'proto/exec.h').write_text('void FreeVec(void *);\n')
 (p/'test.c').write_text(stubs)
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(p),'-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(p/'test.c'),str(ROOT/'src/browser_live.c'),str(ROOT/'src/core.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
