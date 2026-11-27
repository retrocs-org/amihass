/* One REST scheduler for every HASS instance. curl runs in a separate process. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "service.h"
#include "camera.h"
#include "sample.h"
#include "rexx_bridge.h"
#include "amihass_config.h"
#include "ah_log.h"
#include "connectivity.h"
#include "nd_interaction.h"

#include "wait.h"
#include "push_worker.h"
#include "memory_stats.h"
#include "nd_reply.h"
#define FETCH_GUARD "AmiHASS.Fetch"
#define FETCH_PREFIX "T:AmiHASS-"
#define FETCH_PROGRESS_FILE "request.conf"
#include "fetch_worker.h"
#include "http_native.h"
/* Entity slots are sized from free memory at each settings reload. */
#define SELECTED ((int)b->limit)
#define LEASES ((int)b->leases)
#define QUEUED 8
struct Subscription {char owner[20],entity[96];struct AmiHASSEntity value;struct AHControls controls;unsigned long seen,updated,due,watch_seen,delivered;unsigned int interval,failures;int result,watching,once,urgent,pushed;};
struct Action {char entity[96],action[20],code[20];unsigned long ticket;};
struct CatalogCache {int used,count,total,offset,kind;char device[96],area[96],label[64];unsigned long saved;char domain[24],filter[64];struct AmiHASSEntity items[AMIHASS_PAGE];};
struct DiagnosticCounters {unsigned long received,replies,received_bytes,sent_bytes,queries,lists,actions,accepted,action_http,poll_http,catalog_http,http_ok,http_failed,body_bytes,cache_hits,timeouts,invalid,health_http,request_bytes,last_seconds,max_seconds,total_seconds,request_started,refused_full,full_logged;};
/* Recent activity leases, not proof that a client process is still alive. */
struct ClientCounter {char owner[20];unsigned long seen,requests,queries,lists,actions;};
struct ClientSelection {char owner[20],entity[96];unsigned long seen,lease_seen;struct MsgPort *port;unsigned int interval;int leased;};
struct CameraLease {struct MsgPort *port;unsigned long seen;};
struct PressReceipt {struct MsgPort *port;char entity[96];unsigned long ticket,seen;int done;};
struct AHDetail;
struct Backend {int manual_paused;char camera_last_entity[96];unsigned long camera_last_request;struct PressReceipt press_receipts[32];unsigned long press_serial,job_ticket;struct CameraLease camera_clients[16];unsigned long camera_http,camera_failed,camera_bytes,camera_cancels,camera_decode_peak,push_attempts,push_last_update;int push_error;unsigned long camera_serial;struct AHCamera camera;struct MsgPort *push_port;char push_name[80];int push_enabled,push_ready;unsigned long push_seen,push_retry,push_launch,push_updates,push_failures;struct AHMemoryStats memory;struct ClientCounter clients[16];struct ClientSelection *selections;int last_error,last_curl,last_http;struct AHSample sample;int sample_mode;struct DiagnosticCounters diagnostics;struct MsgPort *port,*job;char jobname[80],directory[160],command[1800];unsigned long started,generation,job_generation,last_client,retry,catalog_generation,job_catalog_generation;unsigned long service_started,last_success,next_health,next_network,unit_retry;unsigned long reclaim_due;int low_memory;int auth_blocked,configured,connected,attempted,network,network_known,reconnect;unsigned int retry_interval,background_budget,background_used;unsigned long background_window;int stopping,transferred,phase,index,catalog_state,catalog_count,failures,round;struct AmiHASSConnection connection;struct Subscription *subscriptions;unsigned int limit,leases;struct AmiHASSEntity catalog[AMIHASS_MAX_ENTITIES];struct Action queue[QUEUED];struct CatalogCache pages[4];char temperature_unit[16];int unit_needed,unit_attempted;int prefer_state,queued,catalog_total,catalog_offset,catalog_kind;char catalog_device[96],catalog_area[96],catalog_label[64];char job_action[20];char catalog_domain[24],catalog_filter[64];int batch[AMIHASS_BATCH],batch_count;char token[1025];unsigned long local_retry,lease_at,job_ticks;int lease_dirty,start_local,start_error,remote_security;struct AHDetail *detail[2];int native,native_failures,via_native;unsigned long native_launch;struct AHNote notes[AH_NOTES];int notes_count,notes_known;unsigned long notes_serial,notes_asked;};
/* Monotonic seconds. A wall-clock step (SNTP at boot, a manual change) is
 * absorbed instead of expiring every lease, retry and timeout at once. */
static unsigned long seconds(void)
{static unsigned long mono=1000UL,last;static int started;struct DateStamp d;unsigned long wall;DateStamp(&d);wall=(unsigned long)d.ds_Days*86400UL+(unsigned long)d.ds_Minute*60UL+(unsigned long)d.ds_Tick/50UL;if(started&&wall>=last&&wall-last<=60UL)mono+=wall-last;started=1;last=wall;return mono;}
/* 68000/010: 3, 68020: 2, 68030 and later: 1. TLS handshakes scale with CPU. */
static unsigned int cpu_scale(void)
{UWORD flags=SysBase->AttnFlags;if(!(flags&AFF_68020))return 3;if(!(flags&AFF_68030))return 2;return 1;}
static int present(const char *name){int yes;Forbid();yes=FindPort((STRPTR)name)!=NULL;Permit();return yes;}
/* Inner per-entity messages of a multi-entity request are never replied. */
static struct AmiHASSMessage *fanout_message;
static void reply(struct Backend *b,struct AmiHASSMessage *m)
{if(m==fanout_message)return;if(m->msg.mn_ReplyPort){++b->diagnostics.replies;b->diagnostics.sent_bytes+=m->msg.mn_Length;nd_reply_finish((struct Message *)m);}else FreeVec(m);}
static void job_close(struct Backend *b,int completed)
{if(b->job)fetch_close_port(b->job);if((!b->transferred || completed) && b->directory[0])fetch_clean(b->directory);b->job=NULL;b->directory[0]=0;b->transferred=0;b->phase=0;ah_memory_end(&b->memory,AvailMem(MEMF_FAST));}
static int job_http_status(struct Backend *b)
{char path[200];FILE *f;int code=0;sprintf(path,"%s/fetch.log",b->directory);f=fopen(path,"rb");if(f){if(fscanf(f,"%d",&code)!=1||fgetc(f)!=EOF)code=0;fclose(f);}return code;}
#include "press_receipts.h"
static void (record_http_started)(struct Backend *b,int phase);/* defined below */
static int job_start(struct Backend *b,const char *program,const char *path,const char *body,int phase,int index)
{
 static char config[8192];char token[1025],request[200],tool[260],qtool[540],quoted[1100],self[520],command[1200];struct AmiHASSConnection c;BPTR lock,in,out;LONG rc;FILE *f;int i,ok;unsigned int scale=cpu_scale();
 /* Local problems (no curl, T: full, low memory) are not server outages. */
 b->start_local=1;b->start_error=AH_TEST_PREPARE;
 if(b->job || present(FETCH_GUARD))return 0;
 if(!amihass_network_running()){b->start_local=0;return 0;}
 if(!b->configured || !b->token[0]){b->start_local=0;return 0;}
 /* The token is read once per settings reload, not from disk per request. */
 c=b->connection;strcpy(token,b->token);
 /* curl is only needed when the native worker is not there to take the request. */
 lock=Lock("C:curl",ACCESS_READ);if(lock){rc=NameFromLock(lock,tool,sizeof(tool));UnLock(lock);}else rc=0;if(!rc){if(!(b->native&&!b->low_memory&&present(AH_HTTP_GUARD))){b->start_error=AH_TEST_NO_CURL;amihass_erase(token,sizeof(token));return 0;}strcpy(tool,"C:curl");}
 for(i=0;i<20;++i){sprintf(b->directory,FETCH_PREFIX"%08lx-%08lx-%d",(ULONG)FindTask(NULL),weather_ticks(),i);lock=CreateDir(b->directory);if(lock){UnLock(lock);break;}}
 if(i==20){b->directory[0]=0;b->start_error=AH_TEST_STORAGE;amihass_erase(token,sizeof(token));return 0;}
 sprintf(request,"%s/request.conf",b->directory);f=fopen(request,"wb");ok=0;
 /* No redirects: credentials go only to the configured origin. Config and
  * response are private temporary files; neither credentials nor body are
  * passed on a command line or to the downloader's log. */
 if(f){ok=phase==3 ? amihass_control_request_code(&c,token,body,b->queue[0].action,b->queue[0].code,config,sizeof(config)):amihass_request(&c,token,path,body,config,sizeof(config));if(ok && fputs(config,f)<0)ok=0;if(fclose(f))ok=0;}
 if(ok)b->diagnostics.request_bytes+=(unsigned long)strlen(config);amihass_erase(config,sizeof(config));if(phase==3)amihass_erase(b->queue[0].code,sizeof(b->queue[0].code));
 amihass_erase(token,sizeof(token));if(!ok)goto failed;
 if(!nd_quote_path(qtool,sizeof(qtool),tool) || !nd_quote_path(quoted,sizeof(quoted),request))goto failed;
 sprintf(b->command,"%s --disable --config %s --silent --fail --connect-timeout %u --max-time %u --max-filesize %lu --proto %s --write-out \"%%{http_code}\" --output ",qtool,quoted,10U*scale,30U*scale,(phase==6 ? AH_CAMERA_LIMIT:phase==7 ? AH_HISTORY_LIMIT:AMIHASS_RESPONSE_LIMIT),c.allow_http ? "=http,https":"=https");b->job_ticks=(40UL*scale+10UL)*50UL;
 sprintf(request,"%s/response",b->directory);if(!nd_quote_path(quoted,sizeof(quoted),request) || strlen(b->command)+strlen(quoted)>=sizeof(b->command))goto failed;strcat(b->command,quoted);
 b->job=CreateMsgPort();if(!b->job)goto failed;sprintf(b->jobname,"AmiHASS.Job.%08lx",(ULONG)FindTask(NULL));b->job->mp_Node.ln_Name=b->jobname;AddPort(b->job);
 /* The native worker, when running, keeps the connection open and takes
 * the same request.conf; otherwise curl runs through AmiHASSFetch. */
 b->via_native=0;
 if(b->native&&!b->low_memory){struct FetchMessage *h=AllocVec(sizeof(*h),MEMF_PUBLIC|MEMF_CLEAR);if(h){h->magic=FETCH_MAGIC;h->kind=3;h->msg.mn_Length=sizeof(*h);strcpy(h->directory,b->directory);
  sprintf(h->command,"%s %lu %u %u %d",b->jobname,(phase==6 ? AH_CAMERA_LIMIT:phase==7 ? AH_HISTORY_LIMIT:AMIHASS_RESPONSE_LIMIT),10U*scale,30U*scale,c.allow_http);
  if(fetch_send(AH_HTTP_GUARD,h)){b->via_native=1;b->transferred=1;b->diagnostics.request_started=seconds();ah_memory_begin(&b->memory,AvailMem(MEMF_FAST));record_http_started(b,phase);goto started;}FreeVec(h);}}
 /* Prefer the small AmiHASSFetch helper next to the service; fall back to
 * re-running the service binary in FETCH mode when it is not installed. */
 if(!GetProgramDir() || !NameFromLock(GetProgramDir(),self,sizeof(self)) || !AddPart(self,"AmiHASSFetch",sizeof(self)))goto failed;
 lock=Lock(self,ACCESS_READ);if(lock)UnLock(lock);else if(!NameFromLock(GetProgramDir(),self,sizeof(self)) || !AddPart(self,FilePart((STRPTR)program),sizeof(self)))goto failed;
 if(!nd_quote_path(quoted,sizeof(quoted),self))goto failed;
 sprintf(command,"%s FETCH %s",quoted,b->jobname);in=Open("NIL:",MODE_OLDFILE);out=Open("NIL:",MODE_NEWFILE);if(!in || !out){if(in)Close(in);if(out)Close(out);goto failed;}
 b->diagnostics.request_started=seconds();ah_memory_begin(&b->memory,AvailMem(MEMF_FAST));rc=SystemTags(command,SYS_Input,in,SYS_Output,out,SYS_Asynch,TRUE,NP_StackSize,65536UL,TAG_DONE);if(rc<0){Close(in);Close(out);goto failed;}
 started:b->start_local=0;b->started=weather_ticks();b->job_generation=b->generation;b->job_catalog_generation=b->catalog_generation;b->phase=phase;b->index=index;if(phase==6){strcpy(b->camera_last_entity,b->camera.entity);b->camera_last_request=seconds();}if(phase==3){strcpy(b->job_action,b->queue[0].action);b->job_ticket=b->queue[0].ticket;}return 1;
failed:job_close(b,1);return 0;
}
static char *read_data(struct Backend *b)
{
 FILE *f;char path[200],*p;size_t n;sprintf(path,"%s/response",b->directory);f=fopen(path,"rb");if(!f)return NULL;
 p=AllocVec(AMIHASS_RESPONSE_LIMIT+1,MEMF_PUBLIC|MEMF_CLEAR);if(!p){fclose(f);return NULL;}ah_memory_sample(&b->memory,AvailMem(MEMF_FAST));n=fread(p,1,AMIHASS_RESPONSE_LIMIT,f);b->diagnostics.body_bytes+=(unsigned long)n;
 if(ferror(f) || fgetc(f)!=EOF || memchr(p,0,n)){FreeVec(p);p=NULL;}else p[n]=0;fclose(f);return p;
}
static int cached_page(struct Backend *b,int kind,const char *device,const char *domain,const char *filter,const char *area,const char *label,int offset)
{int i;unsigned long now=seconds();for(i=0;i<4;++i){struct CatalogCache *p=&b->pages[i];if(p->used && (now<p->saved || now-p->saved>60UL))p->used=0;if(p->used && p->kind==kind && !strcmp(p->area,area) && !strcmp(p->label,label) && !strcmp(p->device,device) && p->offset==offset && !strcmp(p->domain,domain) && !strcmp(p->filter,filter))return i;}return -1;}
static void cache_page(struct Backend *b)
{int i,slot=0;for(i=0;i<4;++i){if(!b->pages[i].used){slot=i;break;}if(b->pages[i].saved<b->pages[slot].saved)slot=i;}
 strcpy(b->pages[slot].label,b->catalog_label);strcpy(b->pages[slot].area,b->catalog_area);b->pages[slot].kind=b->catalog_kind;strcpy(b->pages[slot].device,b->catalog_device);b->pages[slot].used=1;b->pages[slot].saved=seconds();b->pages[slot].offset=b->catalog_offset;b->pages[slot].count=b->catalog_count;b->pages[slot].total=b->catalog_total;strcpy(b->pages[slot].domain,b->catalog_domain);strcpy(b->pages[slot].filter,b->catalog_filter);for(i=0;i<b->catalog_count;++i)b->pages[slot].items[i]=b->catalog[i];}
