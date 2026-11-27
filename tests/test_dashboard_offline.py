"""Exercise production Dashboard reply handling across failures and source changes."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static void step(');b=s.index('static void scroll_rows(',a)
code=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "dashboard.h"
#include "action_progress.h"
#define AMIHASS_SUBSCRIBE 10
#define AMIHASS_STATUS 1
#define AMIHASS_QUERY 2
#define AMIHASS_ACTION 3
#define AMIHASS_OK 1
#define AMIHASS_WAIT 2
#ifndef AMIHASS_BUSY
#define AMIHASS_BUSY 6
#endif
#define AMIHASS_STATE_SELECTION_FULL "selection_full"
#define AMIHASS_STALE 7
#define AMIHASS_QUERY_ONCE 1
#define AMIHASS_QUERY_FRESH 2
#define AMIHASS_QUERY_INTERVAL(s) ((int)(s)<<8)
#define AMIHASS_SERVER_SAMPLE "sample"
#define AMIHASS_SERVER_AUTH_REJECTED "auth_rejected"
struct AmiHASSClient {void *pending;};
struct AmiHASSMessage {int result;struct {int connected;char server[160];}status;struct AmiHASSEntity item,items[16];unsigned int count;unsigned long age;};
static unsigned int page_poll;
#define TILE_REFRESH(d,i) ((d)->layout.tiles[(i)].refresh ? (d)->layout.tiles[(i)].refresh:page_poll)
struct Dash {void *thumbs,*screen;struct AHActionProgress progress[24];int in_controls,inactive;struct AHDashboardLayout layout;struct AmiHASSClient client;struct AmiHASSEntity values[24];struct AHControls controls[24];unsigned long camera_attempt[24],read_serial[24];time_t received[24],due[24];int fresh,results[24],selected,round,sent,index,ready,sample,force,queued,action_failed;unsigned int cells,row_offset,default_poll;unsigned long ticks,next,status_due;char status[160],queued_action[20],server[160];};
static struct AmiHASSMessage packet,*incoming;static int can_send,sends,last_refresh;
static void ah_thumbs_limit(void *t,int low){(void)t;(void)low;}static void ah_thumbs_free(void *t){(void)t;}static void *ah_thumbs_new(void *s){(void)s;return NULL;}
static void update(struct Dash *d){(void)d;}
static int send_action(struct Dash *d,const char *s){(void)d;(void)s;return 0;}
static const char *amihass_problem(int r){(void)r;return "Failed";}
static void FreeVec(void *p){assert(p==&packet);}
static struct AmiHASSMessage *amihass_reply(struct AmiHASSClient *c){struct AmiHASSMessage *m=incoming;(void)c;incoming=NULL;return m;}
static int amihass_send(struct AmiHASSClient *c,int kind,const char *owner,const char *id,const char *action,int offset,int refresh,const char *domain,const char *filter){(void)c;(void)kind;(void)owner;(void)id;(void)action;(void)offset;(void)refresh;(void)domain;(void)filter;++sends;last_refresh=refresh;return can_send;}
'''+s[a:b]+r'''
int main(void){struct Dash d;time_t received;memset(&d,0,sizeof(d));d.queued=d.force=-1;d.cells=1;d.ready=1;d.sent=AMIHASS_QUERY;packet.result=AMIHASS_OK;strcpy(packet.item.state,"on");strcpy(packet.items[0].id,"control.brightness");strcpy(packet.items[0].state,"40");packet.count=1;incoming=&packet;step(&d);assert(!strcmp(d.values[0].state,"on")&&d.controls[0].level==40&&d.received[0]);received=d.received[0];packet.result=AMIHASS_STALE;packet.item.state[0]=0;d.sent=AMIHASS_QUERY;incoming=&packet;step(&d);assert(!strcmp(d.values[0].state,"on")&&d.controls[0].level==40&&d.received[0]==received&&d.read_serial[0]==1);
 d.sent=AMIHASS_STATUS;packet.status.connected=0;incoming=&packet;step(&d);assert(!d.ready&&d.results[0]==AMIHASS_STALE&&!strcmp(d.values[0].state,"on"));
 d.ready=1;d.results[0]=AMIHASS_OK;d.sent=AMIHASS_QUERY;incoming=NULL;step(&d);assert(!d.ready&&d.results[0]==AMIHASS_STALE&&d.received[0]==received);
 d.sent=AMIHASS_STATUS;packet.status.connected=1;strcpy(packet.item.state,AMIHASS_SERVER_SAMPLE);incoming=&packet;step(&d);assert(d.sample&&d.ready&&!d.results[0]&&!d.received[0]&&!d.values[0].state[0]&&!d.controls[0].brightness);
 memset(&d,0,sizeof(d));d.force=d.queued=-1;d.cells=2;d.ready=1;d.status_due=500;strcpy(d.layout.tiles[0].entity,"light.shared");strcpy(d.layout.tiles[1].entity,"light.shared");d.layout.tiles[0].refresh=5;d.layout.tiles[1].refresh=300;d.sent=AMIHASS_QUERY;d.index=0;memset(&packet,0,sizeof(packet));packet.result=AMIHASS_OK;packet.age=10;strcpy(packet.item.id,"light.shared");strcpy(packet.item.state,"on");incoming=&packet;step(&d);assert(!strcmp(d.values[1].state,"on")&&d.read_serial[1]==1&&d.due[1]>d.due[0]+50&&time(NULL)-d.received[0]>=10);can_send=1;sends=0;d.next=0;step(&d);assert(!sends);d.due[0]=time(NULL)-1;d.next=0;step(&d);assert(sends==1&&d.index==0&&(last_refresh>>8)==5);
 d.inactive=1;d.sent=0;d.next=0;d.due[0]=time(NULL)-1;step(&d);assert((last_refresh>>8)==20);d.inactive=0;d.sent=0;d.next=0;d.due[0]=time(NULL)-1;step(&d);assert((last_refresh>>8)==5);
 d.sent=AMIHASS_STATUS;strcpy(d.server,"https://old.invalid");strcpy(packet.status.server,"https://new.invalid");packet.status.connected=1;packet.item.state[0]=0;incoming=&packet;d.received[0]=time(NULL);d.results[0]=AMIHASS_OK;strcpy(d.values[0].state,"old-value");step(&d);assert(!d.received[0]&&!d.values[0].state[0]&&!d.results[0]);
 puts("Dashboard offline: last values retained, age unchanged on failures, timeout invalidation and source isolation passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),'-DAH_HOST_TEST','-include','ah_locale.h',str(root/'src/ah_locale.c'),str(root/'src/core.c'),str(root/'src/dashboard_layout.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
