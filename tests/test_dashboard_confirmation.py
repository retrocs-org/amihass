"""Exercise production post-consent checks with query-only fixture replies."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'src/dashboard.c').read_text();a=s.index('static int revalidate_device_action(');b=s.index('static int send_action(',a)
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "dashboard.h"
#include "nd_panel.h"
#define AMIHASS_STATUS 1
#define AMIHASS_QUERY 2
#define AMIHASS_OK 1
#define AMIHASS_WAIT 2
#define AMIHASS_BUSY 6
#define AMIHASS_QUERY_ONCE 1
#define AMIHASS_QUERY_FRESH 2
#define AMIHASS_SERVER_CONNECTED "connected"
struct AmiHASSClient{void *pending;};struct AmiHASSMessage{int kind,result;unsigned int count,age;struct AmiHASSEntity item,items[16];struct{int connected;char server[160];}status;};
static int ah_keypad(const char *t,char *c,unsigned int n){(void)t;(void)c;(void)n;return 0;}
struct Dash{struct AHDashboardLayout layout;struct AmiHASSEntity values[1];struct AHControls controls[1];char server[160];int selected,results[1];time_t received[1];unsigned long read_serial[1];};struct NDPanel{int x;};static struct NDPanel panel;static struct AmiHASSMessage reply;static int mode,kind,sends,statuses,queries,closes,events,refresh;
static int amihass_send(struct AmiHASSClient *c,int k,const char *owner,const char *entity,const char *action,int offset,int flags,const char *domain,const char *filter){(void)owner;(void)offset;(void)domain;(void)filter;assert(!action&&(k==AMIHASS_STATUS||k==AMIHASS_QUERY));kind=k;refresh=flags;++sends;c->pending=&reply;if(k==AMIHASS_STATUS){assert(!entity&&!flags);++statuses;}else{assert(!strcmp(entity,"lock.fixture"));++queries;assert(flags==(queries==1 ? 3:1));}return 1;}
static struct AmiHASSMessage *amihass_reply(struct AmiHASSClient *c){if(mode==9&&kind==AMIHASS_QUERY)return NULL;c->pending=NULL;memset(&reply,0,sizeof(reply));reply.kind=mode==12 ? 77:kind;reply.result=AMIHASS_OK;if(kind==AMIHASS_STATUS){reply.status.connected=mode!=2;strcpy(reply.status.server,mode==4||(mode==11&&statuses==2) ? "https://different.invalid":"https://fixture.local");strcpy(reply.item.state,mode==3||(mode==10&&statuses==2) ? "sample":"connected");}else{if(queries==1){reply.result=mode==14 ? AMIHASS_BUSY:AMIHASS_WAIT;return &reply;}reply.result=mode==8 ? 10:AMIHASS_OK;strcpy(reply.item.id,mode==17 ? "lock.other":"lock.fixture");strcpy(reply.item.state,mode==5 ? "unlocked":mode==6 ? "unavailable":"locked");reply.age=mode==13 ? 10:0;reply.count=mode==7 ? 0:1;strcpy(reply.items[0].id,"control.unlock");strcpy(reply.items[0].state,"yes");}return &reply;}
static void amihass_client_close(struct AmiHASSClient *c){c->pending=NULL;++closes;}static void FreeVec(void *p){(void)p;}static void Delay(int n){assert(n==5);}
/* Production loops wait on signals; in tests this is one scripted Delay step. */
void nd_panel_wait(struct NDPanel *p,unsigned long e,unsigned long t){(void)p;(void)e;(void)t;Delay(5);}
unsigned long amihass_client_signal(const struct AmiHASSClient *c){(void)c;return 0;}

struct NDPanel *nd_panel_open(const char *t,const struct NDPanelField *f,unsigned int n,const char **buttons,unsigned int count){(void)t;(void)f;assert(n==2&&count==1&&!strcmp(buttons[0],"Cancel"));return &panel;}int nd_panel_event(struct NDPanel *p){(void)p;++events;return mode==1||(mode==16&&events==3) ? 100:0;}void nd_panel_close(struct NDPanel *p){assert(p==&panel);}
'''+s[a:b]+r'''
int main(void){struct Dash d;for(mode=0;mode<=17;++mode){if(mode==15)continue;memset(&d,0,sizeof(d));strcpy(d.server,"https://fixture.local");strcpy(d.layout.tiles[0].entity,"lock.fixture");strcpy(d.layout.tiles[0].label,"Fixture door");strcpy(d.values[0].id,"lock.fixture");strcpy(d.values[0].state,"locked");statuses=queries=sends=closes=events=0;if(mode==0||mode==14){assert(revalidate_device_action(&d,"Unlock"));assert(statuses==2&&queries==2&&d.read_serial[0]==1&&d.controls[0].unlock);}else{assert(!revalidate_device_action(&d,"Unlock"));assert(d.read_serial[0]==0);if(mode==1)assert(sends==0);if(mode==9)assert(queries==1&&events==300);}assert(closes==1);}puts("Door confirmation: fresh query, provider/server changes, disconnected/stale/changed state, changed capabilities, malformed replies, cancellation and timeout passed; query-only fixtures.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);cmd=[os.environ.get('CC','cc'),'-std=c99','-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(p/'test.c'),str(ROOT/'src/core.c'),'-o',str(p/'test')]
 if os.environ.get('SANITIZE')=='1':cmd+=['-g','-O1','-fsanitize=address,undefined']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