static void connection_failed(struct Backend *b,unsigned long now)
{
 int i;if(b->last_error==AH_TEST_AUTH||b->last_error==AH_TEST_FORBIDDEN)b->auth_blocked=1;ah_log_event(b->reconnect && !b->auth_blocked ? AH_LOG_SERVER_LOST:AH_LOG_RETRY_PAUSED,0);b->connected=0;b->attempted=1;++b->failures;b->retry=now+b->retry_interval;b->next_health=0;b->queued=0;
 for(i=0;i<SELECTED;++i)if(b->subscriptions[i].owner[0]){b->subscriptions[i].result=AMIHASS_FAILED;b->subscriptions[i].once=0;b->subscriptions[i].urgent=0;}
 if(b->catalog_state==AMIHASS_WAIT)b->catalog_state=AMIHASS_FAILED;
}
static void connection_ok(struct Backend *b,unsigned long now)
{if(b->manual_paused)return;if(!b->connected){ah_log_event(AH_LOG_SERVER_CONNECTED,0);if(b->catalog_state==AMIHASS_FAILED)b->catalog_state=0;}b->connected=1;b->attempted=1;b->last_success=now;b->failures=0;b->retry=0;b->next_health=now+60UL;}
static void connection_refresh(struct Backend *b,unsigned long now)
{
 int previous=b->network;if(b->sample_mode||b->manual_paused)return;
 if(!b->network_known || now>=b->next_network || now+5UL<b->next_network){b->network=amihass_network_running();b->network_known=1;b->next_network=now+(b->network ? 5UL:2UL);}
 if(b->configured && !b->network && (previous || !b->attempted)){b->last_error=AH_TEST_NO_NETWORK;connection_failed(b,now);}
 if(b->connected && (now<b->last_success || now-b->last_success>120UL))connection_failed(b,now);
}
static const char *connection_state(struct Backend *b,unsigned long now)
{
 if(b->manual_paused)return AMIHASS_SERVER_PAUSED;
 if(b->sample_mode)return AMIHASS_SERVER_SAMPLE;
 if(!b->configured)return AMIHASS_SERVER_UNCONFIGURED;
 if(!b->network)return AMIHASS_SERVER_NETWORK_DOWN;
 if(b->auth_blocked)return AMIHASS_SERVER_AUTH_REJECTED;
 if(b->connected)return AMIHASS_SERVER_CONNECTED;
 if(b->job && b->phase==4)return AMIHASS_SERVER_CONNECTING;
 if(b->reconnect && b->retry>now)return AMIHASS_SERVER_RETRY_WAIT;
 return b->attempted ? AMIHASS_SERVER_DISCONNECTED:AMIHASS_SERVER_CONNECTING;
}
#include "camera_backend.h"
#include "detail_backend.h"
/* Apply one polled state (json NULL: missing or unreadable) to subscription i. */
static int poll_done(struct Backend *b,int i,const char *json,unsigned long now)
{
 struct Subscription *s=&b->subscriptions[i];struct AmiHASSEntity e;unsigned long delay;unsigned int attempt;int success=0;
 if(json && amihass_state(json,&e) && !strcmp(e.id,s->entity)){success=1;s->value=e;amihass_controls(json,&s->controls);amihass_control_unit(&s->controls,b->temperature_unit);if(s->controls.temperature&&!s->controls.unit[0])b->unit_needed=1;s->updated=now;s->result=AMIHASS_OK;}
 else {ah_log_event(AH_LOG_STATE_FAILED,0);s->result=AMIHASS_FAILED;}
 delay=s->interval ? s->interval:b->connection.poll;if(success&&amihass_available(&s->value))s->failures=0;else if(s->failures<8)++s->failures;for(attempt=0;attempt<s->failures&&delay<3600UL;++attempt)delay=delay>1800UL ? 3600UL:delay*2UL;s->due=now+delay;
 s->once=0;s->urgent=0;return success;
}
/* The server confirmed the held state is current (conditional read). */
static void poll_same(struct Backend *b,int i,unsigned long now)
{struct Subscription *s=&b->subscriptions[i];unsigned long delay=s->interval ? s->interval:b->connection.poll;s->updated=now;s->result=AMIHASS_OK;if(amihass_available(&s->value))s->failures=0;s->due=now+delay;s->once=0;s->urgent=0;}
/* A batched poll answers with a JSON array of state objects; an entity that
 * no longer exists comes back as {"entity_id":...,"missing":true}. */
