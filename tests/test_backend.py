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
functions=(ROOT/'include/press_receipts.h').read_text()+'\n'+'static void connection_ok(struct Backend*,unsigned long);\n'+(ROOT/'include/camera_backend.h').read_text()+'\n'+(ROOT/'include/detail_backend.h').read_text()+'\n'+'static void push_step(struct Backend *b,const char *program,unsigned long now){(void)b;(void)program;(void)now;}\n'+'\n'.join(block(signature) for signature in ('static void connection_failed(', 'static void connection_ok(', 'static void connection_refresh(', 'static const char *connection_state(', 'static int backend_slots(', 'static void reload(', 'static int subscription(', 'static int cached_page(', 'static void cache_page(', 'static int active_subscription(', 'static int client_recent(', 'static void refresh_leases(', 'static void selection_full(', 'static void leases_update(', 'static int select_lease(', 'static void release_lease(', 'static void client_record(', 'static void client_snapshot(', 'static void diagnostics_snapshot(', 'static void record_http_started(', 'static void notes_request(', 'static void handle(', 'static int background_allow(', 'static int poll_done(', 'static void poll_same(', 'static void batch_done(', 'static void start_failed(', 'static int poll_wanted(', 'static int start_poll(', 'static void schedule(', 'static void complete('))
preamble=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amihass.h"
#include "camera.h"
#include "amihass_config.h"
#include "sample.h"
#include "ah_log.h"
#include "memory_stats.h"
#include "push_state.h"
#define MEMF_FAST 1
#define MEMF_LARGEST 2
static unsigned long fake_avail=1000000UL;static unsigned long AvailMem(int flags){(void)flags;return fake_avail;}
void ah_log_event(enum AHLogEvent event,long code){(void)event;(void)code;}
#define FETCH_GUARD "AmiHASS.Fetch"
#define SELECTED ((int)b->limit)
#define LEASES ((int)b->leases)
#ifndef MEMF_ANY
#define MEMF_ANY 0
#endif
#ifndef MEMF_PUBLIC
#define MEMF_PUBLIC 1
#endif
#ifndef MEMF_CLEAR
#define MEMF_CLEAR 0x10000
#endif
static void *AllocVec(unsigned long n,unsigned long f){(void)f;return calloc(1,n);}
#define QUEUED 8
static unsigned long now=1000000;
static unsigned long seconds(void){return now;}
static int sample_setting,network_up=1,auto_retry=1;static unsigned int retry_setting=60;
int amihass_network_running(void){return network_up;}
static int present(const char *p){(void)p;return 0;}
static long GetVar(const char *n,char *b,long s,unsigned long f){(void)n;(void)b;(void)s;(void)f;return -1;}
static void native_step(void *b,const char *p,unsigned long n){(void)b;(void)p;(void)n;}
static void native_result(void *b,int c){(void)b;(void)c;}
void amihass_erase(void *p,unsigned long n){memset(p,0,n);}
int amihass_connection_load(struct AmiHASSConnection *c,char token[1025]){strcpy(c->url,"https://fixture.local");c->poll=30;c->allow_http=0;token[0]=0;return 1;}
int amihass_config_load(struct AmiHASSConfig *c){memset(c,0,sizeof(*c));amihass_connection_load(&c->connection,c->token);strcpy(c->token,"fixture-test-token-for-backend");c->sample_mode=sample_setting;c->reconnect=auto_retry;c->retry_interval=retry_setting;return 1;}
static void FreeVec(void *p){free(p);}
'''
stubs=r'''
static void record_http_started(struct Backend *b,int phase);
static struct AmiHASSMessage *fanout_message;
static void reply(struct Backend *b,struct AmiHASSMessage *m){if(m==fanout_message)return;++b->diagnostics.replies;b->diagnostics.sent_bytes+=m->msg.mn_Length;}
static const char *state_fixture,*batch_fixture,*catalog_fixture;
static char sent_path[260],sent_entity[4096];static int sent_phase,read_fail=0,bad_api=0,start_fail=0;
static int job_start(struct Backend *b,const char *program,const char *path,const char *entity,int phase,int index){(void)program;if(start_fail)return 0;strcpy(sent_path,path);strcpy(sent_entity,entity ? entity:"");sent_phase=phase;record_http_started(b,phase);b->phase=phase;b->index=index;if(phase==3){strcpy(b->job_action,b->queue[0].action);b->job_ticket=b->queue[0].ticket;}b->job_generation=b->generation;b->job_catalog_generation=b->catalog_generation;b->job=(struct MsgPort *)1;return 1;}
static char *read_data(struct Backend *b){char *p;const char *json="{\"entity_id\":\"light.desk\",\"state\":\"on\",\"attributes\":{}}";(void)b;if(b->phase==1&&state_fixture)json=state_fixture;if(b->phase==1&&b->batch_count>1&&batch_fixture)json=batch_fixture;if(read_fail)return NULL;if(b->phase==4)json=bad_api ? "{\"message\":\"not Home Assistant\"}":"{\"message\":\"API running.\"}";if(b->phase==5)json="{\"unit_system\":{\"temperature\":\"F\"}}";if(b->phase==2)json=b->catalog_kind==AMIHASS_AREAS ? "{\"total\":1,\"items\":[{\"id\":\"study\",\"name\":\"Study\"}]}":"{\"total\":0,\"items\":[]}";if(b->phase==2&&catalog_fixture)json=catalog_fixture;p=malloc(strlen(json)+1);strcpy(p,json);return p;}
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
 {unsigned long before=b->diagnostics.total_seconds;b->diagnostics.request_started=now-7;complete(b,-1);assert(b->diagnostics.last_seconds==7&&b->diagnostics.max_seconds>=7&&b->diagnostics.total_seconds==before+7);message(&m,AMIHASS_DIAGNOSTICS,NULL,NULL,NULL);strcpy(m.domain,"cost");handle(b,&m);assert(m.result==AMIHASS_OK&&m.count==9&&m.total==9&&!strcmp(m.items[1].id,"http.last_seconds")&&!strcmp(m.items[1].state,"7"));}
 assert(b->diagnostics.http_failed>0 && b->diagnostics.http_ok>0 && b->diagnostics.cache_hits>0 && b->diagnostics.invalid==1);
 message(&m,AMIHASS_DIAGNOSTICS,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK && m.count==16 && m.total==27 && !strcmp(m.items[0].id,"ipc.received"));
 assert(strtoul(m.items[0].state,NULL,10)==b->diagnostics.received);
 message(&m,AMIHASS_DIAGNOSTICS,NULL,NULL,NULL);m.offset=16;handle(b,&m);assert(m.count==11 && !strcmp(m.items[0].id,"service.uptime") && !strcmp(m.items[9].id,"entities.limit") && !strcmp(m.items[9].state,"48") && !strcmp(m.items[10].id,"entities.refused"));
 message(&m,AMIHASS_DIAGNOSTICS,NULL,NULL,NULL);m.offset=-1;b->connection.url[0]=0;handle(b,&m);assert(m.result==AMIHASS_OK && m.count==16);
 {
 unsigned int intervals[]={60,600,900,1800};int k;
 for(k=0;k<4;++k){
  {FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}auto_retry=1;retry_setting=intervals[k];network_up=1;read_fail=bad_api=start_fail=0;reload(b);
  message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_CONNECTING)&&m.count>=7&&!strcmp(m.items[3].id,AMIHASS_CAP_AREAS)&&!strcmp(m.items[4].id,AMIHASS_CAP_FILTERS)&&!strcmp(m.items[5].id,AMIHASS_CAP_SUBSCRIPTIONS)&&!strcmp(m.items[3].state,"1"));
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
 {FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}auto_retry=0;retry_setting=600;reload(b);schedule(b,"HASS");assert(sent_phase==4);complete(b,-1);b->job=NULL;
 now+=3600;sent_phase=0;schedule(b,"HASS");assert(!sent_phase);
 message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_DISCONNECTED)&&!strcmp(m.items[2].state,"0"));
 {int slot=subscription(b,"Retry.Keep","light.desk");unsigned long generation=b->generation;message(&m,AMIHASS_RECONNECT,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK&&b->generation==generation&&!strcmp(b->subscriptions[slot].entity,"light.desk")&&b->retry_interval==600);}schedule(b,"HASS");assert(sent_phase==4);complete(b,1);b->job=NULL;
 network_up=0;now+=5;message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_NETWORK_DOWN));sent_phase=0;schedule(b,"HASS");assert(!sent_phase);
 network_up=1;now+=1000;schedule(b,"HASS");assert(!sent_phase); /* remains manual after a failure */
 message(&m,AMIHASS_RECONNECT,NULL,NULL,NULL);handle(b,&m);start_fail=1;schedule(b,"HASS");assert(!b->connected&&b->retry==now+600);start_fail=0;
 b->configured=0;b->connection.url[0]=0;message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_UNCONFIGURED));
 }
 {FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}auto_retry=1;network_up=1;reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_QUERY,"Browser","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(b,&m);a=subscription(b,"Browser","light.desk");assert(active_subscription(&b->subscriptions[a],now));schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;assert(!active_subscription(&b->subscriptions[a],now));
 now+=31;b->next_health=now+60;sent_phase=0;schedule(b,"HASS");assert(!sent_phase);
 message(&m,AMIHASS_QUERY,"Dock","light.desk",NULL);handle(b,&m);assert(active_subscription(&b->subscriptions[a],now));schedule(b,"HASS");complete(b,1);b->job=NULL;
 now+=100;message(&m,AMIHASS_QUERY,"Browser","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(b,&m);schedule(b,"HASS");complete(b,1);b->job=NULL;now+=21;assert(!active_subscription(&b->subscriptions[a],now));
 message(&m,AMIHASS_LIST,NULL,"invalid",NULL);handle(b,&m);assert(m.result==AMIHASS_FAILED);
 message(&m,AMIHASS_DEVICES,NULL,NULL,NULL);handle(b,&m);schedule(b,"HASS");if(b->phase==4){complete(b,1);b->job=NULL;schedule(b,"HASS");}assert(b->phase==2&&b->catalog_kind==AMIHASS_DEVICES);complete(b,1);b->job=NULL;
 message(&m,AMIHASS_DEVICES,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK);
 message(&m,AMIHASS_LIST,NULL,"0123456789abcdef0123456789abcdef",NULL);handle(b,&m);assert(m.result==AMIHASS_WAIT&&!strcmp(b->catalog_device,m.entity));
 /* A slow request keeps IPC available; new work waits without parallel HTTP. */
 {FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
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
 {FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;message(&m,AMIHASS_QUERY,"Slow","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(300);handle(b,&m);schedule(b,"HASS");complete(b,1);b->job=NULL;a=subscription(b,"Slow","light.desk");now+=6;message(&m,AMIHASS_QUERY,"Slow","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(300);handle(b,&m);assert(m.result==AMIHASS_OK&&!b->subscriptions[a].once);message(&m,AMIHASS_QUERY,"Fast","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(5);handle(b,&m);assert(m.result==AMIHASS_WAIT&&b->subscriptions[a].once);assert(subscription(b,"Fast","light.desk")==a);schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;message(&m,AMIHASS_QUERY,"Slow","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(300);handle(b,&m);assert(m.result==AMIHASS_OK&&m.age==0&&!b->subscriptions[a].once);message(&m,AMIHASS_QUERY,"Fast","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(1);handle(b,&m);assert(m.result==AMIHASS_UNSUPPORTED);now+=100;message(&m,AMIHASS_QUERY,"Slow","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_INTERVAL(300);handle(b,&m);assert(m.result==AMIHASS_OK);
 /* A fresh read starts once, then normal queries can obtain the completed state. */
 {FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_QUERY,"HASS","light.desk",NULL);handle(b,&m);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_QUERY,"HASS","light.desk",NULL);m.refresh=AMIHASS_QUERY_FRESH;handle(b,&m);assert(m.result==AMIHASS_WAIT);schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;
 message(&m,AMIHASS_QUERY,"HASS","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(b,&m);assert(m.result==AMIHASS_OK);
 a=subscription(b,"HASS","climate.room");b->subscriptions[a].controls.temperature=1;b->subscriptions[a].controls.target=7200;b->subscriptions[a].controls.minimum=5000;b->subscriptions[a].controls.maximum=9000;b->subscriptions[a].controls.step=50;
 assert(!amihass_control_allowed(&b->subscriptions[a].controls,"Temperature 72"));b->unit_needed=1;schedule(b,"HASS");assert(sent_phase==5&&!strcmp(sent_path,"/api/config"));complete(b,1);b->job=NULL;assert(b->connected&&!b->unit_needed&&!strcmp(b->temperature_unit,"F")&&amihass_control_allowed(&b->subscriptions[a].controls,"Temperature 72"));
 {FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}sample_setting=1;network_up=1;reload(b);sent_phase=0;schedule(b,"HASS");assert(!sent_phase&&!b->job);network_up=0;
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
 {struct Backend tracked;int selected;memset(&tracked,0,sizeof(tracked));backend_slots(&tracked);tracked.connection.poll=30;selected=subscription(&tracked,"Fast","light.shared");message(&m,AMIHASS_SUBSCRIBE,"Fast","light.shared",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)1;assert(select_lease(&tracked,&m,now,10));message(&m,AMIHASS_SUBSCRIBE,"Slow","light.shared",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)2;assert(select_lease(&tracked,&m,now,30));assert(tracked.subscriptions[selected].interval==10);refresh_leases(&tracked,now+91);assert(tracked.subscriptions[selected].interval==40);assert(select_lease(&tracked,&m,now+91,30));assert(tracked.subscriptions[selected].interval==30);refresh_leases(&tracked,now+212);assert(!tracked.subscriptions[selected].watching&&!active_subscription(&tracked.subscriptions[selected],now+212));FreeVec(tracked.subscriptions);}
 {int j;struct Backend tracked;memset(&tracked,0,sizeof(tracked));backend_slots(&tracked);message(&m,AMIHASS_QUERY,"DockOne","light.shared",NULL);client_record(&tracked,&m,now);client_record(&tracked,&m,now);message(&m,AMIHASS_ACTION,"DashTwo","light.shared","Toggle");client_record(&tracked,&m,now);message(&m,AMIHASS_LIST,"DashTwo",NULL,NULL);client_record(&tracked,&m,now);memset(&m,0,sizeof(m));client_snapshot(&tracked,&m,now);assert(m.result==AMIHASS_OK&&m.total==4&&m.count==4&&!strcmp(m.items[0].name,"DockOne")&&!strcmp(m.items[0].state,"2")&&!strcmp(m.items[0].unit,"2")&&!strcmp(m.items[1].id,"light.shared")&&!strcmp(m.items[2].device_class,"1")&&!strcmp(m.items[2].updated,"1"));client_snapshot(&tracked,&m,now+121);assert(!m.count&&!m.total);for(j=0;j<20;++j){char owner[20];sprintf(owner,"Client%d",j);message(&m,AMIHASS_QUERY,owner,"light.shared",NULL);client_record(&tracked,&m,now);}memset(&m,0,sizeof(m));client_snapshot(&tracked,&m,now);assert(m.total==32&&m.count==16);m.offset=16;client_snapshot(&tracked,&m,now);assert(m.count==16);message(&m,AMIHASS_QUERY,"NewClient","light.new",NULL);client_record(&tracked,&m,now+121);memset(&m,0,sizeof(m));client_snapshot(&tracked,&m,now+121);assert(m.total==2&&!strcmp(m.items[0].name,"NewClient"));FreeVec(tracked.subscriptions);}

 /* Two independent clients share one selected entity and release separately. */
 {FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}sample_setting=0;network_up=1;auto_retry=1;reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
 message(&m,AMIHASS_SUBSCRIBE,"Fast","light.desk",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)0x1110;m.refresh=AMIHASS_QUERY_INTERVAL(5);handle(b,&m);assert(m.result==AMIHASS_WAIT);a=subscription(b,"Fast","light.desk");schedule(b,"HASS");assert(sent_phase==1);complete(b,1);b->job=NULL;assert(b->subscriptions[a].due==now+5);
 message(&m,AMIHASS_SUBSCRIBE,"Slow","light.desk",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)0x2220;m.refresh=AMIHASS_QUERY_INTERVAL(300);handle(b,&m);assert(m.result==AMIHASS_OK&&subscription(b,"Slow","light.desk")==a&&b->subscriptions[a].interval==5);
 message(&m,AMIHASS_UNSUBSCRIBE,NULL,"light.desk",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)0x1110;handle(b,&m);assert(m.result==AMIHASS_OK&&active_subscription(&b->subscriptions[a],now)&&b->subscriptions[a].interval==300&&b->subscriptions[a].due==now+300);
 message(&m,AMIHASS_UNSUBSCRIBE,NULL,NULL,NULL);m.msg.mn_ReplyPort=(struct MsgPort *)0x2220;handle(b,&m);assert(!active_subscription(&b->subscriptions[a],now));b->next_health=now+600;sent_phase=0;schedule(b,"HASS");assert(!sent_phase);
 message(&m,AMIHASS_SUBSCRIBE,"Expired","light.desk",NULL);m.refresh=AMIHASS_QUERY_INTERVAL(30);handle(b,&m);now+=121;refresh_leases(b,now);assert(!active_subscription(&b->subscriptions[a],now));
 message(&m,AMIHASS_SUBSCRIBE,"Bad","light.desk",NULL);m.refresh=AMIHASS_QUERY_INTERVAL(1);handle(b,&m);assert(m.result==AMIHASS_UNSUPPORTED);
 message(&m,AMIHASS_SUBSCRIBE,"Bad","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(b,&m);assert(m.result==AMIHASS_UNSUPPORTED);
 /* Area changes never reuse another area's page or alter its in-flight request. */
 {FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}reload(b);schedule(b,"HASS");complete(b,1);b->job=NULL;
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
 {int i;char id[96];{FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}sample_setting=0;reload(b);b->connected=1;b->network=1;b->network_known=1;b->last_success=now;b->next_network=now+1;
  for(i=0;i<SELECTED;++i){sprintf(id,"light.pool_%d",i);message(&m,AMIHASS_SUBSCRIBE,"Pool",id,NULL);m.msg.mn_ReplyPort=(struct MsgPort *)(unsigned long)(100+i);handle(b,&m);assert(m.result!=AMIHASS_BUSY);}
  message(&m,AMIHASS_SUBSCRIBE,"Pool","light.extra",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)1000;handle(b,&m);assert(m.result==AMIHASS_BUSY);
  for(i=0;i<SELECTED;++i){message(&m,AMIHASS_UNSUBSCRIBE,NULL,NULL,NULL);m.msg.mn_ReplyPort=(struct MsgPort *)(unsigned long)(100+i);handle(b,&m);}
  message(&m,AMIHASS_SUBSCRIBE,"Pool","light.extra",NULL);m.msg.mn_ReplyPort=(struct MsgPort *)1000;handle(b,&m);assert(m.result!=AMIHASS_BUSY);a=subscription(b,"Pool","light.extra");assert(a>=0&&!b->subscriptions[a].value.id[0]);
 }
 {int code;for(code=401;code<=403;code+=2){{FreeVec(b->subscriptions);memset(b,0,sizeof(*b));}sample_setting=0;auto_retry=network_up=1;reload(b);schedule(b,"HASS");b->queued=2;b->last_http=code;b->last_error=code==401 ? AH_TEST_AUTH:AH_TEST_FORBIDDEN;complete(b,-1);b->job=NULL;assert(b->auth_blocked&&!b->connected&&!b->queued);message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!strcmp(m.item.state,AMIHASS_SERVER_AUTH_REJECTED)&&!strcmp(m.items[2].state,"0"));sent_phase=0;now+=36000;message(&m,AMIHASS_LIST,"Fixture",NULL,NULL);m.refresh=1;handle(b,&m);schedule(b,"HASS");assert(!sent_phase&&!b->job);network_up=0;connection_refresh(b,now);network_up=1;now+=100;schedule(b,"HASS");assert(b->auth_blocked&&!b->job);message(&m,AMIHASS_RECONNECT,NULL,NULL,NULL);handle(b,&m);assert(!b->auth_blocked);schedule(b,"HASS");assert(sent_phase==4);complete(b,1);b->job=NULL;assert(b->connected);}}
 {struct Backend isolated;int bad,good,n;memset(&isolated,0,sizeof(isolated));backend_slots(&isolated);network_up=1;sample_setting=0;reload(&isolated);connection_ok(&isolated,now);isolated.network=isolated.network_known=1;isolated.next_network=now+1;
 bad=subscription(&isolated,"Broken","light.missing");good=subscription(&isolated,"Healthy","light.desk");message(&m,AMIHASS_SUBSCRIBE,"Broken","light.missing",NULL);assert(select_lease(&isolated,&m,now,10));message(&m,AMIHASS_SUBSCRIBE,"Healthy","light.desk",NULL);assert(select_lease(&isolated,&m,now,10));isolated.phase=1;isolated.index=bad;isolated.job_generation=isolated.generation;isolated.last_http=404;read_fail=1;complete(&isolated,-1);read_fail=0;assert(isolated.connected&&!isolated.retry&&isolated.subscriptions[bad].failures==1&&isolated.subscriptions[bad].due==now+20);
 message(&m,AMIHASS_QUERY,"Broken","light.missing",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(&isolated,&m);assert(!isolated.subscriptions[bad].once&&isolated.subscriptions[bad].due==now+20);
 isolated.last_http=200;sent_phase=0;schedule(&isolated,"HASS");assert(sent_phase==1&&isolated.index==good);complete(&isolated,1);isolated.job=NULL;assert(isolated.connected&&isolated.subscriptions[good].result==AMIHASS_OK&&!isolated.subscriptions[good].failures);
 message(&m,AMIHASS_QUERY,"Broken","light.missing",NULL);m.refresh=AMIHASS_QUERY_ONCE|AMIHASS_QUERY_FRESH;handle(&isolated,&m);assert(isolated.subscriptions[bad].once&&!isolated.subscriptions[bad].due);
 strcpy(isolated.subscriptions[bad].entity,"light.desk");state_fixture="{\"entity_id\":\"light.desk\",\"state\":\"unavailable\",\"attributes\":{}}";isolated.phase=1;isolated.index=bad;isolated.last_http=200;for(n=0;n<10;++n)complete(&isolated,1);assert(isolated.connected&&isolated.subscriptions[bad].failures==8&&isolated.subscriptions[bad].due==now+2560);state_fixture=NULL;complete(&isolated,1);assert(!isolated.subscriptions[bad].failures&&isolated.subscriptions[bad].due==now+10);
 isolated.catalog_state=AMIHASS_WAIT;isolated.unit_needed=1;isolated.queued=1;strcpy(isolated.queue[0].entity,"light.desk");strcpy(isolated.queue[0].action,"Toggle");isolated.subscriptions[good].urgent=isolated.subscriptions[good].once=1;isolated.subscriptions[good].due=0;isolated.subscriptions[good].seen=now;schedule(&isolated,"HASS");assert(isolated.phase==1&&isolated.index==good&&isolated.queued==1);complete(&isolated,1);isolated.job=NULL;schedule(&isolated,"HASS");assert(isolated.phase==3&&!isolated.queued);complete(&isolated,1);isolated.job=NULL;schedule(&isolated,"HASS");assert(isolated.phase==1);complete(&isolated,1);isolated.job=NULL;
 isolated.subscriptions[bad].once=1;isolated.subscriptions[bad].due=0;isolated.subscriptions[bad].seen=now;schedule(&isolated,"HASS");assert(isolated.phase==1&&isolated.index==bad);complete(&isolated,1);isolated.job=NULL;schedule(&isolated,"HASS");assert(isolated.phase==5);isolated.job=NULL;state_fixture=NULL;
 FreeVec(isolated.subscriptions);}
 {struct Backend limited;int slot;memset(&limited,0,sizeof(limited));backend_slots(&limited);sample_setting=0;reload(&limited);limited.background_budget=1;limited.background_window=now;connection_ok(&limited,now);limited.network=limited.network_known=1;limited.next_network=now+1;slot=subscription(&limited,"Budget","light.desk");message(&m,AMIHASS_SUBSCRIBE,"Budget","light.desk",NULL);select_lease(&limited,&m,now,5);sent_phase=0;schedule(&limited,"HASS");assert(sent_phase==1&&limited.background_used==1);complete(&limited,1);limited.job=NULL;limited.subscriptions[slot].due=0;sent_phase=0;schedule(&limited,"HASS");assert(!sent_phase);limited.catalog_state=AMIHASS_WAIT;schedule(&limited,"HASS");assert(!sent_phase);message(&m,AMIHASS_DIAGNOSTICS,"Budget",NULL,NULL);strcpy(m.domain,"cost");handle(&limited,&m);assert(m.count==9&&!strcmp(m.items[6].state,"1")&&!strcmp(m.items[8].state,"60"));limited.subscriptions[slot].once=1;limited.subscriptions[slot].seen=now;schedule(&limited,"HASS");assert(limited.phase==1&&limited.background_used==1);complete(&limited,1);limited.job=NULL;limited.queued=1;strcpy(limited.queue[0].entity,"light.desk");strcpy(limited.queue[0].action,"Toggle");schedule(&limited,"HASS");assert(limited.phase==3&&limited.background_used==1);complete(&limited,1);limited.job=NULL;schedule(&limited,"HASS");assert(limited.phase==1&&limited.background_used==1);complete(&limited,1);limited.job=NULL;now+=60;limited.subscriptions[slot].due=0;schedule(&limited,"HASS");assert(limited.job&&limited.background_used==1);limited.job=NULL;assert(background_allow(&limited,now-1));limited.background_budget=0;for(slot=0;slot<1000;++slot)assert(background_allow(&limited,now));FreeVec(limited.subscriptions);}
 /* Several due entities share one HTTP request; a missing one fails alone. */
 {struct Backend batch;int x,y,z;memset(&batch,0,sizeof(batch));backend_slots(&batch);sample_setting=0;network_up=1;auto_retry=1;reload(&batch);connection_ok(&batch,now);batch.network=batch.network_known=1;batch.next_network=now+5;
  x=subscription(&batch,"Batch","light.desk");y=subscription(&batch,"Batch","sensor.power");z=subscription(&batch,"Batch","switch.gone");
  message(&m,AMIHASS_SUBSCRIBE,"Batch","light.desk",NULL);assert(select_lease(&batch,&m,now,10));message(&m,AMIHASS_SUBSCRIBE,"Batch","sensor.power",NULL);assert(select_lease(&batch,&m,now,10));message(&m,AMIHASS_SUBSCRIBE,"Batch","switch.gone",NULL);assert(select_lease(&batch,&m,now,10));
  sent_phase=0;schedule(&batch,"HASS");assert(sent_phase==1&&!strcmp(sent_path,"/api/template")&&batch.batch_count==3&&strstr(sent_entity,"light.desk")&&strstr(sent_entity,"sensor.power")&&strstr(sent_entity,"switch.gone")&&batch.diagnostics.poll_http==1);
  assert(subscription(&batch,"Other","light.evict")>=0); /* in-flight batch members stay pinned */
  assert(!strcmp(batch.subscriptions[x].entity,"light.desk")&&!strcmp(batch.subscriptions[y].entity,"sensor.power")&&!strcmp(batch.subscriptions[z].entity,"switch.gone"));
  batch_fixture="[{\"entity_id\":\"sensor.power\",\"state\":\"A very long text sensor state that exceeds the field\",\"attributes\":{\"unit_of_measurement\":\"W\"}},{\"entity_id\":\"light.desk\",\"state\":\"on\",\"attributes\":{\"brightness\":255,\"supported_color_modes\":[\"brightness\"]}},{\"entity_id\":\"switch.gone\",\"missing\":true}]";
  complete(&batch,1);batch.job=NULL;batch_fixture=NULL;
  assert(batch.connected&&batch.subscriptions[x].result==AMIHASS_OK&&batch.subscriptions[x].controls.level==100&&batch.subscriptions[y].result==AMIHASS_OK&&strlen(batch.subscriptions[y].value.state)==39&&!strcmp(batch.subscriptions[y].value.unit,"W")&&batch.subscriptions[z].result==AMIHASS_FAILED&&batch.subscriptions[z].failures==1&&batch.subscriptions[x].due==now+10);
  /* Nothing due: no request. One due entity uses the plain state endpoint. */
  sent_phase=0;schedule(&batch,"HASS");assert(!sent_phase);batch.subscriptions[x].due=0;schedule(&batch,"HASS");assert(sent_phase==1&&!strcmp(sent_path,"/api/states/light.desk")&&batch.batch_count==1);complete(&batch,1);batch.job=NULL;
  /* Held states are read conditionally; "same" keeps the cached value. */
  strcpy(batch.subscriptions[x].value.updated,"2026-11-24T10:00:00.123456+00:00");batch.subscriptions[x].due=batch.subscriptions[y].due=0;schedule(&batch,"HASS");assert(batch.batch_count==2&&!strcmp(sent_path,"/api/template")&&strstr(sent_entity,"2026-11-24T10:00:00.123456+00:00")&&strstr(sent_entity,"same"));
  batch_fixture="[{\"entity_id\":\"light.desk\",\"same\":true},{\"entity_id\":\"sensor.power\",\"state\":\"12\",\"attributes\":{}}]";complete(&batch,1);batch.job=NULL;batch_fixture=NULL;
  assert(batch.subscriptions[x].result==AMIHASS_OK);assert(batch.subscriptions[x].updated==now);assert(!strcmp(batch.subscriptions[x].value.updated,"2026-11-24T10:00:00.123456+00:00"));assert(!strcmp(batch.subscriptions[y].value.state,"12"));
  /* A lone held entity is also read conditionally (one-element answer). */
  batch.subscriptions[x].due=0;schedule(&batch,"HASS");assert(batch.batch_count==1&&!strcmp(sent_path,"/api/template"));batch_fixture=NULL;state_fixture="[{\"entity_id\":\"light.desk\",\"same\":true}]";complete(&batch,1);batch.job=NULL;state_fixture=NULL;assert(batch.subscriptions[x].result==AMIHASS_OK&&batch.subscriptions[x].due==now+10);
  /* A malformed batch answer fails every member but keeps the connection. */
  batch.subscriptions[x].due=batch.subscriptions[y].due=0;schedule(&batch,"HASS");assert(batch.batch_count==2);batch_fixture="[{\"broken\"";complete(&batch,1);batch.job=NULL;batch_fixture=NULL;assert(batch.connected&&batch.subscriptions[x].result==AMIHASS_FAILED&&batch.subscriptions[y].result==AMIHASS_FAILED);
  /* A local start failure (no curl, full T:) never marks the server down. */
  batch.subscriptions[x].due=0;batch.subscriptions[x].failures=0;start_fail=1;batch.start_local=1;batch.start_error=AH_TEST_NO_CURL;sent_phase=0;schedule(&batch,"HASS");start_fail=0;assert(batch.connected&&!batch.retry&&batch.local_retry==now+15&&batch.last_error==AH_TEST_NO_CURL);
  sent_phase=0;schedule(&batch,"HASS");assert(!sent_phase);now+=15;schedule(&batch,"HASS");assert(sent_phase==1);batch.job=NULL;now-=15;FreeVec(batch.subscriptions);}
 /* One message reads several entities; each item carries its own result. */
 {struct Backend many;unsigned long replies;memset(&many,0,sizeof(many));backend_slots(&many);sample_setting=0;network_up=1;reload(&many);connection_ok(&many,now);many.network=many.network_known=1;many.next_network=now+5;
  {int k=subscription(&many,"Multi","light.desk");assert(k>=0);assert(amihass_state("{\"entity_id\":\"light.desk\",\"state\":\"on\",\"attributes\":{}}",&many.subscriptions[k].value));many.subscriptions[k].result=AMIHASS_OK;many.subscriptions[k].updated=now;}
  message(&m,AMIHASS_SUBSCRIBE_MANY,"Multi",NULL,NULL);m.count=3;strcpy(m.items[0].id,"light.desk");strcpy(m.items[1].id,"sensor.power");strcpy(m.items[2].id,"Bad id");replies=many.diagnostics.replies;handle(&many,&m);
  assert(m.result==AMIHASS_OK&&m.count==3&&m.total==1&&many.diagnostics.replies==replies+1);
  assert(AMIHASS_ITEM_RESULT(&m,0)==AMIHASS_OK&&!strcmp(m.items[0].id,"light.desk")&&!strcmp(m.items[0].state,"on"));assert(AMIHASS_ITEM_RESULT(&m,1)==AMIHASS_WAIT&&!strcmp(m.items[1].id,"sensor.power"));assert(AMIHASS_ITEM_RESULT(&m,2)!=AMIHASS_OK);
  {int k,leased=0;for(k=0;k<(int)many.leases;++k)if(many.selections[k].leased&&!strcmp(many.selections[k].owner,"Multi"))++leased;assert(leased==2);}
  message(&m,AMIHASS_QUERY_MANY,"Multi",NULL,NULL);m.count=0;handle(&many,&m);assert(m.result==AMIHASS_UNSUPPORTED);
  message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(&many,&m);assert(m.count==16&&!strcmp(m.items[13].id,AMIHASS_CAP_QUERY_MANY));
  FreeVec(many.subscriptions);}
 /* Entity slots follow free memory; Low memory mode keeps fewer. */
 {struct Backend sized;memset(&sized,0,sizeof(sized));assert(backend_slots(&sized)&&sized.limit==48&&sized.leases==64);sized.low_memory=1;assert(backend_slots(&sized)&&sized.limit==32);sized.low_memory=0;fake_avail=3UL<<20;assert(backend_slots(&sized)&&sized.limit==96&&sized.leases==112);fake_avail=9UL<<20;assert(backend_slots(&sized)&&sized.limit==128);fake_avail=1000000UL;FreeVec(sized.subscriptions);}
 /* When every entity slot is taken the reply says why, and it is counted. */
 {struct Backend full;int k;char id[96];memset(&full,0,sizeof(full));backend_slots(&full);sample_setting=0;network_up=1;reload(&full);connection_ok(&full,now);full.network=full.network_known=1;full.next_network=now+5;
  for(k=0;k<(int)full.limit;++k){sprintf(id,"sensor.slot_%d",k);message(&m,AMIHASS_SUBSCRIBE,"Owner",id,NULL);handle(&full,&m);assert(m.result!=AMIHASS_BUSY);}
  message(&m,AMIHASS_SUBSCRIBE,"Owner","sensor.one_more",NULL);handle(&full,&m);assert(m.result==AMIHASS_BUSY&&!strcmp(m.item.state,AMIHASS_STATE_SELECTION_FULL)&&!strcmp(m.item.id,"sensor.one_more")&&full.diagnostics.refused_full==1);
  message(&m,AMIHASS_QUERY,"Other","sensor.another",NULL);handle(&full,&m);assert(m.result==AMIHASS_BUSY&&!strcmp(m.item.state,AMIHASS_STATE_SELECTION_FULL)&&full.diagnostics.refused_full==2);FreeVec(full.subscriptions);}
 /* Door/cover openings: only the Dashboard (which confirms) unless allowed. */
 {struct Backend guard;int k;const char *lock="{\"entity_id\":\"lock.front\",\"state\":\"locked\",\"attributes\":{\"supported_features\":1}}";memset(&guard,0,sizeof(guard));backend_slots(&guard);sample_setting=0;network_up=1;reload(&guard);connection_ok(&guard,now);guard.network=guard.network_known=1;guard.next_network=now+5;
  k=subscription(&guard,"Script","lock.front");assert(k>=0);assert(amihass_state(lock,&guard.subscriptions[k].value));amihass_controls(lock,&guard.subscriptions[k].controls);guard.subscriptions[k].result=AMIHASS_OK;guard.subscriptions[k].updated=now;
  message(&m,AMIHASS_ACTION,"AmiHASS.ARexx","lock.front","Unlock");handle(&guard,&m);assert(m.result==AMIHASS_UNSUPPORTED&&!strcmp(m.item.state,AMIHASS_STATE_CONFIRMATION)&&!guard.queued);
  message(&m,AMIHASS_ACTION,"AmiHASS.Prefs","lock.front","Open lock");handle(&guard,&m);assert(m.result==AMIHASS_UNSUPPORTED&&!guard.queued);
  message(&m,AMIHASS_ACTION,"AmiHASS.ARexx","lock.front","Lock");handle(&guard,&m);assert(m.result==AMIHASS_WAIT&&guard.queued==1); /* locking is never blocked */
  guard.queued=0;guard.subscriptions[k].result=AMIHASS_OK;message(&m,AMIHASS_ACTION,"AmiHASS.Dashboard","lock.front","Unlock");handle(&guard,&m);assert(m.result==AMIHASS_WAIT&&guard.queued==1);
  guard.queued=0;guard.subscriptions[k].result=AMIHASS_OK;guard.remote_security=1;message(&m,AMIHASS_ACTION,"AmiHASS.ARexx","lock.front","Unlock");handle(&guard,&m);assert(m.result==AMIHASS_WAIT&&guard.queued==1);FreeVec(guard.subscriptions);}
 /* The TCP/IP stack is probed every 5 s while up, not on every call. */
 {struct Backend net;int probes=0;unsigned long t;memset(&net,0,sizeof(net));backend_slots(&net);reload(&net);for(t=0;t<20;++t){unsigned long before=net.next_network;connection_refresh(&net,now+t);if(net.next_network!=before)++probes;}assert(probes==4&&net.network);FreeVec(net.subscriptions);}

 /* A Browser-only preparation, HTTP or malformed catalogue error must not
  * fail healthy selected entities, discard queued actions or loop forever. */
 {int scenario;for(scenario=0;scenario<6;++scenario){struct Backend *x=calloc(1,sizeof(*x));int good;reload(x);x->connected=x->network=x->network_known=1;x->last_success=now;x->next_network=now+5;x->next_health=now+60;
 good=subscription(x,"Dock","light.desk");strcpy(x->subscriptions[good].value.id,"light.desk");strcpy(x->subscriptions[good].value.state,"on");x->subscriptions[good].updated=now;x->subscriptions[good].due=now+60;x->subscriptions[good].result=AMIHASS_OK;
 message(&m,AMIHASS_DEVICES,"Browser",NULL,NULL);m.refresh=1;handle(x,&m);schedule(x,"AmiHASS");assert(x->phase==2);x->queued=2;
 x->last_curl=scenario==0 ? 3:scenario==1 ? 23:scenario==2 ? 63:scenario==3 ? 22:0;x->last_http=scenario==3 ? 400:scenario==4 ? 200:0;
 catalog_fixture="{\"unexpected\":true}";complete(x,scenario>=4 ? 1:-1);x->job=NULL;catalog_fixture=NULL;
 assert(x->connected&&!x->retry&&!x->auth_blocked&&x->queued==2&&x->subscriptions[good].result==AMIHASS_OK&&x->catalog_state==AMIHASS_FAILED);
 message(&m,AMIHASS_DEVICES,"Browser",NULL,NULL);handle(x,&m);assert(m.result==AMIHASS_FAILED&&strstr(m.item.name,"Discovery failed")&&x->catalog_state==AMIHASS_FAILED);
 x->queued=0;message(&m,AMIHASS_QUERY,"Dock","light.desk",NULL);m.refresh=AMIHASS_QUERY_ONCE;handle(x,&m);assert(m.result==AMIHASS_OK);
 message(&m,AMIHASS_DEVICES,"Browser",NULL,NULL);m.refresh=1;handle(x,&m);assert(m.result==AMIHASS_WAIT);x->last_curl=0;x->last_http=200;schedule(x,"AmiHASS");assert(x->phase==2);complete(x,1);x->job=NULL;message(&m,AMIHASS_DEVICES,"Browser",NULL,NULL);handle(x,&m);assert(m.result==AMIHASS_OK);
 FreeVec(x->subscriptions);free(x);}}
 {int slot;unsigned long generation;if(b->subscriptions)FreeVec(b->subscriptions);memset(b,0,sizeof(*b));sample_setting=0;auto_retry=network_up=1;reload(b);slot=subscription(b,"Manual.QA","light.desk");message(&m,AMIHASS_SUBSCRIBE,"Manual.QA","light.desk",NULL);assert(select_lease(b,&m,now,30));b->subscriptions[slot].watching=1;b->subscriptions[slot].watch_seen=now;b->subscriptions[slot].result=AMIHASS_OK;connection_ok(b,now);b->queued=1;strcpy(b->queue[0].action,"Toggle");generation=b->generation;message(&m,AMIHASS_DISCONNECT,NULL,NULL,NULL);handle(b,&m);assert(m.result==AMIHASS_OK&&b->manual_paused&&!b->connected&&!b->queued&&b->generation!=generation&&b->subscriptions[slot].watching&&b->subscriptions[slot].result==AMIHASS_STALE);sent_phase=0;now+=7200;schedule(b,"HASS");assert(!sent_phase&&!b->job);message(&m,AMIHASS_STATUS,NULL,NULL,NULL);handle(b,&m);assert(!m.status.connected&&!strcmp(m.item.state,AMIHASS_SERVER_PAUSED)&&!strcmp(m.items[2].state,"0"));message(&m,AMIHASS_ACTION,"Manual.QA","light.desk","Toggle");handle(b,&m);assert(m.result==AMIHASS_FAILED&&!b->queued);message(&m,AMIHASS_RECONNECT,NULL,NULL,NULL);handle(b,&m);assert(!b->manual_paused);schedule(b,"HASS");assert(sent_phase==4);}
 {int states[4]={0,AMIHASS_WAIT,AMIHASS_OK,AMIHASS_FAILED};const char *labels[4]={"Not requested","Loading devices","Ready","Discovery failed"};int j,was=b->connected;b->sample_mode=0;for(j=0;j<4;++j){b->catalog_state=states[j];message(&m,AMIHASS_DIAGNOSTICS,"QA",NULL,NULL);strcpy(m.domain,"overview");handle(b,&m);assert(m.result==AMIHASS_OK&&m.count==6&&!strcmp(m.items[0].state,labels[j])&&strtoul(m.items[3].state,NULL,10)>=sizeof(*b)&&b->connected==was);}b->sample_mode=1;message(&m,AMIHASS_DIAGNOSTICS,"QA",NULL,NULL);strcpy(m.domain,"overview");handle(b,&m);assert(!strcmp(m.items[0].state,"Sample data"));}
 FreeVec(b->subscriptions);free(b);puts("HASS broker deduplication, batched and conditional polling, action validation/readback, door/cover guard, memory-sized entity limit, multi-entity queries, entity-limit reporting, stale state, local start failures, connection reload and backoff passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp=Path(temporary);(tmp/'exec').mkdir();(tmp/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort; struct Message { unsigned short mn_Length; struct MsgPort *mn_ReplyPort; };\n')
    test=tmp/'test.c';test.write_text(preamble+structs+stubs+functions+main)
    exe=tmp/'test';command=[os.environ.get('CC','cc'),'-std=c99','-g','-Wno-deprecated-declarations','-I'+str(tmp),'-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(test),str(ROOT/'src/core.c'),str(ROOT/'src/camera.c'),str(ROOT/'src/sample.c'),str(ROOT/'src/browser_data.c'),'-o',str(exe)]
    if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
    subprocess.run(command,check=True);subprocess.run([str(exe)],check=True)
