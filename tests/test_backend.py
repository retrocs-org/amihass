#!/usr/bin/env python3
"""Run the production broker's selection, action and scheduling code with fake I/O."""
import os
import re
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
source=re.sub(r'/\*.*?\*/', '', (ROOT/'src/backend.c').read_text(), flags=re.S)
def block(start):
    at=source.index(start);opening=source.index('{',at);depth=1;i=opening+1;quote=None;escape=False
    while depth:
        c=source[i]
        if quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c==quote:quote=None
        elif c in ('"',"'"):quote=c
        elif c=='{':depth+=1
        elif c=='}':depth-=1
        i+=1
    return source[at:i]
structs=source[source.index('struct Subscription'):source.index('static unsigned long seconds')]
functions='\n'.join(block(signature) for signature in ('static void connection_failed(', 'static void connection_ok(', 'static void connection_refresh(', 'static const char *connection_state(', 'static void reload(', 'static int subscription(', 'static int cached_page(', 'static void cache_page(', 'static int active_subscription(', 'static int client_recent(', 'static void refresh_leases(', 'static int select_lease(', 'static void release_lease(', 'static void client_record(', 'static void client_snapshot(', 'static void diagnostics_snapshot(', 'static void record_http_started(', 'static void handle(', 'static void schedule(', 'static void complete('))
preamble=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amihass.h"
#include "amihass_config.h"
#include "sample.h"
#include "ah_log.h"
#include "memory_stats.h"
#define MEMF_FAST 1
#define MEMF_LARGEST 2
static unsigned long AvailMem(int flags){(void)flags;return 1000000UL;}
void ah_log_event(enum AHLogEvent event,long code){(void)event;(void)code;}
#define FETCH_GUARD "AmiHASS.Fetch"
#define SELECTED 48
#define QUEUED 8
static unsigned long now=1000000;
static unsigned long seconds(void){return now;}
static int sample_setting,network_up=1,auto_retry=1;static unsigned int retry_setting=60;
int amihass_network_running(void){return network_up;}
static int present(const char *p){(void)p;return 0;}
void amihass_erase(void *p,unsigned long n){memset(p,0,n);}
int amihass_connection_load(struct AmiHASSConnection *c,char token[1025]){strcpy(c->url,"https://fixture.local");c->poll=30;c->allow_http=0;token[0]=0;return 1;}
int amihass_config_load(struct AmiHASSConfig *c){memset(c,0,sizeof(*c));amihass_connection_load(&c->connection,c->token);strcpy(c->token,"fixture-test-token-for-backend");c->sample_mode=sample_setting;c->reconnect=auto_retry;c->retry_interval=retry_setting;return 1;}
static void FreeVec(void *p){free(p);}
'''
stubs=r'''
static void record_http_started(struct Backend *b,int phase);
static void reply(struct Backend *b,struct AmiHASSMessage *m){++b->diagnostics.replies;b->diagnostics.sent_bytes+=m->msg.mn_Length;}
static char sent_path[128],sent_entity[4096];static int sent_phase,read_fail=0,bad_api=0,start_fail=0;
static int job_start(struct Backend *b,const char *program,const char *path,const char *entity,int phase,int index){(void)program;if(start_fail)return 0;strcpy(sent_path,path);strcpy(sent_entity,entity ? entity:"");sent_phase=phase;record_http_started(b,phase);b->phase=phase;b->index=index;b->job_generation=b->generation;b->job_catalog_generation=b->catalog_generation;b->job=(struct MsgPort *)1;return 1;}
static char *read_data(struct Backend *b){char *p;const char *json="{\"entity_id\":\"light.desk\",\"state\":\"on\",\"attributes\":{}}";(void)b;if(read_fail)return NULL;if(b->phase==4)json=bad_api ? "{\"message\":\"not Home Assistant\"}":"{\"message\":\"API running.\"}";if(b->phase==5)json="{\"unit_system\":{\"temperature\":\"F\"}}";if(b->phase==2)json=b->catalog_kind==AMIHASS_AREAS ? "{\"total\":1,\"items\":[{\"id\":\"study\",\"name\":\"Study\"}]}":"{\"total\":0,\"items\":[]}";p=malloc(strlen(json)+1);strcpy(p,json);return p;}
'''
main=r'''
static void message(struct AmiHASSMessage *m,int kind,const char *owner,const char *entity,const char *action){memset(m,0,sizeof(*m));m->magic=AMIHASS_MAGIC;m->abi=AMIHASS_ABI;m->msg.mn_Length=sizeof(*m);m->kind=kind;if(owner)strcpy(m->owner,owner);if(entity)strcpy(m->entity,entity);if(action)strcpy(m->action,action);}
int main(void){struct Backend *b=calloc(1,sizeof(*b));struct AmiHASSMessage m;int a,other;
 reload(b);schedule(b,"HASS");assert(sent_phase==4);complete(b,1);b->job=NULL;message(&m,AMIHASS_QUERY,"HASS","light.desk",NULL);handle(b,&m);assert(m.result==AMIHASS_WAIT);a=subscription(b,"HASS","light.desk");other=subscription(b,"HASS_2","light.desk");assert(a==other);
 schedule(b,"HASS");assert(sent_phase==1 && !strcmp(sent_path,"/api/states/light.desk"));
 complete(b,1);b->job=NULL;assert(b->subscriptions[a].result==AMIHASS_OK && !strcmp(b->subscriptions[a].value.state,"on"));
 message(&m,AMIHASS_ACTION,"HASS","light.desk","Toggle");handle(b,&m);assert(m.result==AMIHASS_WAIT && b->queued==1 && b->subscriptions[a].result==AMIHASS_WAIT);
 schedule(b,"HASS");assert(sent_phase==3 && !strcmp(sent_path,"/api/services/homeassistant/toggle") && !strcmp(sent_entity,"light.desk"));
 complete(b,1);b->job=NULL;assert(b->subscriptions[a].result==AMIHASS_WAIT && b->subscriptions[a].due==0); /* no optimistic state */
 schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;assert(b->subscriptions[a].result==AMIHASS_OK);
 now+=100;b->next_health=now+60;message(&m,AMIHASS_ACTION,"HASS","light.desk","Turn off");handle(b,&m);assert(m.result==AMIHASS_STALE && !b->queued);
 message(&m,AMIHASS_ACTION,"HASS","sensor.temperature","Toggle");handle(b,&m);assert(m.result==AMIHASS_UNSUPPORTED && !b->queued);
 b->subscriptions[a].updated=now;b->subscriptions[a].result=AMIHASS_OK;message(&m,AMIHASS_ACTION,"HASS","light.desk","Turn off");handle(b,&m);assert(b->queued==1);
 message(&m,AMIHASS_RELOAD,NULL,NULL,NULL);handle(b,&m);assert(!b->queued && !b->subscriptions[a].value.id[0]); /* server changes never replay queued actions */
 schedule(b,"HASS");assert(sent_phase==4);complete(b,1);b->job=NULL;message(&m,AMIHASS_QUERY,"HASS","light.desk",NULL);handle(b,&m);a=subscription(b,"HASS","light.desk");schedule(b,"HASS");read_fail=1;b->queued=2;complete(b,-1);assert(!b->queued);b->job=NULL;assert(b->subscriptions[a].result==AMIHASS_FAILED && b->retry>now);
 sent_phase=0;schedule(b,"HASS");assert(!sent_phase); /* backoff */
 b->retry=0;read_fail=0;schedule(b,"HASS");assert(sent_phase==4);complete(b,1);b->job=NULL;message(&m,AMIHASS_LIST,NULL,NULL,NULL);strcpy(m.domain,"light");handle(b,&m);schedule(b,"AmiHASS");assert(b->phase==2);
 message(&m,AMIHASS_LIST,NULL,NULL,NULL);strcpy(m.domain,"scene");handle(b,&m);assert(m.result==AMIHASS_WAIT && !strcmp(b->catalog_domain,"light"));
 complete(b,1);b->job=NULL;message(&m,AMIHASS_LIST,NULL,NULL,NULL);strcpy(m.domain,"light");handle(b,&m);assert(m.result==AMIHASS_OK && m.total==0);
 message(&m,AMIHASS_LIST,NULL,NULL,NULL);strcpy(m.domain,"scene");handle(b,&m);schedule(b,"AmiHASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_LIST,NULL,NULL,NULL);strcpy(m.domain,"light");handle(b,&m);assert(m.result==AMIHASS_OK);
 message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK && m.status.subscriptions>0 && !strstr(m.status.server,"token"));
 message(&m,AMIHASS_STOP,NULL,NULL,NULL);handle(b,&m);assert(b->stopping && m.result==AMIHASS_OK && !b->queued);
 sent_phase=0;b->catalog_state=AMIHASS_WAIT;schedule(b,"AmiHASS");assert(!sent_phase);
 message(&m,AMIHASS_QUERY,"HASS","light.desk",NULL);handle(b,&m);assert(m.result==AMIHASS_FAILED);
 b->stopping=0;message(&m,AMIHASS_STOP,NULL,NULL,NULL);m.abi=99;handle(b,&m);assert(!b->stopping);
 assert(b->diagnostics.queries>0 && b->diagnostics.actions>0 && b->diagnostics.accepted>0 && b->diagnostics.action_http>0 && b->diagnostics.poll_http>0 && b->diagnostics.catalog_http>0);
 assert(b->diagnostics.http_failed>0 && b->diagnostics.http_ok>0 && b->diagnostics.cache_hits>0 && b->diagnostics.invalid==1);
 message(&m,AMIHASS_DIAGNOSTICS,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK && m.count==16 && m.total==25 && !strcmp(m.items[0].id,"ipc.received"));
 assert(strtoul(m.items[0].state,NULL,10)==b->diagnostics.received);
 message(&m,AMIHASS_DIAGNOSTICS,NULL,NULL,NULL);m.offset=16;handle(b,&m);assert(m.count==9 && !strcmp(m.items[0].id,"service.uptime"));
 message(&m,AMIHASS_DIAGNOSTICS,NULL,NULL,NULL);m.offset=-1;b->connection.url[0]=0;handle(b,&m);assert(m.result==AMIHASS_OK && m.count==16);
 {
 unsigned int intervals[]={60,600,900,1800};int k;
 for(k=0;k<4;++k){
  memset(b,0,sizeof(*b));auto_retry=1;retry_setting=intervals[k];network_up=1;read_fail=bad_api=start_fail=0;reload(b);
  message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_CONNECTING)&&m.count==3);
  schedule(b,"HASS");assert(sent_phase==4&&!strcmp(sent_path,"/api/"));complete(b,-1);b->job=NULL;
  message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK&&!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_RETRY_WAIT));assert(strtoul(m.items[2].state,NULL,10)==intervals[k]);
  /* Client catalogue changes cannot bypass the configured retry time. */
  message(&m,AMIHASS_LIST,NULL,NULL,NULL);strcpy(m.domain,"light");m.refresh=1;handle(b,&m);
  sent_phase=0;now+=intervals[k]-1;schedule(b,"HASS");assert(!sent_phase);++now;schedule(b,"HASS");assert(sent_phase==4);
  bad_api=1;complete(b,1);b->job=NULL;assert(!b->connected&&b->retry==now+intervals[k]);
  now+=intervals[k];bad_api=0;schedule(b,"HASS");complete(b,1);b->job=NULL;
  message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_CONNECTED)&&m.status.subscriptions==0);
  /* Idle service checks connection; a failure immediately clears the old success. */
  b->catalog_state=0;now+=60;schedule(b,"HASS");assert(sent_phase==4);b->queued=2;complete(b,-1);b->job=NULL;
  message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!b->queued);
 }
 memset(b,0,sizeof(*b));auto_retry=0;retry_setting=600;reload(b);schedule(b,"HASS");assert(sent_phase==4);complete(b,-1);b->job=NULL;
 now+=3600;sent_phase=0;schedule(b,"HASS");assert(!sent_phase);
 message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_DISCONNECTED)&&!strcmp(m.items[2].state,"0"));
 message(&m,AMIHASS_RECONNECT,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK);schedule(b,"HASS");assert(sent_phase==4);complete(b,1);b->job=NULL;
 network_up=0;++now;message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_NETWORK_DOWN));sent_phase=0;schedule(b,"HASS");assert(!sent_phase);
 network_up=1;now+=1000;schedule(b,"HASS");assert(!sent_phase); /* remains manual after a failure */
 message(&m,AMIHASS_RECONNECT,NULL,NULL,NULL);handle(b,&m);start_fail=1;schedule(b,"HASS");assert(!b->connected&&b->retry==now+600);start_fail=0;
 b->configured=0;b->connection.url[0]=0;message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_UNCONFIGURED));
 }
 memset(b,0,sizeof(*b));auto_retry=1;network_up=1;reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_QUERY,"Browser","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(b,&m);a=subscription(b,"Browser","light.desk");assert(active_subscription(&b->subscriptions[a],now));schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;assert(!active_subscription(&b->subscriptions[a],now));
 now+=31;b->next_health=now+60;sent_phase=0;schedule(b,"HASS");assert(!sent_phase);
 message(&m,AMIHASS_QUERY,"Dock","light.desk",NULL);handle(b,&m);assert(active_subscription(&b->subscriptions[a],now));schedule(b,"HASS");complete(b,1);b->job=NULL;
 now+=100;message(&m,AMIHASS_QUERY,"Browser","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(b,&m);schedule(b,"HASS");complete(b,1);b->job=NULL;now+=21;assert(!active_subscription(&b->subscriptions[a],now));
 message(&m,AMIHASS_LIST,NULL,"invalid",NULL);handle(b,&m);assert(m.result==AMIHASS_FAILED);
 message(&m,AMIHASS_DEVICES,NULL,NULL,NULL);handle(b,&m);schedule(b,"HASS");if(b->phase==4){complete(b,1);b->job=NULL;schedule(b,"HASS");}assert(b->phase==2&&b->catalog_kind==AMIHASS_DEVICES);complete(b,1);b->job=NULL;
 message(&m,AMIHASS_DEVICES,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK);
 message(&m,AMIHASS_LIST,NULL,"0123456789abcdef0123456789abcdef",NULL);handle(b,&m);assert(m.result==AMIHASS_WAIT&&!strcmp(b->catalog_device,m.entity));
 /* A slow request keeps IPC available; new work waits without parallel HTTP. */
 memset(b,0,sizeof(*b));reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_QUERY,"Dock","light.desk",NULL);handle(b,&m);a=subscription(b,"Dock","light.desk");
 b->catalog_state=AMIHASS_WAIT;b->catalog_kind=AMIHASS_DEVICES;b->next_health=0;
 schedule(b,"HASS");assert(sent_phase==1); /* due state before discovery and health */
 message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK);
 sent_phase=0;schedule(b,"HASS");assert(!sent_phase); /* no second in-flight worker */
 complete(b,1);b->job=NULL;b->subscriptions[a].due=0;
 schedule(b,"HASS");assert(sent_phase==2); /* alternate to avoid starving discovery */
 complete(b,1);b->job=NULL;b->catalog_state=AMIHASS_WAIT;
 schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;
 message(&m,AMIHASS_ACTION,"Dock","light.desk","Toggle");handle(b,&m);assert(b->queued==1);
 schedule(b,"HASS");assert(sent_phase==3);complete(b,1);b->job=NULL;
 b->prefer_state=0;schedule(b,"HASS");assert(sent_phase==1); /* urgent action readback beats catalogue */
 complete(b,1);b->job=NULL;assert(!b->subscriptions[a].urgent);
 /* Per-query intervals share one entity cache; the fastest current caller drives reads. */
 memset(b,0,sizeof(*b));reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;message(&m,AMIHASS_QUERY,"Slow","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(300);handle(b,&m);schedule(b,"HASS");complete(b,1);b->job=NULL;a=subscription(b,"Slow","light.desk");now+=6;message(&m,AMIHASS_QUERY,"Slow","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(300);handle(b,&m);assert(m.result==AMIHASS_OK&&!b->subscriptions[a].once);message(&m,AMIHASS_QUERY,"Fast","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(5);handle(b,&m);assert(m.result==AMIHASS_WAIT&&b->subscriptions[a].once);assert(subscription(b,"Fast","light.desk")==a);schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;message(&m,AMIHASS_QUERY,"Slow","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(300);handle(b,&m);assert(m.result==AMIHASS_OK&&m.age==0&&!b->subscriptions[a].once);message(&m,AMIHASS_QUERY,"Fast","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(1);handle(b,&m);assert(m.result==AMIHASS_UNSUPPORTED);now+=100;message(&m,AMIHASS_QUERY,"Slow","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(300);handle(b,&m);assert(m.result==AMIHASS_OK);
 /* A fresh read starts once, then normal queries can obtain the completed state. */
 memset(b,0,sizeof(*b));reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_QUERY,"HASS","light.desk",NULL);handle(b,&m);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_QUERY,"HASS","light.desk",NULL);m.refresh=AMIHASS_QUERY_FRESH;handle(b,&m);assert(m.result==AMIHASS_WAIT);schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;
 message(&m,AMIHASS_QUERY,"HASS","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(b,&m);assert(m.result==AMIHASS_OK);
 a=subscription(b,"HASS","climate.room");b->subscriptions[a].controls.temperature=1;b->subscriptions[a].controls.target=7200;b->subscriptions[a].controls.minimum=5000;b->subscriptions[a].controls.maximum=9000;b->subscriptions[a].controls.step=50;
 assert(!amihass_control_allowed(&b->subscriptions[a].controls,"Temperature 72"));b->unit_needed=1;schedule(b,"HASS");assert(sent_phase==5&&!strcmp(sent_path,"/api/config"));complete(b,1);b->job=NULL;assert(b->connected&&!b->unit_needed&&!strcmp(b->temperature_unit,"F")&&amihass_control_allowed(&b->subscriptions[a].controls,"Temperature 72"));
 memset(b,0,sizeof(*b));sample_setting=1;network_up=1;reload(b);sent_phase=0;schedule(b,"HASS");assert(!sent_phase&&!b->job);network_up=0;
 message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_SAMPLE)&&!strcmp(m.status.server,"sample://amihass"));
 message(&m,AMIHASS_DEVICES,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK&&m.count==4&&amihass_device_id(m.items[0].id));
 {char device[96];strcpy(device,m.items[0].id);message(&m,AMIHASS_LIST,NULL,device,NULL);handle(b,&m);assert(m.result==AMIHASS_OK&&m.count==3);}
 message(&m,AMIHASS_DEVICES,NULL,NULL,NULL);strcpy(m.domain,"light");handle(b,&m);assert(m.result==AMIHASS_OK&&m.count==2);
 message(&m,AMIHASS_QUERY,"Dock1","light.amihass_sample_living_room",NULL);handle(b,&m);assert(m.result==AMIHASS_OK&&!strcmp(m.item.state,"on"));
 message(&m,AMIHASS_ACTION,"Dock1","light.amihass_sample_living_room","Toggle");handle(b,&m);assert(m.result==AMIHASS_WAIT&&!strcmp(m.item.state,"off"));
 message(&m,AMIHASS_QUERY,"Dock2","light.amihass_sample_living_room",NULL);handle(b,&m);assert(m.result==AMIHASS_OK&&!strcmp(m.item.state,"off"));
 message(&m,AMIHASS_ACTION,"Dock2","sensor.amihass_sample_house_power","Toggle");handle(b,&m);assert(m.result==AMIHASS_UNSUPPORTED);
 message(&m,AMIHASS_ACTION,"Dock2","light.real_device","Toggle");handle(b,&m);assert(m.result==AMIHASS_UNCONFIGURED);
 message(&m,AMIHASS_QUERY,"Dock2","sensor.amihass_sample_garden_battery",NULL);handle(b,&m);assert(m.result==AMIHASS_UNAVAILABLE);
 schedule(b,"HASS");assert(!sent_phase&&b->diagnostics.action_http==0&&b->diagnostics.poll_http==0&&b->diagnostics.catalog_http==0&&b->diagnostics.health_http==0);
 sample_setting=0;network_up=1;reload(b);message(&m,AMIHASS_ACTION,"Dock1","light.amihass_sample_living_room","Toggle");handle(b,&m);assert(m.result==AMIHASS_UNCONFIGURED);assert(!b->sample_mode&&!b->catalog_count&&!b->subscriptions[0].owner[0]);schedule(b,"HASS");assert(sent_phase==4);b->job=NULL;
 {int j;struct Backend tracked;memset(&tracked,0,sizeof(tracked));message(&m,AMIHASS_QUERY,"DockOne","light.shared",NULL);client_record(&tracked,&m,now);client_record(&tracked,&m,now);message(&m,AMIHASS_ACTION,"DashTwo","light.shared","Toggle");client_record(&tracked,&m,now);message(&m,AMIHASS_LIST,"DashTwo",NULL,NULL);client_record(&tracked,&m,now);memset(&m,0,sizeof(m));client_snapshot(&tracked,&m,now);assert(m.result==AMIHASS_OK&&m.total==4&&m.count==4&&!strcmp(m.items[0].name,"DockOne")&&!strcmp(m.items[0].state,"2")&&!strcmp(m.items[0].unit,"2")&&!strcmp(m.items[1].id,"light.shared")&&!strcmp(m.items[2].device_class,"1")&&!strcmp(m.items[2].updated,"1"));client_snapshot(&tracked,&m,now+121);assert(!m.count&&!m.total);for(j=0;j<20;++j){char owner[20];sprintf(owner,"Client%d",j);message(&m,AMIHASS_QUERY,owner,"light.shared",NULL);client_record(&tracked,&m,now);}memset(&m,0,sizeof(m));client_snapshot(&tracked,&m,now);assert(m.total==32&&m.count==16);m.offset=16;client_snapshot(&tracked,&m,now);assert(m.count==16);message(&m,AMIHASS_QUERY,"NewClient","light.new",NULL);client_record(&tracked,&m,now+121);memset(&m,0,sizeof(m));client_snapshot(&tracked,&m,now+121);assert(m.total==2&&!strcmp(m.items[0].name,"NewClient"));}

 /* Two independent clients share one selected entity and release separately. */
 memset(b,0,sizeof(*b));sample_setting=0;network_up=1;auto_retry=1;reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_SUBSCRIBE,"Fast","light.desk",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)0x1110;m.refresh=AMIHASS_QUERY_INTERVAL(5);handle(b,&m);assert(m.result==AMIHASS_WAIT);a=subscription(b,"Fast","light.desk");schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;assert(b->subscriptions[a].due==now+5);
 message(&m,AMIHASS_SUBSCRIBE,"Slow","light.desk",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)0x2220;m.refresh=AMIHASS_QUERY_INTERVAL(300);handle(b,&m);assert(m.result==AMIHASS_OK&&subscription(b,"Slow","light.desk")==a&&b->subscriptions[a].interval==5);
 message(&m,AMIHASS_UNSUBSCRIBE,NULL,"light.desk",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)0x1110;handle(b,&m);assert(m.result==AMIHASS_OK&&active_subscription(&b->subscriptions[a],now)&&b->subscriptions[a].interval==300&&b->subscriptions[a].due==now+300);
 message(&m,AMIHASS_UNSUBSCRIBE,NULL,NULL,NULL);m.msg.mn_ReplyPort=(struct MsgPort *)0x2220;handle(b,&m);assert(!active_subscription(&b->subscriptions[a],now));b->next_health=now+600;sent_phase=0;schedule(b,"HASS");assert(!sent_phase);
 message(&m,AMIHASS_SUBSCRIBE,"Expired","light.desk",NULL);m.refresh=AMIHASS_QUERY_INTERVAL(30);handle(b,&m);now+=121;refresh_leases(b,now);assert(!active_subscription(&b->subscriptions[a],now));
 message(&m,AMIHASS_SUBSCRIBE,"Bad","light.desk",NULL);m.refresh=AMIHASS_QUERY_INTERVAL(1);handle(b,&m);assert(m.result==AMIHASS_UNSUPPORTED);
 message(&m,AMIHASS_SUBSCRIBE,"Bad","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(b,&m);assert(m.result==AMIHASS_UNSUPPORTED);
 /* Area changes never reuse another area's page or alter its in-flight request. */
 memset(b,0,sizeof(*b));reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_DEVICES,"Browser",NULL,NULL);strcpy(m.item.id,"Kitchen");handle(b,&m);schedule(b,"HASS");assert(b->phase==2&&!strcmp(b->catalog_area,"Kitchen")&&strstr(sent_entity,"area_devices"));
 message(&m,AMIHASS_DEVICES,"Browser",NULL,NULL);strcpy(m.item.id,"Study");handle(b,&m);assert(m.result==AMIHASS_WAIT&&!strcmp(b->catalog_area,"Kitchen"));complete(b,1);b->job=NULL;
 message(&m,AMIHASS_DEVICES,"Browser",NULL,NULL);strcpy(m.item.id,"Study");handle(b,&m);assert(m.result==AMIHASS_WAIT&&!strcmp(b->catalog_area,"Study"));schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_DEVICES,"Browser",NULL,NULL);strcpy(m.item.id,"Kitchen");handle(b,&m);assert(m.result==AMIHASS_OK);
 message(&m,AMIHASS_DIAGNOSTICS,"Diagnostics",NULL,NULL);strcpy(m.domain,"memory");handle(b,&m);assert(m.count==12&&!strcmp(m.items[0].id,"memory.fast.baseline")&&!strcmp(m.items[11].id,"leases.active"));

 /* Room discovery is paged, separately cached and never creates a state lease. */
 message(&m,AMIHASS_AREAS,"Rooms",NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_WAIT);schedule(b,"HASS");assert(b->phase==2&&strstr(sent_entity,"areas()")&&!strstr(sent_entity,"for e in states"));complete(b,1);b->job=NULL;
 message(&m,AMIHASS_AREAS,"Rooms",NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK&&m.total==1&&m.count==1&&!strcmp(m.items[0].id,"study")&&!strcmp(m.items[0].name,"Study"));
 message(&m,AMIHASS_AREAS,"Rooms",NULL,NULL);m.offset=16;handle(b,&m);schedule(b,"HASS");assert(strstr(sent_entity,"entries[16:32]"));complete(b,1);b->job=NULL;

 /* Released page selections can be replaced immediately, without a 120s stall. */
 {int i;char id[96];memset(b,0,sizeof(*b));sample_setting=0;reload(b);b->connected=1;b->network=1;b->network_known=1;b->last_success=now;b->next_network=now+1;
  for(i=0;i<SELECTED;++i){sprintf(id,"light.pool_%d",i);message(&m,AMIHASS_SUBSCRIBE,"Pool",id,NULL);m.msg.mn_ReplyPort=(struct MsgPort *)(unsigned long)(100+i);handle(b,&m);assert(m.result!=AMIHASS_BUSY);}
  message(&m,AMIHASS_SUBSCRIBE,"Pool","light.extra",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)1000;handle(b,&m);assert(m.result==AMIHASS_BUSY);
  for(i=0;i<SELECTED;++i){message(&m,AMIHASS_UNSUBSCRIBE,NULL,NULL,NULL);m.msg.mn_ReplyPort=(struct MsgPort *)(unsigned long)(100+i);handle(b,&m);}
  message(&m,AMIHASS_SUBSCRIBE,"Pool","light.extra",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)1000;handle(b,&m);assert(m.result!=AMIHASS_BUSY);a=subscription(b,"Pool","light.extra");assert(a>=0&&!b->subscriptions[a].value.id[0]);
 }
 free(b);puts("HASS broker deduplication, action validation/readback, stale state, connection reload and backoff passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp=Path(temporary);(tmp/'exec').mkdir();(tmp/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort; struct Message { unsigned short mn_Length; struct MsgPort *mn_ReplyPort; };\n')
    test=tmp/'test.c';test.write_text(preamble+structs+stubs+functions+main)
    exe=tmp/'test';command=[os.environ.get('CC','cc'),'-std=c99','-g','-Wno-deprecated-declarations','-I'+str(tmp),'-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(test),str(ROOT/'src/core.c'),str(ROOT/'src/sample.c'),str(ROOT/'src/browser_data.c'),'-o',str(exe)]
    if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
    subprocess.run(command,check=True);subprocess.run([str(exe)],check=True)