static void batch_done(struct Backend *b,const char *data,unsigned long now)
{
 static char item[8192];char id[96];const char *p=data,*end;int k,done[AMIHASS_BATCH];unsigned int n;
 for(k=0;k<b->batch_count;++k)done[k]=0;
 if(p){while(*p&&strchr(" \t\r\n",*p))++p;if(*p=='['){++p;for(;;){while(*p&&strchr(" \t\r\n",*p))++p;if(*p!='{')break;end=ah_json_end(p);if(!end)break;n=(unsigned int)(end-p);
   if(n<sizeof(item)){memcpy(item,p,n);item[n]=0;if(ah_json_text(ah_json_member(item,"entity_id"),id,sizeof(id),1))for(k=0;k<b->batch_count;++k)if(!done[k]&&!strcmp(b->subscriptions[b->batch[k]].entity,id)){done[k]=1;if(ah_json_member(item,"same")&&b->subscriptions[b->batch[k]].result==AMIHASS_OK&&b->subscriptions[b->batch[k]].value.updated[0])poll_same(b,b->batch[k],now);else poll_done(b,b->batch[k],ah_json_member(item,"missing") ? NULL:item,now);break;}}
   p=end;while(*p&&strchr(" \t\r\n",*p))++p;if(*p!=',')break;++p;}}}
 for(k=0;k<b->batch_count;++k)if(!done[k])poll_done(b,b->batch[k],NULL,now);
}
static void complete(struct Backend *b,int result)
{
 char *data=NULL;unsigned long now=seconds();int i,count=-1,success=0;
 b->reclaim_due=now+30UL;
 b->diagnostics.last_seconds=now>=b->diagnostics.request_started ? now-b->diagnostics.request_started:0;if(b->diagnostics.last_seconds>b->diagnostics.max_seconds)b->diagnostics.max_seconds=b->diagnostics.last_seconds;b->diagnostics.total_seconds+=b->diagnostics.last_seconds;
 if(result==1&&b->last_http!=401&&b->last_http!=403)++b->diagnostics.http_ok;else ++b->diagnostics.http_failed;
 if(b->job_generation!=b->generation)return;
 if(b->last_http==401||b->last_http==403||b->last_error==AH_TEST_AUTH||b->last_error==AH_TEST_FORBIDDEN){if(b->phase==6){++b->camera_failed;b->camera.pending=0;b->camera.result=AMIHASS_FAILED;}if(b->phase==7&&b->index>=0&&b->index<2&&b->detail[b->index]){b->detail[b->index]->pending=0;b->detail[b->index]->result=AMIHASS_FAILED;}b->last_error=b->last_http==403||b->last_error==AH_TEST_FORBIDDEN ? AH_TEST_FORBIDDEN:AH_TEST_AUTH;connection_failed(b,now);return;}
 if(b->phase==6){camera_complete(b,result,now);return;}
 if(b->phase==7){detail_complete(b,result,now);return;}
 if(result==1)data=read_data(b);

 if(b->phase==1){if(b->batch_count>1||(data&&*data=='['))batch_done(b,data,now);else success=poll_done(b,b->index,data,now);}else if(b->phase==2){if(b->job_catalog_generation!=b->catalog_generation){if(data)FreeVec(data);return;}if(data)count=b->catalog_kind==AMIHASS_LABELS ? amihass_labels_page(data,b->catalog,AMIHASS_MAX_ENTITIES,&b->catalog_total):b->catalog_kind==AMIHASS_AREAS ? amihass_areas_page(data,b->catalog,AMIHASS_MAX_ENTITIES,&b->catalog_total):b->catalog_kind==AMIHASS_DEVICES ? amihass_devices_page(data,b->catalog,AMIHASS_MAX_ENTITIES,&b->catalog_total):amihass_page(data,b->catalog,AMIHASS_MAX_ENTITIES,&b->catalog_total);b->catalog_count=count>0 ? count:0;b->catalog_state=count>=0 ? AMIHASS_OK:AMIHASS_FAILED;if(count<0)ah_log_event(AH_LOG_CATALOG_FAILED,0);if(count>=0){success=1;cache_page(b);}}
 else if(b->phase==3){/* Acceptance isn't a device-state acknowledgement. Re-read the target. */
  i=b->index;if(result!=1)ah_log_event(AH_LOG_ACTION_FAILED,0);if(result==1)success=1;b->subscriptions[i].due=0;b->subscriptions[i].once=result==1;b->subscriptions[i].urgent=result==1;b->subscriptions[i].seen=now;b->subscriptions[i].result=result==1 ? AMIHASS_WAIT:AMIHASS_FAILED;if(!strcmp(b->job_action,"Press"))press_finish(b,b->job_ticket,result==1);if(result==1&&!strcmp(b->job_action,"Press")){++b->subscriptions[i].delivered;b->subscriptions[i].result=AMIHASS_OK;b->subscriptions[i].once=b->subscriptions[i].urgent=0;b->subscriptions[i].updated=now;}
 }
 /* Catalogue/formatting failures are not proof that Home Assistant went
  * offline. Return a terminal page error, preserving healthy entity leases.
  * Real network failures, authentication and gateway outages still back off. */
 if(b->phase==2&&(result==1||b->last_curl==3||b->last_curl==23||b->last_curl==63||(b->last_curl==22&&b->last_http>=400&&b->last_http!=502&&b->last_http!=503&&b->last_http!=504))){
  if(!success)b->last_error=b->last_curl==3 ? AH_TEST_PREPARE:b->last_curl==23 ? AH_TEST_STORAGE:b->last_http>=400 ? ah_connection_result(22,b->last_http,0):AH_TEST_RESPONSE;
  if((result==1||b->last_http>=400)&&b->network)connection_ok(b,now);
  if(data)FreeVec(data);return;
 }
 /* A missing/unavailable entity or malformed state does not disconnect healthy peers. */
 if(b->phase==1&&(result==1||b->last_http==404)){if(b->network)connection_ok(b,now);if(data)FreeVec(data);return;}
 if(b->phase==5){if(data&&amihass_temperature_unit(data,b->temperature_unit)){b->unit_needed=0;for(i=0;i<SELECTED;++i)amihass_control_unit(&b->subscriptions[i].controls,b->temperature_unit);}if(data)FreeVec(data);return;}
 if(b->phase==4)success=data && amihass_api_response(data);
 if(!success&&result==1)b->last_error=AH_TEST_RESPONSE;
 if(success && b->network){b->last_error=0;connection_ok(b,now);}else connection_failed(b,now);
 if(data)FreeVec(data);
}
static void record_http_started(struct Backend *b,int phase)
{b->diagnostics.request_started=seconds();if(phase==6)++b->camera_http;else if(phase==1||phase==7)++b->diagnostics.poll_http;else if(phase==2)++b->diagnostics.catalog_http;else if(phase==3)++b->diagnostics.action_http;else if(phase==4||phase==5)++b->diagnostics.health_http;}
/* A certificate or handshake failure in the native worker (AmiSSL may trust
 * other roots than curl's bundle), or repeated garbled answers, switch this
 * session back to curl; Reconnect or a settings reload tries native again. */
static void native_result(struct Backend *b,int curl)
{if(!curl){b->native_failures=0;return;}
 if(curl==35||curl==60)b->native_failures=3;else if(curl==52||curl==56)++b->native_failures;
 if(b->native_failures>=3){struct FetchMessage *q=AllocVec(sizeof(*q),MEMF_PUBLIC|MEMF_CLEAR);b->native=0;ah_log_event(AH_LOG_DOWNLOAD_FAILED,2000L+curl);
  if(q){q->magic=FETCH_MAGIC;q->kind=4;q->msg.mn_Length=sizeof(*q);if(!fetch_send(AH_HTTP_GUARD,q))FreeVec(q);}}}
/* Use the OS expunge command only after a quiet period. Never unload an
 * in-use library or keep retrying a missing command on every service tick. */
static void reclaim_idle(struct Backend *b)
{
 BPTR input,output;int flushed=0;unsigned long before,now=seconds();
 if(!b->low_memory || !b->reclaim_due || now<b->reclaim_due || b->job || b->queued || b->catalog_state==AMIHASS_WAIT || present(FETCH_GUARD) || present("AmiHASS.Probe"))return;
 b->reclaim_due=0;before=AvailMem(MEMF_FAST);input=Open("NIL:",MODE_OLDFILE);output=Open("NIL:",MODE_NEWFILE);
 if(input && output){LONG rc=SystemTags("C:Avail FLUSH",SYS_Input,input,SYS_Output,output,NP_StackSize,16384UL,TAG_DONE);flushed=rc==0;ah_log_event(AH_LOG_MEMORY_RECLAIM,rc);}
 if(input)Close(input);if(output)Close(output);if(flushed)ah_memory_reclaimed(&b->memory,before,AvailMem(MEMF_FAST));
}
static void job_events(struct Backend *b)
{
 struct FetchMessage *m;int result=0;if(!b->job)return;
 while((m=(struct FetchMessage *)GetMsg(b->job))!=NULL){
  if(m->magic!=FETCH_MAGIC || m->msg.mn_Length!=sizeof(*m)){if(m->msg.mn_ReplyPort)ReplyMsg((struct Message *)m);continue;}
  if(m->kind==1){int handed=0;if(b->transferred)m->kind=-1;else{strcpy(m->command,b->command);strcpy(m->directory,b->directory);b->transferred=handed=1;record_http_started(b,b->phase);}
   /* A worker that gave up waiting has gone: reclaim its packet and fail now. */
   if(nd_reply_finish((struct Message *)m)&&handed){b->transferred=0;b->last_error=AH_TEST_TIMEOUT;result=-1;break;}}
  else if(m->kind==2){int code=job_http_status(b);if(m->result==28)++b->diagnostics.timeouts;b->last_curl=(int)m->result;b->last_http=code;b->last_error=ah_connection_result((int)m->result,code,m->result==0&&code==200);if(m->result!=0||code>=300)ah_log_event(AH_LOG_DOWNLOAD_FAILED,code>=300 ? 1000L+code:(long)m->result);result=m->result==0 ? 1:-1;if(b->via_native)native_result(b,(int)m->result);FreeVec(m);break;}}
 if(result){complete(b,result);job_close(b,1);}else if(weather_ticks()-b->started>(b->job_ticks ? b->job_ticks:2000UL)){++b->diagnostics.timeouts;b->last_error=AH_TEST_TIMEOUT;ah_log_event(AH_LOG_DOWNLOAD_FAILED,28);complete(b,-1);job_close(b,0);}
}
/* Entity slots from free memory: 48 as before on small machines, more where
 * memory allows; Low memory mode keeps 32. Leases get 16 spare entries. */
static int backend_slots(struct Backend *b)
{
 unsigned long avail=AvailMem(MEMF_ANY);unsigned int want=b->low_memory ? 32U:avail>=(8UL<<20) ? 128U:avail>=(2UL<<20) ? 96U:avail>=(1UL<<20) ? 64U:48U;
 if(b->subscriptions&&b->limit==want){memset(b->subscriptions,0,b->limit*sizeof(*b->subscriptions));memset(b->selections,0,b->leases*sizeof(*b->selections));return 1;}
 if(b->subscriptions)FreeVec(b->subscriptions);b->subscriptions=NULL;b->selections=NULL;b->limit=b->leases=0;
 for(;want>=8;want/=2){unsigned int leases=want+16;struct Subscription *slots=AllocVec(want*sizeof(*slots)+leases*sizeof(struct ClientSelection),MEMF_PUBLIC|MEMF_CLEAR);if(slots){b->subscriptions=slots;b->selections=(struct ClientSelection *)(slots+want);b->limit=want;b->leases=leases;return 1;}}
 return 0;
}
static void reload(struct Backend *b)
{struct AmiHASSConfig config;int loaded;b->manual_paused=0;ah_camera_clear(&b->camera);loaded=amihass_config_load(&config);b->last_error=b->last_curl=b->last_http=b->auth_blocked=0;b->camera_last_entity[0]=0;b->camera_last_request=0;memset(b->press_receipts,0,sizeof(b->press_receipts));memset(b->camera_clients,0,sizeof(b->camera_clients));b->push_enabled=config.push_updates;{char how[16];how[0]=0;b->native=!(GetVar("AmiHASS/HTTP",how,sizeof(how),0)>0&&!strcmp(how,"curl"));b->native_failures=0;}b->remote_security=config.remote_security;b->push_ready=0;b->push_retry=0;b->sample_mode=config.sample_mode;ah_sample_init(&b->sample);b->low_memory=config.low_memory;b->reclaim_due=0;b->connection=config.connection;b->configured=loaded && config.connection.url[0] && config.token[0];b->reconnect=config.reconnect;b->background_budget=config.background_budget;b->background_used=0;b->background_window=seconds();b->retry_interval=config.retry_interval;amihass_erase(b->token,sizeof(b->token));if(b->configured)strcpy(b->token,config.token);amihass_erase(&config,sizeof(config));if(!b->configured)b->connection.url[0]=0;b->local_retry=0;b->start_local=0;b->batch_count=0;b->lease_dirty=1;
 ah_log_event(b->configured ? AH_LOG_SETTINGS_RELOADED:AH_LOG_SERVICE_UNCONFIGURED,0);++b->generation;b->prefer_state=1;b->temperature_unit[0]=0;b->unit_needed=b->unit_attempted=0;b->unit_retry=0;b->queued=0;b->catalog_count=0;b->catalog_state=0;b->catalog_kind=0;b->catalog_device[0]=0;b->catalog_area[0]=0;b->retry=0;b->failures=0;b->connected=0;b->attempted=0;b->last_success=0;b->next_health=0;b->network_known=0;if(!backend_slots(b))b->configured=0;memset(b->pages,0,sizeof(b->pages));}
static int subscription(struct Backend *b,const char *owner,const char *entity)
{int i,j,free_slot=-1,reusable=-1,pinned;unsigned long now=seconds();for(i=0;i<SELECTED;++i){struct Subscription *s=&b->subscriptions[i];if(!strcmp(s->entity,entity))return i;pinned=b->job&&(b->phase==1||b->phase==3)&&b->index==i;if(!pinned&&b->job&&b->phase==1&&b->batch_count>1){for(j=0;j<b->batch_count;++j)if(b->batch[j]==i)pinned=1;}for(j=0;j<b->queued&&!pinned;++j)if(!strcmp(b->queue[j].entity,s->entity))pinned=1;if(pinned)continue;if(!s->owner[0]){if(free_slot<0)free_slot=i;}else if(!s->watching&&(!s->once||now-s->seen>10UL)&&!s->urgent&&(reusable<0||s->seen<b->subscriptions[reusable].seen))reusable=i;}if(free_slot<0)free_slot=reusable;if(free_slot<0)return -1;b->lease_dirty=1;memset(&b->subscriptions[free_slot],0,sizeof(b->subscriptions[free_slot]));strcpy(b->subscriptions[free_slot].owner,owner);strcpy(b->subscriptions[free_slot].entity,entity);b->subscriptions[free_slot].result=AMIHASS_WAIT;return free_slot;}
static int active_subscription(struct Subscription *s,unsigned long now)
{return s->owner[0] && ((s->watching && now>=s->watch_seen && now-s->watch_seen<=120UL) || (s->once && now>=s->seen && now-s->seen<=10UL));}
static int client_recent(unsigned long seen,unsigned long now)
{return seen&&now>=seen&&now-seen<=120UL;}
static void refresh_leases(struct Backend *b,unsigned long now)
{int i,j;b->lease_at=now;b->lease_dirty=0;for(i=0;i<SELECTED;++i){struct Subscription *s=&b->subscriptions[i];unsigned int previous=s->interval;s->watching=0;s->watch_seen=0;s->interval=b->connection.poll;for(j=0;j<LEASES;++j){struct ClientSelection *c=&b->selections[j];if(!c->leased||!client_recent(c->lease_seen,now)||strcmp(c->entity,s->entity))continue;{unsigned int desired=c->interval;if(now-c->lease_seen>90UL)desired=desired>900 ? 3600:desired*4;if(!s->watching||desired<s->interval)s->interval=desired;}s->watching=1;if(c->lease_seen>s->watch_seen)s->watch_seen=c->lease_seen;s->seen=now;}if(s->watching&&s->updated&&!s->once&&!s->urgent&&s->result!=AMIHASS_WAIT&&previous!=s->interval&&!s->failures)s->due=s->updated+s->interval;}}
/* Every entity slot is in use: say so instead of a bare BUSY, count it, and
 * log it at most once a minute. */
static void selection_full(struct Backend *b,struct AmiHASSMessage *m,unsigned long now)
{m->result=AMIHASS_BUSY;memset(&m->item,0,sizeof(m->item));strcpy(m->item.id,m->entity);strcpy(m->item.state,AMIHASS_STATE_SELECTION_FULL);++b->diagnostics.refused_full;if(!b->diagnostics.full_logged||now-b->diagnostics.full_logged>=60UL){b->diagnostics.full_logged=now;ah_log_event(AH_LOG_SELECTION_FULL,SELECTED);}}
/* Leases only change with a new second or a selection change. */
static void leases_update(struct Backend *b,unsigned long now)
{if(b->lease_dirty||b->lease_at!=now)refresh_leases(b,now);}
static int select_lease(struct Backend *b,const struct AmiHASSMessage *m,unsigned long now,unsigned int interval)
{int i,slot=-1;struct ClientSelection *c;for(i=0;i<LEASES;++i){c=&b->selections[i];if(c->port==m->msg.mn_ReplyPort&&!strcmp(c->owner,m->owner)&&!strcmp(c->entity,m->entity)){slot=i;break;}if(slot<0&&(!c->leased||!client_recent(c->lease_seen,now)))slot=i;}if(slot<0)return 0;c=&b->selections[slot];strcpy(c->owner,m->owner);strcpy(c->entity,m->entity);c->seen=now;c->port=m->msg.mn_ReplyPort;c->interval=interval;c->lease_seen=now;c->leased=1;refresh_leases(b,now);return 1;}
static void release_lease(struct Backend *b,const struct AmiHASSMessage *m,unsigned long now)
{int i;struct MsgPort *identity=m->msg.mn_ReplyPort ? m->msg.mn_ReplyPort:(struct MsgPort *)(unsigned long)m->age;for(i=0;i<LEASES;++i){struct ClientSelection *c=&b->selections[i];if(c->port==identity&&(!m->entity[0]||!strcmp(c->entity,m->entity)))memset(c,0,sizeof(*c));}refresh_leases(b,now);}
static void client_record(struct Backend *b,const struct AmiHASSMessage *m,unsigned long now)
{int i,slot=-1;struct ClientCounter *c;if(!m->owner[0])return;for(i=0;m->owner[i];++i)if((unsigned char)m->owner[i]<32||(unsigned char)m->owner[i]>126)return;for(i=0;i<16;++i){if(!strcmp(b->clients[i].owner,m->owner)){slot=i;break;}if(slot<0&&!client_recent(b->clients[i].seen,now))slot=i;}if(slot<0)return;c=&b->clients[slot];if(strcmp(c->owner,m->owner)){memset(c,0,sizeof(*c));strcpy(c->owner,m->owner);}c->seen=now;++c->requests;if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE))++c->queries;if(m->kind==AMIHASS_LIST||m->kind==AMIHASS_DEVICES||m->kind==AMIHASS_AREAS)++c->lists;if(m->kind==AMIHASS_ACTION)++c->actions;
 if((m->kind!=AMIHASS_QUERY&&m->kind!=AMIHASS_SUBSCRIBE&&m->kind!=AMIHASS_ACTION)||!amihass_entity_id(m->entity))return;slot=-1;for(i=0;i<LEASES;++i){if(b->selections[i].port==m->msg.mn_ReplyPort&&!strcmp(b->selections[i].owner,m->owner)&&!strcmp(b->selections[i].entity,m->entity)){slot=i;break;}if(slot<0&&!client_recent(b->selections[i].seen,now))slot=i;}if(slot>=0){if(b->selections[slot].port!=m->msg.mn_ReplyPort||strcmp(b->selections[slot].owner,m->owner)||strcmp(b->selections[slot].entity,m->entity)){if(b->selections[slot].leased)b->lease_dirty=1;memset(&b->selections[slot],0,sizeof(b->selections[slot]));}strcpy(b->selections[slot].owner,m->owner);strcpy(b->selections[slot].entity,m->entity);b->selections[slot].seen=now;b->selections[slot].port=m->msg.mn_ReplyPort;}}
static void client_snapshot(struct Backend *b,struct AmiHASSMessage *m,unsigned long now)
{int i,j,index=0;struct AmiHASSEntity item;m->total=m->count=0;if(m->offset<0)m->offset=0;for(i=0;i<16;++i){struct ClientCounter *c=&b->clients[i];if(!client_recent(c->seen,now))continue;memset(&item,0,sizeof(item));strcpy(item.id,c->owner);strcpy(item.name,c->owner);sprintf(item.state,"%lu",c->requests);sprintf(item.unit,"%lu",c->queries);sprintf(item.device_class,"%lu",c->actions);sprintf(item.updated,"%lu",c->lists);if(index>=m->offset&&m->count<AMIHASS_PAGE)m->items[m->count++]=item;++index;
 for(j=0;j<LEASES;++j){struct ClientSelection *e=&b->selections[j];if(strcmp(e->owner,c->owner)||!client_recent(e->seen,now))continue;memset(&item,0,sizeof(item));strcpy(item.id,e->entity);strcpy(item.name,c->owner);strcpy(item.state,"Selected entity");strcpy(item.device_class,"entity");if(index>=m->offset&&m->count<AMIHASS_PAGE)m->items[m->count++]=item;++index;}}m->total=index;m->result=AMIHASS_OK;}
static void diagnostics_snapshot(struct Backend *b,struct AmiHASSMessage *m,unsigned long now)
{
 static const char *keys[]={"ipc.received","ipc.replies","ipc.bytes_in","ipc.bytes_out","ipc.queries","ipc.lists","ipc.actions","actions.accepted","http.actions","http.polls","http.catalogue","http.completed","http.failed","http.body_bytes","cache.hits","http.timeouts","service.uptime","poll.seconds","entities.active","actions.queued","retry.seconds","http.active","cache.pages","ipc.invalid","http.health","entities.limit","entities.refused"};
 static const char *names[]={"Bridge messages received","Bridge replies returned","Bridge bytes received","Bridge bytes returned","Client state queries","Client catalogue queries","Client action requests","Actions accepted into queue","HA action HTTP attempts","HA state-poll HTTP attempts","HA catalogue HTTP attempts","HTTP completions (curl success)","HTTP request failures","Response body bytes read","Cache hits","Downloader timeouts","Service uptime (seconds)","Default poll interval (seconds)","Active selected entities","Actions waiting in queue","Retry backoff (seconds left)","Downloader active (0/1)","Cached catalogue pages","Invalid bridge messages","HA connection/config requests","Selected entity limit","Requests refused: limit reached"};
 unsigned long values[27];int i,index;if(!strcmp(m->domain,"transport")||!strcmp(m->domain,"camera")){static const char *tk[]={"transport.mode","push.watched","push.attempts","push.updates","push.failures","push.update_age","push.error"};static const char *tn[]={"Transport mode","Watched entities","Live connection attempts","Live updates received","Live failures","Last live update age (seconds)","Last live error (0 none, 1 auth, 2 transport)"};static const char *ck[]={"camera.requests","camera.failed","camera.bytes","camera.cache_bytes","camera.watchers","camera.cancelled","camera.decode_peak"};static const char *cn[]={"Camera HTTP attempts","Camera image failures","Camera image bytes received","Encoded image cache bytes","Active camera clients","Queued/discarded camera requests","Reported viewer Fast RAM drop (bytes)"};unsigned long v[7];int camera=!strcmp(m->domain,"camera");memset(v,0,sizeof(v));if(camera){v[0]=b->camera_http;v[1]=b->camera_failed;v[2]=b->camera_bytes;v[3]=b->camera.size;v[4]=camera_watchers(b,now);v[5]=b->camera_cancels;v[6]=b->camera_decode_peak;}else{for(i=0;i<SELECTED;++i)if(b->subscriptions[i].watching)++v[1];v[2]=b->push_attempts;v[3]=b->push_updates;v[4]=b->push_failures;v[5]=b->push_last_update&&now>=b->push_last_update?now-b->push_last_update:0;v[6]=b->push_error;}m->count=m->total=7;m->offset=0;for(i=0;i<7;++i){memset(&m->items[i],0,sizeof(m->items[i]));strcpy(m->items[i].id,camera?ck[i]:tk[i]);strcpy(m->items[i].name,camera?cn[i]:tn[i]);if(!camera&&!i)strcpy(m->items[i].state,b->sample_mode?"Sample":b->push_ready?"WebSocket":b->push_enabled&&!b->low_memory?"REST fallback":"REST");else sprintf(m->items[i].state,"%lu",v[i]);}m->result=AMIHASS_OK;return;}if(!strcmp(m->domain,"cost")){static const char *keys[]={"http.request_bytes","http.last_seconds","http.max_seconds","http.total_seconds","http.body_bytes","memory.request.peak","background.limit","background.used","background.wait_seconds"};static const char *labels[]={"Serialized request bytes (not TLS traffic)","Last HTTP duration (whole seconds)","Longest HTTP duration (whole seconds)","Total HTTP duration (whole seconds)","Response body bytes read","Peak observed HTTP Fast RAM drop","Background attempts per minute (0 unlimited)","Background attempts in current window","Seconds until background budget resets"};unsigned long v[9];v[0]=b->diagnostics.request_bytes;v[1]=b->diagnostics.last_seconds;v[2]=b->diagnostics.max_seconds;v[3]=b->diagnostics.total_seconds;v[4]=b->diagnostics.body_bytes;v[5]=b->memory.peak;v[6]=b->background_budget;v[7]=now>=b->background_window&&now-b->background_window<60UL ? b->background_used:0;v[8]=b->background_budget&&v[7]>=b->background_budget ? 60UL-(now-b->background_window):0;m->count=m->total=9;m->offset=0;for(i=0;i<9;++i){memset(&m->items[i],0,sizeof(m->items[i]));strcpy(m->items[i].id,keys[i]);strcpy(m->items[i].name,labels[i]);sprintf(m->items[i].state,"%lu",v[i]);}m->result=AMIHASS_OK;return;}if(!strcmp(m->domain,"memory")){static const char *keys[]={"memory.fast.baseline","memory.fast.free","memory.fast.minimum","memory.request.before","memory.request.minimum","memory.request.after","memory.request.peak","memory.reclaimed","memory.requests","memory.flushes","memory.fast.largest","leases.active"};static const char *labels[]={"Fast free at service baseline","Fast free now","Lowest observed Fast free","Fast free before last HTTP","Lowest free during last HTTP","Fast free after last HTTP","Largest observed HTTP drop","Fast reclaimed by last flush","HTTP requests measured","Idle flushes measured","Largest free Fast block","Active client/entity leases"};unsigned long v[12];ah_memory_sample(&b->memory,AvailMem(MEMF_FAST));v[0]=b->memory.baseline;v[1]=b->memory.current;v[2]=b->memory.minimum;v[3]=b->memory.before;v[4]=b->memory.request_min;v[5]=b->memory.after;v[6]=b->memory.peak;v[7]=b->memory.reclaimed;v[8]=b->memory.requests;v[9]=b->memory.flushes;v[10]=AvailMem(MEMF_FAST|MEMF_LARGEST);v[11]=0;for(i=0;i<LEASES;++i)if(b->selections[i].leased&&client_recent(b->selections[i].lease_seen,now))++v[11];m->count=m->total=12;m->offset=0;for(i=0;i<12;++i){memset(&m->items[i],0,sizeof(m->items[i]));strcpy(m->items[i].id,keys[i]);strcpy(m->items[i].name,labels[i]);sprintf(m->items[i].state,"%lu",v[i]);}m->result=AMIHASS_OK;return;}if(!strcmp(m->domain,"connection")){static const char *labels[]={"network.running","connection.error","curl.code","http.code"};int codes[4];codes[0]=amihass_network_running();codes[1]=b->last_error;codes[2]=b->last_curl;codes[3]=b->last_http;m->count=m->total=4;m->offset=0;for(i=0;i<4;++i){memset(&m->items[i],0,sizeof(m->items[i]));strcpy(m->items[i].id,labels[i]);sprintf(m->items[i].state,"%d",codes[i]);}m->result=AMIHASS_OK;return;}if(!strcmp(m->domain,"clients")){client_snapshot(b,m,now);return;}
 values[0]=b->diagnostics.received;values[1]=b->diagnostics.replies;values[2]=b->diagnostics.received_bytes;values[3]=b->diagnostics.sent_bytes;
 values[4]=b->diagnostics.queries;values[5]=b->diagnostics.lists;values[6]=b->diagnostics.actions;values[7]=b->diagnostics.accepted;
 values[8]=b->diagnostics.action_http;values[9]=b->diagnostics.poll_http;values[10]=b->diagnostics.catalog_http;values[11]=b->diagnostics.http_ok;
 values[12]=b->diagnostics.http_failed;values[13]=b->diagnostics.body_bytes;values[14]=b->diagnostics.cache_hits;values[15]=b->diagnostics.timeouts;
 values[16]=now-b->service_started;values[17]=b->connection.poll;values[18]=0;values[19]=b->queued;values[20]=!b->auth_blocked && b->retry>now ? b->retry-now:0;values[21]=b->job!=NULL;values[22]=0;values[23]=b->diagnostics.invalid;values[24]=b->diagnostics.health_http;values[25]=SELECTED;values[26]=b->diagnostics.refused_full;
 for(i=0;i<SELECTED;++i)if(active_subscription(&b->subscriptions[i],now))++values[18];
 for(i=0;i<4;++i)if(b->pages[i].used && now>=b->pages[i].saved && now-b->pages[i].saved<=60UL)++values[22];
 if(m->offset<0 || m->offset>=27)m->offset=0;m->total=27;m->count=27-m->offset;if(m->count>AMIHASS_PAGE)m->count=AMIHASS_PAGE;
 for(i=0;i<m->count;++i){index=(int)m->offset+i;memset(&m->items[i],0,sizeof(m->items[i]));strcpy(m->items[i].id,keys[index]);strcpy(m->items[i].name,names[index]);sprintf(m->items[i].state,"%lu",values[index]);}
 m->result=AMIHASS_OK;
}
/* NOTIFICATIONS: the list (entity empty) or one message in 63-character
 * pieces (entity = notification ID). m->age is a serial that changes with
 * every update. They need live updates; REST has no notification API. */
static void notes_request(struct Backend *b,struct AmiHASSMessage *m,unsigned long now)
{int i,k;unsigned int n;memset(&m->item,0,sizeof(m->item));m->age=b->notes_serial;
 if(b->sample_mode){m->result=AMIHASS_OK;return;}
 if(!b->configured){m->result=AMIHASS_UNCONFIGURED;return;}
 if(!b->push_enabled||b->low_memory){m->result=AMIHASS_UNSUPPORTED;strcpy(m->item.state,AMIHASS_STATE_LIVE_UPDATES_OFF);return;}
 b->notes_asked=now;if(!b->notes_known){m->result=AMIHASS_WAIT;return;}
 if(!m->entity[0]){if(m->offset<0||m->offset>AH_NOTES){m->result=AMIHASS_UNSUPPORTED;return;}m->total=b->notes_count;
  for(i=(int)m->offset,k=0;i<b->notes_count&&k<AMIHASS_PAGE;++i,++k){struct AmiHASSEntity *e=&m->items[k];memset(e,0,sizeof(*e));strcpy(e->id,b->notes[i].id);strcpy(e->name,b->notes[i].title);strncpy(e->state,b->notes[i].message,39);strcpy(e->updated,b->notes[i].created);}m->count=k;m->result=AMIHASS_OK;return;}
 for(i=0;i<b->notes_count;++i)if(!strncmp(b->notes[i].id,m->entity,95))break;if(i==b->notes_count){m->result=AMIHASS_UNAVAILABLE;return;}
 n=(unsigned int)strlen(b->notes[i].message);m->total=(LONG)n;for(k=0;k*63U<n&&k<AMIHASS_PAGE;++k){memset(&m->items[k],0,sizeof(m->items[k]));strncpy(m->items[k].name,b->notes[i].message+k*63,63);}
 m->count=k;strcpy(m->item.id,b->notes[i].id);strcpy(m->item.name,b->notes[i].title);strcpy(m->item.updated,b->notes[i].created);m->result=AMIHASS_OK;}
static void handle(struct Backend *b,struct AmiHASSMessage *m)
{
 int i,index;unsigned int interval;char path[96],body[180],area[96],label[64];unsigned long now=seconds();
 ++b->diagnostics.received;b->diagnostics.received_bytes+=m->msg.mn_Length;
 if(m->magic!=AMIHASS_MAGIC || m->abi!=AMIHASS_ABI || m->msg.mn_Length!=sizeof(*m)){++b->diagnostics.invalid;reply(b,m);return;}
 /* Multi-entity requests run each entity through the normal single-entity
  * path (leases, sample mode, caching) and collect the answers. */
 if((m->kind==AMIHASS_QUERY_MANY||m->kind==AMIHASS_SUBSCRIBE_MANY)&&!fanout_message){static struct AmiHASSMessage one;int k,count=(int)m->count,ok=0;
  if(count<1||count>AMIHASS_PAGE||!m->owner[0]){m->result=AMIHASS_UNSUPPORTED;reply(b,m);return;}
  memset(m->action,0,sizeof(m->action));
  for(k=0;k<count;++k){memset(&one,0,sizeof(one));one.msg.mn_ReplyPort=m->msg.mn_ReplyPort;one.msg.mn_Length=m->msg.mn_Length;one.magic=m->magic;one.abi=m->abi;one.kind=m->kind==AMIHASS_QUERY_MANY ? AMIHASS_QUERY:AMIHASS_SUBSCRIBE;one.refresh=m->refresh;strcpy(one.owner,m->owner);strncpy(one.entity,m->items[k].id,sizeof(one.entity)-1);
   fanout_message=&one;handle(b,&one);fanout_message=NULL;--b->diagnostics.received;b->diagnostics.received_bytes-=m->msg.mn_Length;
   m->items[k]=one.item;if(!m->items[k].id[0])strcpy(m->items[k].id,one.entity);m->action[k]=(char)('0'+(one.result>=0&&one.result<=9 ? one.result:AMIHASS_FAILED));if(one.result==AMIHASS_OK)++ok;}
  m->count=count;m->total=ok;m->result=AMIHASS_OK;reply(b,m);return;}
 m->item.name[63]=0;strcpy(label,m->item.name);m->item.id[95]=0;strcpy(area,m->item.id);m->owner[19]=0;m->entity[95]=0;m->action[19]=0;m->domain[23]=0;m->filter[63]=0;m->result=AMIHASS_WAIT;m->count=m->total=0;b->last_client=now;connection_refresh(b,now);
 leases_update(b,now);if(m->kind==AMIHASS_UNSUBSCRIBE){press_release(b,m->msg.mn_ReplyPort ? m->msg.mn_ReplyPort:(struct MsgPort*)(unsigned long)m->age);camera_release(b,m->msg.mn_ReplyPort ? m->msg.mn_ReplyPort:(struct MsgPort*)(unsigned long)m->age,now);release_lease(b,m,now);m->result=AMIHASS_OK;reply(b,m);return;}
 if(m->kind==AMIHASS_SUBSCRIBE&&(((ULONG)m->refresh&~(65535UL<<8))||!m->owner[0]||!amihass_entity_id(m->entity)||(AMIHASS_QUERY_MAX_AGE(m->refresh)&&(AMIHASS_QUERY_MAX_AGE(m->refresh)<5||AMIHASS_QUERY_MAX_AGE(m->refresh)>3600)))){m->result=AMIHASS_UNSUPPORTED;reply(b,m);return;}
 client_record(b,m,now);if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE))++b->diagnostics.queries;else if(m->kind==AMIHASS_LIST || m->kind==AMIHASS_DEVICES || m->kind==AMIHASS_AREAS)++b->diagnostics.lists;else if(m->kind==AMIHASS_ACTION)++b->diagnostics.actions;
 if(m->kind==AMIHASS_CAMERA_STATS){if(m->offset<0||m->offset>(long)AH_CAMERA_LIMIT||m->refresh<0||m->refresh>67108864){m->result=AMIHASS_UNSUPPORTED;}else{if((unsigned long)m->refresh>b->camera_decode_peak)b->camera_decode_peak=(unsigned long)m->refresh;m->result=AMIHASS_OK;}reply(b,m);return;}
 if(m->kind==AMIHASS_DIAGNOSTICS && !strcmp(m->domain,"overview")){
  unsigned long bytes=sizeof(*b)+b->limit*sizeof(*b->subscriptions)+b->leases*sizeof(*b->selections)+b->camera.size;unsigned long cached=0,pages=0;const char *discovery=b->catalog_state==AMIHASS_WAIT ? "Loading devices":b->catalog_state==AMIHASS_FAILED ? "Discovery failed":b->catalog_state==AMIHASS_OK ? "Ready":"Not requested";
  for(i=0;i<SELECTED;++i)if(b->subscriptions[i].value.id[0])++cached;for(i=0;i<4;++i)if(b->pages[i].used)++pages;
  m->count=m->total=6;for(i=0;i<6;++i)memset(&m->items[i],0,sizeof(m->items[i]));
  strcpy(m->items[0].id,"discovery.state");strcpy(m->items[0].state,b->sample_mode ? "Sample data":discovery);if(b->catalog_state==AMIHASS_FAILED)strcpy(m->items[0].name,"Use Refresh to retry; see server logs");
  strcpy(m->items[1].id,"cache.entities");sprintf(m->items[1].state,"%lu",cached);
  strcpy(m->items[2].id,"cache.pages");sprintf(m->items[2].state,"%lu",pages);
  strcpy(m->items[3].id,"memory.tracked");sprintf(m->items[3].state,"%lu",bytes);
  strcpy(m->items[4].id,"requests.pending");sprintf(m->items[4].state,"%u",b->queued+(b->job ? 1:0));
  strcpy(m->items[5].id,"memory.low");sprintf(m->items[5].state,"%d",b->low_memory);m->result=AMIHASS_OK;reply(b,m);return;
 }
 if(m->kind==AMIHASS_DIAGNOSTICS){diagnostics_snapshot(b,m,now);reply(b,m);return;}
 if(b->stopping && m->kind!=AMIHASS_STATUS && m->kind!=AMIHASS_STOP){m->result=AMIHASS_FAILED;reply(b,m);return;}
 if(m->kind==AMIHASS_STATUS){
  memset(&m->status,0,sizeof(m->status));m->status.uptime=now-b->service_started;m->status.last_contact=b->last_success;m->status.connected=!b->manual_paused&&(b->sample_mode || (b->connected && b->network));
  strcpy(m->status.server,b->sample_mode ? "sample://amihass":b->connection.url);m->status.queued=b->queued;
  for(i=0;i<SELECTED;++i)if(active_subscription(&b->subscriptions[i],now))++m->status.subscriptions;
  memset(&m->item,0,sizeof(m->item));strcpy(m->item.state,connection_state(b,now));if(b->last_error<0)strncpy(m->item.name,ah_connection_text(b->last_error),sizeof(m->item.name)-1);m->age=b->last_success && now>=b->last_success ? now-b->last_success:0;
  m->count=m->total=16;for(i=0;i<16;++i)memset(&m->items[i],0,sizeof(m->items[i]));
  strcpy(m->items[13].id,AMIHASS_CAP_QUERY_MANY);strcpy(m->items[13].state,"1");strcpy(m->items[14].id,AMIHASS_CAP_ATTRIBUTES);strcpy(m->items[14].state,"1");strcpy(m->items[15].id,AMIHASS_CAP_HISTORY);strcpy(m->items[15].state,"1");
  strcpy(m->items[0].id,"reconnect.enabled");sprintf(m->items[0].state,"%d",b->reconnect);
  strcpy(m->items[1].id,"retry.interval");sprintf(m->items[1].state,"%u",b->retry_interval);
  strcpy(m->items[2].id,"retry.remaining");sprintf(m->items[2].state,"%lu",b->reconnect && !b->auth_blocked && b->retry>now ? b->retry-now:0);
  strcpy(m->items[3].id,AMIHASS_CAP_AREAS);strcpy(m->items[3].state,"1");
  strcpy(m->items[4].id,AMIHASS_CAP_FILTERS);strcpy(m->items[4].state,"1");
  strcpy(m->items[5].id,AMIHASS_CAP_SUBSCRIPTIONS);strcpy(m->items[5].state,"1");
  strcpy(m->items[6].id,"poll.seconds");sprintf(m->items[6].state,"%u",b->connection.poll);
  strcpy(m->items[7].id,"transport.mode");strcpy(m->items[7].state,b->push_ready ? "WebSocket":b->push_enabled&&!b->low_memory ? "REST fallback":"REST");strcpy(m->items[8].id,"push.updates");sprintf(m->items[8].state,"%lu",b->push_updates);strcpy(m->items[9].id,"push.failures");sprintf(m->items[9].state,"%lu",b->push_failures);
  strcpy(m->items[10].id,AMIHASS_CAP_SNAPSHOTS);strcpy(m->items[10].state,"1");strcpy(m->items[11].id,AMIHASS_CAP_LABELS);strcpy(m->items[11].state,"1");strcpy(m->items[12].id,"memory.low");sprintf(m->items[12].state,"%d",b->low_memory);
  m->result=AMIHASS_OK;reply(b,m);return;
 }
 if(m->kind==AMIHASS_STOP){if(!b->stopping)ah_log_event(AH_LOG_SERVICE_STOPPING,0);b->stopping=1;b->queued=0;m->result=AMIHASS_OK;reply(b,m);return;}
 if(m->kind==AMIHASS_RELOAD){reload(b);m->result=AMIHASS_OK;reply(b,m);return;}
 if(m->kind==AMIHASS_DISCONNECT){
  if(b->sample_mode){m->result=AMIHASS_UNSUPPORTED;reply(b,m);return;}
  b->manual_paused=1;++b->generation;b->connected=b->push_ready=0;b->retry=0;b->attempted=1;b->queued=0;b->last_error=b->last_curl=b->last_http=0;
  amihass_erase(b->queue,sizeof(b->queue));ah_camera_clear(&b->camera);if(b->catalog_state==AMIHASS_WAIT)b->catalog_state=AMIHASS_FAILED;
  for(i=0;i<SELECTED;++i){b->subscriptions[i].pushed=0;b->subscriptions[i].once=b->subscriptions[i].urgent=0;if(b->subscriptions[i].result==AMIHASS_OK||b->subscriptions[i].result==AMIHASS_WAIT)b->subscriptions[i].result=AMIHASS_STALE;}
  for(i=0;i<2;++i)if(b->detail[i]){b->detail[i]->pending=0;b->detail[i]->result=AMIHASS_FAILED;}
  ah_log_event(AH_LOG_RETRY_PAUSED,0);m->result=AMIHASS_OK;reply(b,m);return;
 }
 if(m->kind==AMIHASS_RECONNECT){b->manual_paused=0;b->native_failures=0;{char how[16];how[0]=0;b->native=!(GetVar("AmiHASS/HTTP",how,sizeof(how),0)>0&&!strcmp(how,"curl"));}b->last_error=b->last_curl=b->last_http=0;b->connected=0;b->retry=0;b->next_health=0;b->auth_blocked=0;b->attempted=0;b->push_retry=0;b->network_known=0;m->result=AMIHASS_OK;reply(b,m);return;}
 if(b->manual_paused){m->result=AMIHASS_FAILED;reply(b,m);return;}
 if(m->kind==AMIHASS_SNAPSHOT){camera_request(b,m,now);reply(b,m);return;}
 if(m->kind==AMIHASS_ATTRIBUTES||m->kind==AMIHASS_HISTORY){detail_request(b,m,now);reply(b,m);return;}
 if(m->kind==AMIHASS_NOTIFICATIONS){notes_request(b,m,now);reply(b,m);return;}
 if(b->sample_mode){int receipt=-1,kind=m->kind;if(kind==AMIHASS_ACTION&&!strcmp(m->action,"Press")){receipt=press_reserve(b,m,now);if(receipt<0){m->result=AMIHASS_BUSY;reply(b,m);return;}}if(kind==AMIHASS_SUBSCRIBE)m->kind=AMIHASS_QUERY;ah_sample_request(&b->sample,m,now);m->kind=kind;if(receipt>=0){press_finish(b,b->press_receipts[receipt].ticket,m->result==AMIHASS_WAIT);m->age=b->press_receipts[receipt].ticket;}if((kind==AMIHASS_QUERY||kind==AMIHASS_SUBSCRIBE)&&(!strncmp(m->entity,"button.",7)||!strncmp(m->entity,"input_button.",13))){struct AHControls controls;amihass_controls_items(m->items,m->count,&controls);controls.delivery=press_value(b,m->msg.mn_ReplyPort,m->entity);m->count=amihass_control_items(&controls,m->items,AMIHASS_PAGE);}if(kind==AMIHASS_SUBSCRIBE&&(m->result==AMIHASS_OK||m->result==AMIHASS_UNAVAILABLE)){interval=AMIHASS_QUERY_MAX_AGE(m->refresh);if(!interval)interval=b->connection.poll;index=subscription(b,m->owner,m->entity);if(interval<5||interval>3600||index<0||!select_lease(b,m,now,interval))m->result=AMIHASS_BUSY;}if(m->kind==AMIHASS_ACTION && m->result==AMIHASS_WAIT)++b->diagnostics.accepted;reply(b,m);return;}
 if(((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE) || m->kind==AMIHASS_ACTION) && strstr(m->entity,".amihass_sample_")){m->result=AMIHASS_UNCONFIGURED;reply(b,m);return;}
 if(!b->connection.url[0]){m->result=AMIHASS_UNCONFIGURED;reply(b,m);return;}
 if(m->kind==AMIHASS_LIST || m->kind==AMIHASS_DEVICES || (m->kind==AMIHASS_AREAS||m->kind==AMIHASS_LABELS)){
  int cached;if((m->kind==AMIHASS_AREAS||m->kind==AMIHASS_LABELS)){label[0]=area[0]=m->domain[0]=m->filter[0]=m->entity[0]=0;}if((!amihass_area_valid(area)||!amihass_area_valid(label))){m->result=AMIHASS_FAILED;reply(b,m);return;}if(m->kind==AMIHASS_DEVICES){m->entity[0]=0;}else if(m->entity[0] && !amihass_device_id(m->entity)){m->result=AMIHASS_FAILED;reply(b,m);return;}
  cached=cached_page(b,m->kind,m->entity,m->domain,m->filter,area,label,m->offset);
  if(cached>=0){struct CatalogCache *p=&b->pages[cached];if(m->refresh)p->used=0;else{++b->diagnostics.cache_hits;m->result=AMIHASS_OK;m->count=p->count;m->total=p->total;for(i=0;i<p->count;++i)m->items[i]=p->items[i];reply(b,m);return;}}
  /* An overlapping browser never invalidates another caller's in-flight page. */
  if(b->job && b->phase==2 && (strcmp(area,b->catalog_area) || strcmp(label,b->catalog_label) || m->kind!=b->catalog_kind || strcmp(m->entity,b->catalog_device) || strcmp(m->domain,b->catalog_domain) || strcmp(m->filter,b->catalog_filter) || m->offset!=b->catalog_offset)){reply(b,m);return;}
  if(m->offset<0 || m->offset>100000)m->offset=0;
  if((m->refresh && b->catalog_state!=AMIHASS_WAIT) || strcmp(area,b->catalog_area) || strcmp(label,b->catalog_label) || m->kind!=b->catalog_kind || strcmp(m->entity,b->catalog_device) || strcmp(m->domain,b->catalog_domain) || strcmp(m->filter,b->catalog_filter) || m->offset!=b->catalog_offset){
   strcpy(b->catalog_label,label);strcpy(b->catalog_area,area);b->catalog_kind=m->kind;strcpy(b->catalog_device,m->entity);strcpy(b->catalog_domain,m->domain);strcpy(b->catalog_filter,m->filter);b->catalog_offset=m->offset;++b->catalog_generation;b->catalog_state=AMIHASS_WAIT;b->catalog_count=0;
  }
  if(!b->catalog_state)b->catalog_state=AMIHASS_WAIT;m->result=b->catalog_state;m->total=b->catalog_total;m->count=b->catalog_count;
  for(i=0;i<b->catalog_count;++i)m->items[i]=b->catalog[i];
  if(m->result==AMIHASS_FAILED)strcpy(m->item.name,"Discovery failed - Refresh or see server logs");
 }else if(((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE) || m->kind==AMIHASS_ACTION) && m->owner[0] && amihass_entity_id(m->entity)){
  index=subscription(b,m->owner,m->entity);if(index<0)selection_full(b,m,now);
  else{struct Subscription *s=&b->subscriptions[index];s->seen=now;interval=b->connection.poll;if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE)&&AMIHASS_QUERY_MAX_AGE(m->refresh)){interval=AMIHASS_QUERY_MAX_AGE(m->refresh);if(interval<5||interval>3600){m->result=AMIHASS_UNSUPPORTED;reply(b,m);return;}}
   if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE)){
    if(m->refresh & (AMIHASS_QUERY_ONCE|AMIHASS_QUERY_FRESH)){if((!s->failures||now>=s->due||(m->refresh&AMIHASS_QUERY_FRESH))&&(((m->refresh&AMIHASS_QUERY_FRESH)&&s->result!=AMIHASS_WAIT) || s->result==AMIHASS_FAILED || !s->updated || now<s->updated || now-s->updated>=interval)){s->once=1;s->due=0;if(b->connected)s->result=AMIHASS_WAIT;}}
    else if(!select_lease(b,m,now,interval)){selection_full(b,m,now);reply(b,m);return;}
   }
   m->item=s->value;s->controls.delivery=press_value(b,m->msg.mn_ReplyPort,m->entity);m->count=amihass_control_items(&s->controls,m->items,AMIHASS_PAGE);m->age=s->updated ? now-s->updated:0;m->result=s->result;
   if(s->result==AMIHASS_OK && !amihass_available(&s->value))m->result=AMIHASS_UNAVAILABLE;
   if(s->updated && m->age>interval*2UL+30UL)m->result=AMIHASS_STALE;
   if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE) && m->result==AMIHASS_OK)++b->diagnostics.cache_hits;
   if(m->kind==AMIHASS_ACTION){
    if(!amihass_action(m->entity,m->action,path,body)||!amihass_control_allowed(&s->controls,m->action))m->result=AMIHASS_UNSUPPORTED;
    /* Door and cover openings need the Dashboard's confirmation unless the
     * user allowed them for ARexx, CLI and other clients in Preferences. */
    else if(!b->remote_security&&strcmp(m->owner,"AmiHASS.Dashboard")&&amihass_action_sensitive(m->entity,m->action)){m->result=AMIHASS_UNSUPPORTED;strcpy(m->item.state,AMIHASS_STATE_CONFIRMATION);ah_log_event(AH_LOG_ACTION_BLOCKED,0);}
    else if(s->result!=AMIHASS_OK || m->result==AMIHASS_STALE)m->result=AMIHASS_STALE;
    else if(!amihass_available(&s->value))m->result=AMIHASS_UNAVAILABLE;
    else if(!b->connected || !b->network || now<b->retry)m->result=AMIHASS_FAILED;
    else if(b->queued>=QUEUED)m->result=AMIHASS_BUSY;
    else if(!strncmp(m->entity,"alarm_control_panel.",20)&&s->controls.alarm==2&&!amihass_alarm_code(m->filter)){/* the panel asks for a code */m->result=AMIHASS_UNSUPPORTED;strcpy(m->item.state,AMIHASS_STATE_CODE_REQUIRED);reply(b,m);return;}
    else{int receipt=-1;if(!strcmp(m->action,"Press")){receipt=press_reserve(b,m,now);if(receipt<0){m->result=AMIHASS_BUSY;reply(b,m);return;}b->queue[b->queued].ticket=b->press_receipts[receipt].ticket;m->age=b->queue[b->queued].ticket;}else b->queue[b->queued].ticket=0;++b->diagnostics.accepted;/* An alarm code (SDK: the filter field) is held only until the request is written. */
     memset(b->queue[b->queued].code,0,sizeof(b->queue[0].code));if(!strncmp(m->entity,"alarm_control_panel.",20)&&amihass_alarm_code(m->filter))strcpy(b->queue[b->queued].code,m->filter);amihass_erase(m->filter,sizeof(m->filter));
     strcpy(b->queue[b->queued].entity,m->entity);strcpy(b->queue[b->queued++].action,m->action);m->result=AMIHASS_WAIT;s->result=AMIHASS_WAIT;}
   }
  }
 }else m->result=AMIHASS_UNCONFIGURED;reply(b,m);
}
static int background_allow(struct Backend *b,unsigned long now)
{if(now<b->background_window||now-b->background_window>=60UL){b->background_window=now;b->background_used=0;}if(!b->background_budget)return 1;if(b->background_used>=b->background_budget)return 0;++b->background_used;return 1;}
#include "push_backend.h"
/* A start that failed locally waits briefly; a network failure backs off. */
static void start_failed(struct Backend *b,unsigned long now)
{if(b->start_local){b->start_local=0;b->local_retry=now+15UL;b->last_error=b->start_error;ah_log_event(b->start_error==AH_TEST_NO_CURL ? AH_LOG_TEST_NO_CURL:AH_LOG_TEST_PREPARE,0);if(b->catalog_state==AMIHASS_WAIT&&b->phase==2)b->catalog_state=AMIHASS_FAILED;return;}connection_failed(b,now);}
static int poll_wanted(struct Backend *b,struct Subscription *s,unsigned long now)
{return active_subscription(s,now)&&now>=s->due&&(s->urgent||s->once||!(s->pushed&&b->push_ready));}
/* Read `first` plus every other due selected entity in one HTTP request. */
static int start_poll(struct Backend *b,const char *program,unsigned long now,int first)
{
 static char body[4096];static char ids[AMIHASS_BATCH][96],known[AMIHASS_BATCH][40];char path[160];int i,ok,conditional=0;
 b->batch[0]=first;b->batch_count=1;
 for(i=1;i<SELECTED&&b->batch_count<AMIHASS_BATCH;++i){int candidate=(first+i)%SELECTED;if(poll_wanted(b,&b->subscriptions[candidate],now))b->batch[b->batch_count++]=candidate;}
 /* Entities with a held state are read conditionally: unchanged ones cost a
  * few bytes instead of their full state. */
 for(i=0;i<b->batch_count;++i){struct Subscription *s=&b->subscriptions[b->batch[i]];strcpy(ids[i],s->entity);known[i][0]=0;if(s->result==AMIHASS_OK&&s->value.updated[0]){strcpy(known[i],s->value.updated);conditional=1;}}
 if(b->batch_count==1&&!conditional){sprintf(path,"/api/states/%s",b->subscriptions[first].entity);ok=job_start(b,program,path,NULL,1,first);}
 else{ok=amihass_states_request_known((const char (*)[96])ids,(const char (*)[40])known,(unsigned int)b->batch_count,body,sizeof(body))&&job_start(b,program,"/api/template",body,1,first);}
 if(!ok){for(i=0;i<b->batch_count;++i)b->subscriptions[b->batch[i]].result=AMIHASS_FAILED;b->batch_count=0;start_failed(b,now);}
 b->round=(first+1)%SELECTED;return ok;
}
/* Start the native HTTP worker (the service binary in HTTP mode) when it is
 * wanted and not running; at most every 30 seconds. */
static void native_step(struct Backend *b,const char *program,unsigned long now)
{char quoted[540],command[620];BPTR in,out;LONG rc;
 if(b->manual_paused){if(present(AH_HTTP_GUARD)){struct FetchMessage *q=AllocVec(sizeof(*q),MEMF_PUBLIC|MEMF_CLEAR);if(q){q->magic=FETCH_MAGIC;q->kind=4;q->msg.mn_Length=sizeof(*q);if(!fetch_send(AH_HTTP_GUARD,q))FreeVec(q);}}return;}
 if(!b->native||b->low_memory||b->sample_mode||!b->configured||b->stopping||present(AH_HTTP_GUARD)||(b->native_launch&&now>=b->native_launch&&now-b->native_launch<30UL))return;
 b->native_launch=now;if(!nd_quote_path(quoted,sizeof(quoted),program))return;sprintf(command,"%s HTTP "AMIHASS_PORT,quoted);
 in=Open("NIL:",MODE_OLDFILE);out=Open("NIL:",MODE_NEWFILE);if(!in||!out){if(in)Close(in);if(out)Close(out);return;}
 rc=SystemTags(command,SYS_Input,in,SYS_Output,out,SYS_Asynch,TRUE,NP_StackSize,65536UL,NP_Priority,0L,TAG_DONE);if(rc<0){Close(in);Close(out);}}
static void schedule(struct Backend *b,const char *program)
{
 unsigned long now=seconds();int i,index;char path[160],body[180],catalogue[4096];
 leases_update(b,now);if(b->camera.data&&!b->camera.pending&&now>=b->camera.touched&&now-b->camera.touched>(b->low_memory?5UL:30UL))ah_camera_clear(&b->camera);native_step(b,program,now);detail_reclaim(b,now);if(b->sample_mode||b->manual_paused)return;
 connection_refresh(b,now);push_step(b,program,now);
 if(b->auth_blocked || b->stopping || b->job || present(FETCH_GUARD) || !b->configured || !b->network || now<b->retry || now<b->local_retry)return;
 if(!b->connected){
  if(b->attempted && !b->reconnect)return;b->attempted=1;
  if(!job_start(b,program,"/api/",NULL,4,-1))start_failed(b,now);return;
 }
 for(i=0;i<SELECTED;++i){int candidate=(b->round+i)%SELECTED;struct Subscription *target=&b->subscriptions[candidate];if(target->urgent&&active_subscription(target,now)&&now>=target->due){start_poll(b,program,now,candidate);return;}}
 if(b->queued){
  for(index=0;index<SELECTED;++index)if(!strcmp(b->subscriptions[index].entity,b->queue[0].entity))break;
  if(index<SELECTED && amihass_action(b->queue[0].entity,b->queue[0].action,path,body)){
   if(!job_start(b,program,path,b->queue[0].entity,3,index)){if(b->start_local){b->subscriptions[index].result=AMIHASS_FAILED;--b->queued;memmove(b->queue,b->queue+1,(unsigned int)b->queued*sizeof(b->queue[0]));amihass_erase(&b->queue[b->queued],sizeof(b->queue[0]));}else b->subscriptions[index].result=AMIHASS_FAILED;start_failed(b,now);return;}
  }
  --b->queued;memmove(b->queue,b->queue+1,(unsigned int)b->queued*sizeof(b->queue[0]));amihass_erase(&b->queue[b->queued],sizeof(b->queue[0]));return;
 }
 for(i=0;i<SELECTED;++i){int candidate=(b->round+i)%SELECTED;struct Subscription *target=&b->subscriptions[candidate];if(target->once&&active_subscription(target,now)&&now>=target->due){start_poll(b,program,now,candidate);return;}}
 if(b->camera.pending){if(!ah_camera_path_size(b->camera.entity,b->camera.small,path,sizeof(path))||!job_start(b,program,path,NULL,6,-1)){b->camera.pending=0;b->camera.result=AMIHASS_FAILED;}return;}
 if(detail_start(b,program,now))return;
 if(b->unit_needed&&(!b->unit_attempted||now>=b->unit_retry)){if(!background_allow(b,now))return;b->unit_attempted=1;b->unit_retry=now+60;job_start(b,program,"/api/config",NULL,5,-1);return;}
 index=-1;
 for(i=0;i<SELECTED;++i){int candidate=(b->round+i)%SELECTED;struct Subscription *s=&b->subscriptions[candidate];
  if(active_subscription(s,now)&&!(s->pushed&&b->push_ready)&&now>=s->due){if(index<0)index=candidate;if(s->urgent){index=candidate;break;}}}
 if(index>=0&&(b->subscriptions[index].urgent||b->catalog_state!=AMIHASS_WAIT||b->prefer_state)){if(!background_allow(b,now))return;
  start_poll(b,program,now,index);b->prefer_state=0;return;
 }
 if(b->catalog_state==AMIHASS_WAIT){if(!background_allow(b,now))return;if(!(b->catalog_kind==AMIHASS_LABELS ? amihass_labels_request(b->catalog_offset,catalogue,sizeof(catalogue)):b->catalog_kind==AMIHASS_AREAS ? amihass_areas_request(b->catalog_offset,catalogue,sizeof(catalogue)):b->catalog_kind==AMIHASS_DEVICES ? amihass_devices_scope_request(b->catalog_area,b->catalog_label,b->catalog_domain,b->catalog_filter,b->catalog_offset,catalogue,sizeof(catalogue)):amihass_device_scope_page_request(b->catalog_device,b->catalog_area,b->catalog_label,b->catalog_domain,b->catalog_filter,b->catalog_offset,catalogue,sizeof(catalogue)))){b->catalog_state=AMIHASS_FAILED;b->prefer_state=1;return;}if(!job_start(b,program,"/api/template",catalogue,2,-1)){b->catalog_state=AMIHASS_FAILED;start_failed(b,now);}b->prefer_state=1;return;}
 if(!b->push_ready&&now>=b->next_health){if(!background_allow(b,now))return;if(!job_start(b,program,"/api/",NULL,4,-1))start_failed(b,now);return;}

}
static int backend_run(const char *program,int restoring)
{
 struct Backend *b;struct AmiHASSMessage *m;struct AHWait sleeper;struct AHRexx rexx;int owner=0;
 ((struct Process *)FindTask(NULL))->pr_WindowPtr=(APTR)-1;
 if(!restoring && present("AmiHASS.Maintenance"))return 20;
 b=AllocVec(sizeof(*b),MEMF_PUBLIC|MEMF_CLEAR);if(!b)return 20;b->port=CreateMsgPort();if(!b->port){FreeVec(b);return 20;}
 Forbid();if(!FindPort(AMIHASS_PORT)){b->port->mp_Node.ln_Name=AMIHASS_PORT;AddPort(b->port);owner=1;}Permit();if(!owner){DeleteMsgPort(b->port);FreeVec(b);return 0;}
 /* The boot sequence copies ENVARC: to RAM ENV:, including AmiHASS.cfg and
  * the token; nothing reads those copies, so do not leave them in RAM. */
 amihass_config_scrub_env();
 b->push_port=CreateMsgPort();if(b->push_port){sprintf(b->push_name,"AmiHASS.Push.%08lx",(ULONG)FindTask(NULL));b->push_port->mp_Node.ln_Name=b->push_name;AddPort(b->push_port);}
 ah_log_event(AH_LOG_SERVICE_STARTED,0);
 memset(&sleeper,0,sizeof(sleeper));ah_rexx_open(&rexx);reload(b);b->last_client=seconds();b->service_started=seconds();
 for(;;){ah_memory_sample(&b->memory,AvailMem(MEMF_FAST));ah_rexx_step(&rexx);while((m=(struct AmiHASSMessage *)GetMsg(b->port))!=NULL)handle(b,m);push_events(b);job_events(b);schedule(b,program);reclaim_idle(b);if(b->stopping || (SetSignal(0L,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C))break;
  ah_wait(&sleeper,ah_rexx_signals(&rexx)|(b->push_port ? 1UL<<b->push_port->mp_SigBit:0)|(1UL<<b->port->mp_SigBit)|(b->job ? 1UL<<b->job->mp_SigBit:0));}
 b->stopping=1;if(b->push_port){Forbid();RemPort(b->push_port);Permit();push_events(b);DeleteMsgPort(b->push_port);b->push_port=NULL;}
 ah_rexx_close(&rexx);Forbid();RemPort(b->port);Permit();while((m=(struct AmiHASSMessage *)GetMsg(b->port))!=NULL){m->result=AMIHASS_FAILED;reply(b,m);}DeleteMsgPort(b->port);job_close(b,0);ah_wait_close(&sleeper);ah_camera_clear(&b->camera);detail_free(b,0);detail_free(b,1);amihass_erase(b->token,sizeof(b->token));if(b->subscriptions)FreeVec(b->subscriptions);FreeVec(b);ah_log_event(AH_LOG_SERVICE_STOPPED,0);return 0;
}
int amihass_backend(const char *program){return backend_run(program,0);}
int amihass_backend_restore(const char *program){return backend_run(program,1);}
int amihass_worker(const char *parent)
{if(!parent || strncmp(parent,"AmiHASS.Job.",12) || strlen(parent)>70)return 20;return fetch_worker(parent);}
