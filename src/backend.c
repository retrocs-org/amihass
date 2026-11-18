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
#include "sample.h"
#include "rexx_bridge.h"
#include "amihass_config.h"
#include "ah_log.h"
#include "connectivity.h"
#include "nd_interaction.h"

#include "wait.h"
#include "memory_stats.h"
#include "nd_reply.h"
#define FETCH_GUARD "AmiHASS.Fetch"
#define FETCH_PREFIX "T:AmiHASS-"
#define FETCH_PROGRESS_FILE "request.conf"
#include "fetch_worker.h"
#define SELECTED 48
#define QUEUED 8
struct Subscription {char owner[20],entity[96];struct AmiHASSEntity value;struct AHControls controls;unsigned long seen,updated,due,watch_seen;unsigned int interval;int result,watching,once,urgent;};
struct Action {char entity[96],action[20];};
struct CatalogCache {int used,count,total,offset,kind;char device[96],area[96];unsigned long saved;char domain[24],filter[64];struct AmiHASSEntity items[AMIHASS_PAGE];};
struct DiagnosticCounters {unsigned long received,replies,received_bytes,sent_bytes,queries,lists,actions,accepted,action_http,poll_http,catalog_http,http_ok,http_failed,body_bytes,cache_hits,timeouts,invalid,health_http;};
/* Recent activity leases, not proof that a client process is still alive. */
struct ClientCounter {char owner[20];unsigned long seen,requests,queries,lists,actions;};
struct ClientSelection {char owner[20],entity[96];unsigned long seen,lease_seen;struct MsgPort *port;unsigned int interval;int leased;};
struct Backend {struct AHMemoryStats memory;struct ClientCounter clients[16];struct ClientSelection selections[64];int last_error,last_curl,last_http;struct AHSample sample;int sample_mode;struct DiagnosticCounters diagnostics;struct MsgPort *port,*job;char jobname[80],directory[160],command[1800];unsigned long started,generation,job_generation,last_client,retry,catalog_generation,job_catalog_generation;unsigned long service_started,last_success,next_health,next_network,unit_retry;unsigned long reclaim_due;int low_memory;int configured,connected,attempted,network,network_known,reconnect;unsigned int retry_interval;int stopping,transferred,phase,index,catalog_state,catalog_count,failures,round;struct AmiHASSConnection connection;struct Subscription subscriptions[SELECTED];struct AmiHASSEntity catalog[AMIHASS_MAX_ENTITIES];struct Action queue[QUEUED];struct CatalogCache pages[4];char temperature_unit[16];int unit_needed,unit_attempted;int prefer_state,queued,catalog_total,catalog_offset,catalog_kind;char catalog_device[96],catalog_area[96];char catalog_domain[24],catalog_filter[64];};
static unsigned long seconds(void){struct DateStamp d;DateStamp(&d);return (unsigned long)d.ds_Days*86400UL+(unsigned long)d.ds_Minute*60UL+(unsigned long)d.ds_Tick/50UL;}
static int present(const char *name){int yes;Forbid();yes=FindPort((STRPTR)name)!=NULL;Permit();return yes;}
static void reply(struct Backend *b,struct AmiHASSMessage *m)
{if(m->msg.mn_ReplyPort){++b->diagnostics.replies;b->diagnostics.sent_bytes+=m->msg.mn_Length;nd_reply_finish((struct Message *)m);}else FreeVec(m);}
static void job_close(struct Backend *b,int completed)
{if(b->job)fetch_close_port(b->job);if((!b->transferred || completed) && b->directory[0])fetch_clean(b->directory);b->job=NULL;b->directory[0]=0;b->transferred=0;b->phase=0;ah_memory_end(&b->memory,AvailMem(MEMF_FAST));}
static int job_http_status(struct Backend *b)
{char path[200];FILE *f;int code=0;sprintf(path,"%s/fetch.log",b->directory);f=fopen(path,"rb");if(f){if(fscanf(f,"%d",&code)!=1||fgetc(f)!=EOF)code=0;fclose(f);}return code;}
static int job_start(struct Backend *b,const char *program,const char *path,const char *body,int phase,int index)
{
 char token[1025],config[8192],request[200],tool[260],qtool[540],quoted[1100],self[520],command[1200];struct AmiHASSConnection c;BPTR lock,in,out;LONG rc;FILE *f;int i,ok;
 if(b->job || present(FETCH_GUARD) || !amihass_network_running() || !amihass_connection_load(&c,token))return 0;
 lock=Lock("C:curl",ACCESS_READ);if(!lock){amihass_erase(token,sizeof(token));return 0;}rc=NameFromLock(lock,tool,sizeof(tool));UnLock(lock);if(!rc){amihass_erase(token,sizeof(token));return 0;}
 for(i=0;i<20;++i){sprintf(b->directory,FETCH_PREFIX"%08lx-%08lx-%d",(ULONG)FindTask(NULL),weather_ticks(),i);lock=CreateDir(b->directory);if(lock){UnLock(lock);break;}}
 if(i==20){b->directory[0]=0;amihass_erase(token,sizeof(token));return 0;}
 sprintf(request,"%s/request.conf",b->directory);f=fopen(request,"wb");ok=0;
 /* No redirects: credentials go only to the configured origin. Config and
  * response are private temporary files; neither credentials nor body are
  * passed on a command line or to the downloader's log. */
 if(f){ok=phase==3 ? amihass_control_request(&c,token,body,b->queue[0].action,config,sizeof(config)):amihass_request(&c,token,path,body,config,sizeof(config));if(ok && fputs(config,f)<0)ok=0;if(fclose(f))ok=0;}
 amihass_erase(config,sizeof(config));
 amihass_erase(token,sizeof(token));if(!ok)goto failed;
 if(!nd_quote_path(qtool,sizeof(qtool),tool) || !nd_quote_path(quoted,sizeof(quoted),request))goto failed;
 sprintf(b->command,"%s --disable --config %s --silent --fail --connect-timeout 10 --max-time 30 --max-filesize %lu --proto %s --write-out \"%%{http_code}\" --output ",qtool,quoted,AMIHASS_RESPONSE_LIMIT,c.allow_http ? "=http,https":"=https");
 sprintf(request,"%s/response",b->directory);if(!nd_quote_path(quoted,sizeof(quoted),request) || strlen(b->command)+strlen(quoted)>=sizeof(b->command))goto failed;strcat(b->command,quoted);
 b->job=CreateMsgPort();if(!b->job)goto failed;sprintf(b->jobname,"AmiHASS.Job.%08lx",(ULONG)FindTask(NULL));b->job->mp_Node.ln_Name=b->jobname;AddPort(b->job);
 if(!GetProgramDir() || !NameFromLock(GetProgramDir(),self,sizeof(self)) || !AddPart(self,FilePart((STRPTR)program),sizeof(self)) || !nd_quote_path(quoted,sizeof(quoted),self))goto failed;
 sprintf(command,"%s FETCH %s",quoted,b->jobname);in=Open("NIL:",MODE_OLDFILE);out=Open("NIL:",MODE_NEWFILE);if(!in || !out){if(in)Close(in);if(out)Close(out);goto failed;}
 ah_memory_begin(&b->memory,AvailMem(MEMF_FAST));rc=SystemTags(command,SYS_Input,in,SYS_Output,out,SYS_Asynch,TRUE,NP_StackSize,65536UL,TAG_DONE);if(rc<0){Close(in);Close(out);goto failed;}
 b->started=weather_ticks();b->job_generation=b->generation;b->job_catalog_generation=b->catalog_generation;b->phase=phase;b->index=index;return 1;
failed:job_close(b,1);return 0;
}
static char *read_data(struct Backend *b)
{
 FILE *f;char path[200],*p;size_t n;sprintf(path,"%s/response",b->directory);f=fopen(path,"rb");if(!f)return NULL;
 p=AllocVec(AMIHASS_RESPONSE_LIMIT+1,MEMF_PUBLIC|MEMF_CLEAR);if(!p){fclose(f);return NULL;}ah_memory_sample(&b->memory,AvailMem(MEMF_FAST));n=fread(p,1,AMIHASS_RESPONSE_LIMIT,f);b->diagnostics.body_bytes+=(unsigned long)n;
 if(ferror(f) || fgetc(f)!=EOF || memchr(p,0,n)){FreeVec(p);p=NULL;}else p[n]=0;fclose(f);return p;
}
static int cached_page(struct Backend *b,int kind,const char *device,const char *domain,const char *filter,const char *area,int offset)
{int i;unsigned long now=seconds();for(i=0;i<4;++i){struct CatalogCache *p=&b->pages[i];if(p->used && (now<p->saved || now-p->saved>60UL))p->used=0;if(p->used && p->kind==kind && !strcmp(p->area,area) && !strcmp(p->device,device) && p->offset==offset && !strcmp(p->domain,domain) && !strcmp(p->filter,filter))return i;}return -1;}
static void cache_page(struct Backend *b)
{int i,slot=0;for(i=0;i<4;++i){if(!b->pages[i].used){slot=i;break;}if(b->pages[i].saved<b->pages[slot].saved)slot=i;}
 strcpy(b->pages[slot].area,b->catalog_area);b->pages[slot].kind=b->catalog_kind;strcpy(b->pages[slot].device,b->catalog_device);b->pages[slot].used=1;b->pages[slot].saved=seconds();b->pages[slot].offset=b->catalog_offset;b->pages[slot].count=b->catalog_count;b->pages[slot].total=b->catalog_total;strcpy(b->pages[slot].domain,b->catalog_domain);strcpy(b->pages[slot].filter,b->catalog_filter);for(i=0;i<b->catalog_count;++i)b->pages[slot].items[i]=b->catalog[i];}
static void connection_failed(struct Backend *b,unsigned long now)
{
 int i;ah_log_event(b->reconnect ? AH_LOG_SERVER_LOST:AH_LOG_RETRY_PAUSED,0);b->connected=0;b->attempted=1;++b->failures;b->retry=now+b->retry_interval;b->next_health=0;b->queued=0;
 for(i=0;i<SELECTED;++i)if(b->subscriptions[i].owner[0]){b->subscriptions[i].result=AMIHASS_FAILED;b->subscriptions[i].once=0;b->subscriptions[i].urgent=0;}
 if(b->catalog_state==AMIHASS_WAIT)b->catalog_state=AMIHASS_FAILED;
}
static void connection_ok(struct Backend *b,unsigned long now)
{if(!b->connected)ah_log_event(AH_LOG_SERVER_CONNECTED,0);b->connected=1;b->attempted=1;b->last_success=now;b->failures=0;b->retry=0;b->next_health=now+60UL;}
static void connection_refresh(struct Backend *b,unsigned long now)
{
 int previous=b->network;if(b->sample_mode)return;
 if(!b->network_known || now>=b->next_network || now+1UL<b->next_network){b->network=amihass_network_running();b->network_known=1;b->next_network=now+1UL;}
 if(b->configured && !b->network && (previous || !b->attempted)){b->last_error=AH_TEST_NO_NETWORK;connection_failed(b,now);}
 if(b->connected && (now<b->last_success || now-b->last_success>120UL))connection_failed(b,now);
}
static const char *connection_state(struct Backend *b,unsigned long now)
{
 if(b->sample_mode)return AMIHASS_SERVER_SAMPLE;
 if(!b->configured)return AMIHASS_SERVER_UNCONFIGURED;
 if(!b->network)return AMIHASS_SERVER_NETWORK_DOWN;
 if(b->connected)return AMIHASS_SERVER_CONNECTED;
 if(b->job && b->phase==4)return AMIHASS_SERVER_CONNECTING;
 if(b->reconnect && b->retry>now)return AMIHASS_SERVER_RETRY_WAIT;
 return b->attempted ? AMIHASS_SERVER_DISCONNECTED:AMIHASS_SERVER_CONNECTING;
}
static void complete(struct Backend *b,int result)
{
 char *data=NULL;unsigned long now=seconds();int i,count=-1,success=0;struct AmiHASSEntity e;
 b->reclaim_due=now+30UL;
 if(result==1)++b->diagnostics.http_ok;else ++b->diagnostics.http_failed;
 if(b->job_generation!=b->generation)return;
 if(result==1)data=read_data(b);

 if(b->phase==1){i=b->index;if(data && amihass_state(data,&e) && !strcmp(e.id,b->subscriptions[i].entity)){success=1;b->subscriptions[i].value=e;amihass_controls(data,&b->subscriptions[i].controls);amihass_control_unit(&b->subscriptions[i].controls,b->temperature_unit);if(b->subscriptions[i].controls.temperature&&!b->subscriptions[i].controls.unit[0])b->unit_needed=1;b->subscriptions[i].updated=now;b->subscriptions[i].result=AMIHASS_OK;}
  else {ah_log_event(AH_LOG_STATE_FAILED,0);b->subscriptions[i].result=AMIHASS_FAILED;}b->subscriptions[i].due=now+(b->subscriptions[i].interval ? b->subscriptions[i].interval:b->connection.poll);b->subscriptions[i].once=0;b->subscriptions[i].urgent=0;
 }else if(b->phase==2){if(b->job_catalog_generation!=b->catalog_generation){if(data)FreeVec(data);return;}if(data)count=b->catalog_kind==AMIHASS_AREAS ? amihass_areas_page(data,b->catalog,AMIHASS_MAX_ENTITIES,&b->catalog_total):b->catalog_kind==AMIHASS_DEVICES ? amihass_devices_page(data,b->catalog,AMIHASS_MAX_ENTITIES,&b->catalog_total):amihass_page(data,b->catalog,AMIHASS_MAX_ENTITIES,&b->catalog_total);b->catalog_count=count>0 ? count:0;b->catalog_state=count>=0 ? AMIHASS_OK:AMIHASS_FAILED;if(count<0)ah_log_event(AH_LOG_CATALOG_FAILED,0);if(count>=0){success=1;cache_page(b);}}
 else if(b->phase==3){/* Acceptance isn't a device-state acknowledgement. Re-read the target. */
  i=b->index;if(result!=1)ah_log_event(AH_LOG_ACTION_FAILED,0);if(result==1)success=1;b->subscriptions[i].due=0;b->subscriptions[i].once=result==1;b->subscriptions[i].urgent=result==1;b->subscriptions[i].seen=now;b->subscriptions[i].result=result==1 ? AMIHASS_WAIT:AMIHASS_FAILED;
 }
 if(b->phase==5){if(data&&amihass_temperature_unit(data,b->temperature_unit)){b->unit_needed=0;for(i=0;i<SELECTED;++i)amihass_control_unit(&b->subscriptions[i].controls,b->temperature_unit);}if(data)FreeVec(data);return;}
 if(b->phase==4)success=data && amihass_api_response(data);
 if(!success&&result==1)b->last_error=AH_TEST_RESPONSE;
 if(success && amihass_network_running()){b->last_error=0;connection_ok(b,now);}else connection_failed(b,now);
 if(data)FreeVec(data);
}
static void record_http_started(struct Backend *b,int phase)
{if(phase==1)++b->diagnostics.poll_http;else if(phase==2)++b->diagnostics.catalog_http;else if(phase==3)++b->diagnostics.action_http;else if(phase==4||phase==5)++b->diagnostics.health_http;}
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
  if(m->kind==1){if(b->transferred)m->kind=-1;else{strcpy(m->command,b->command);strcpy(m->directory,b->directory);b->transferred=1;record_http_started(b,b->phase);}ReplyMsg((struct Message *)m);}
  else if(m->kind==2){if(m->result==28)++b->diagnostics.timeouts;b->last_curl=(int)m->result;b->last_http=job_http_status(b);b->last_error=ah_connection_result((int)m->result,job_http_status(b),m->result==0&&job_http_status(b)==200);if(m->result!=0||job_http_status(b)>=300)ah_log_event(AH_LOG_DOWNLOAD_FAILED,job_http_status(b)>=300 ? 1000L+job_http_status(b):(long)m->result);result=m->result==0 ? 1:-1;FreeVec(m);break;}}
 if(result){complete(b,result);job_close(b,1);}else if(weather_ticks()-b->started>2000UL){++b->diagnostics.timeouts;b->last_error=AH_TEST_TIMEOUT;ah_log_event(AH_LOG_DOWNLOAD_FAILED,28);complete(b,-1);job_close(b,0);}
}
static void reload(struct Backend *b)
{struct AmiHASSConfig config;int loaded=amihass_config_load(&config);b->last_error=b->last_curl=b->last_http=0;b->sample_mode=config.sample_mode;ah_sample_init(&b->sample);b->low_memory=config.low_memory;b->reclaim_due=0;b->connection=config.connection;b->configured=loaded && config.connection.url[0] && config.token[0];b->reconnect=config.reconnect;b->retry_interval=config.retry_interval;amihass_erase(&config,sizeof(config));if(!b->configured)b->connection.url[0]=0;
 ah_log_event(b->configured ? AH_LOG_SETTINGS_RELOADED:AH_LOG_SERVICE_UNCONFIGURED,0);++b->generation;b->prefer_state=1;b->temperature_unit[0]=0;b->unit_needed=b->unit_attempted=0;b->unit_retry=0;b->queued=0;b->catalog_count=0;b->catalog_state=0;b->catalog_kind=0;b->catalog_device[0]=0;b->catalog_area[0]=0;b->retry=0;b->failures=0;b->connected=0;b->attempted=0;b->last_success=0;b->next_health=0;b->network_known=0;memset(b->subscriptions,0,sizeof(b->subscriptions));memset(b->selections,0,sizeof(b->selections));memset(b->pages,0,sizeof(b->pages));}
static int subscription(struct Backend *b,const char *owner,const char *entity)
{int i,j,free_slot=-1,reusable=-1,pinned;unsigned long now=seconds();for(i=0;i<SELECTED;++i){struct Subscription *s=&b->subscriptions[i];if(!strcmp(s->entity,entity))return i;pinned=b->job&&(b->phase==1||b->phase==3)&&b->index==i;for(j=0;j<b->queued&&!pinned;++j)if(!strcmp(b->queue[j].entity,s->entity))pinned=1;if(pinned)continue;if(!s->owner[0]){if(free_slot<0)free_slot=i;}else if(!s->watching&&(!s->once||now-s->seen>10UL)&&!s->urgent&&(reusable<0||s->seen<b->subscriptions[reusable].seen))reusable=i;}if(free_slot<0)free_slot=reusable;if(free_slot<0)return -1;memset(&b->subscriptions[free_slot],0,sizeof(b->subscriptions[free_slot]));strcpy(b->subscriptions[free_slot].owner,owner);strcpy(b->subscriptions[free_slot].entity,entity);b->subscriptions[free_slot].result=AMIHASS_WAIT;return free_slot;}
static int active_subscription(struct Subscription *s,unsigned long now)
{return s->owner[0] && ((s->watching && now>=s->watch_seen && now-s->watch_seen<=120UL) || (s->once && now>=s->seen && now-s->seen<=10UL));}
static int client_recent(unsigned long seen,unsigned long now)
{return seen&&now>=seen&&now-seen<=120UL;}
static void refresh_leases(struct Backend *b,unsigned long now)
{int i,j;for(i=0;i<SELECTED;++i){struct Subscription *s=&b->subscriptions[i];unsigned int previous=s->interval;s->watching=0;s->watch_seen=0;s->interval=b->connection.poll;for(j=0;j<64;++j){struct ClientSelection *c=&b->selections[j];if(!c->leased||!client_recent(c->lease_seen,now)||strcmp(c->entity,s->entity))continue;if(!s->watching||c->interval<s->interval)s->interval=c->interval;s->watching=1;if(c->lease_seen>s->watch_seen)s->watch_seen=c->lease_seen;s->seen=now;}if(s->watching&&s->updated&&!s->once&&!s->urgent&&s->result!=AMIHASS_WAIT&&previous!=s->interval)s->due=s->updated+s->interval;}}
static int select_lease(struct Backend *b,const struct AmiHASSMessage *m,unsigned long now,unsigned int interval)
{int i,slot=-1;struct ClientSelection *c;for(i=0;i<64;++i){c=&b->selections[i];if(c->port==m->msg.mn_ReplyPort&&!strcmp(c->owner,m->owner)&&!strcmp(c->entity,m->entity)){slot=i;break;}if(slot<0&&(!c->leased||!client_recent(c->lease_seen,now)))slot=i;}if(slot<0)return 0;c=&b->selections[slot];strcpy(c->owner,m->owner);strcpy(c->entity,m->entity);c->seen=now;c->port=m->msg.mn_ReplyPort;c->interval=interval;c->lease_seen=now;c->leased=1;refresh_leases(b,now);return 1;}
static void release_lease(struct Backend *b,const struct AmiHASSMessage *m,unsigned long now)
{int i;struct MsgPort *identity=m->msg.mn_ReplyPort ? m->msg.mn_ReplyPort:(struct MsgPort *)(unsigned long)m->age;for(i=0;i<64;++i){struct ClientSelection *c=&b->selections[i];if(c->port==identity&&(!m->entity[0]||!strcmp(c->entity,m->entity)))memset(c,0,sizeof(*c));}refresh_leases(b,now);}
static void client_record(struct Backend *b,const struct AmiHASSMessage *m,unsigned long now)
{int i,slot=-1;struct ClientCounter *c;if(!m->owner[0])return;for(i=0;m->owner[i];++i)if((unsigned char)m->owner[i]<32||(unsigned char)m->owner[i]>126)return;for(i=0;i<16;++i){if(!strcmp(b->clients[i].owner,m->owner)){slot=i;break;}if(slot<0&&!client_recent(b->clients[i].seen,now))slot=i;}if(slot<0)return;c=&b->clients[slot];if(strcmp(c->owner,m->owner)){memset(c,0,sizeof(*c));strcpy(c->owner,m->owner);}c->seen=now;++c->requests;if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE))++c->queries;if(m->kind==AMIHASS_LIST||m->kind==AMIHASS_DEVICES||m->kind==AMIHASS_AREAS)++c->lists;if(m->kind==AMIHASS_ACTION)++c->actions;
 if((m->kind!=AMIHASS_QUERY&&m->kind!=AMIHASS_SUBSCRIBE&&m->kind!=AMIHASS_ACTION)||!amihass_entity_id(m->entity))return;slot=-1;for(i=0;i<64;++i){if(b->selections[i].port==m->msg.mn_ReplyPort&&!strcmp(b->selections[i].owner,m->owner)&&!strcmp(b->selections[i].entity,m->entity)){slot=i;break;}if(slot<0&&!client_recent(b->selections[i].seen,now))slot=i;}if(slot>=0){if(b->selections[slot].port!=m->msg.mn_ReplyPort||strcmp(b->selections[slot].owner,m->owner)||strcmp(b->selections[slot].entity,m->entity))memset(&b->selections[slot],0,sizeof(b->selections[slot]));strcpy(b->selections[slot].owner,m->owner);strcpy(b->selections[slot].entity,m->entity);b->selections[slot].seen=now;b->selections[slot].port=m->msg.mn_ReplyPort;}}
static void client_snapshot(struct Backend *b,struct AmiHASSMessage *m,unsigned long now)
{int i,j,index=0;struct AmiHASSEntity item;m->total=m->count=0;if(m->offset<0)m->offset=0;for(i=0;i<16;++i){struct ClientCounter *c=&b->clients[i];if(!client_recent(c->seen,now))continue;memset(&item,0,sizeof(item));strcpy(item.id,c->owner);strcpy(item.name,c->owner);sprintf(item.state,"%lu",c->requests);sprintf(item.unit,"%lu",c->queries);sprintf(item.device_class,"%lu",c->actions);sprintf(item.updated,"%lu",c->lists);if(index>=m->offset&&m->count<AMIHASS_PAGE)m->items[m->count++]=item;++index;
 for(j=0;j<64;++j){struct ClientSelection *e=&b->selections[j];if(strcmp(e->owner,c->owner)||!client_recent(e->seen,now))continue;memset(&item,0,sizeof(item));strcpy(item.id,e->entity);strcpy(item.name,c->owner);strcpy(item.state,"Selected entity");strcpy(item.device_class,"entity");if(index>=m->offset&&m->count<AMIHASS_PAGE)m->items[m->count++]=item;++index;}}m->total=index;m->result=AMIHASS_OK;}
static void diagnostics_snapshot(struct Backend *b,struct AmiHASSMessage *m,unsigned long now)
{
 static const char *keys[]={"ipc.received","ipc.replies","ipc.bytes_in","ipc.bytes_out","ipc.queries","ipc.lists","ipc.actions","actions.accepted","http.actions","http.polls","http.catalogue","http.completed","http.failed","http.body_bytes","cache.hits","http.timeouts","service.uptime","poll.seconds","entities.active","actions.queued","retry.seconds","http.active","cache.pages","ipc.invalid","http.health"};
 static const char *names[]={"Bridge messages received","Bridge replies returned","Bridge bytes received","Bridge bytes returned","Client state queries","Client catalogue queries","Client action requests","Actions accepted into queue","HA action HTTP attempts","HA state-poll HTTP attempts","HA catalogue HTTP attempts","HTTP completions (curl success)","HTTP request failures","Response body bytes read","Cache hits","Downloader timeouts","Service uptime (seconds)","Default poll interval (seconds)","Active selected entities","Actions waiting in queue","Retry backoff (seconds left)","Downloader active (0/1)","Cached catalogue pages","Invalid bridge messages","HA connection/config requests"};
 unsigned long values[25];int i,index;if(!strcmp(m->domain,"memory")){static const char *keys[]={"memory.fast.baseline","memory.fast.free","memory.fast.minimum","memory.request.before","memory.request.minimum","memory.request.after","memory.request.peak","memory.reclaimed","memory.requests","memory.flushes","memory.fast.largest","leases.active"};static const char *labels[]={"Fast free at service baseline","Fast free now","Lowest observed Fast free","Fast free before last HTTP","Lowest free during last HTTP","Fast free after last HTTP","Largest observed HTTP drop","Fast reclaimed by last flush","HTTP requests measured","Idle flushes measured","Largest free Fast block","Active client/entity leases"};unsigned long v[12];ah_memory_sample(&b->memory,AvailMem(MEMF_FAST));v[0]=b->memory.baseline;v[1]=b->memory.current;v[2]=b->memory.minimum;v[3]=b->memory.before;v[4]=b->memory.request_min;v[5]=b->memory.after;v[6]=b->memory.peak;v[7]=b->memory.reclaimed;v[8]=b->memory.requests;v[9]=b->memory.flushes;v[10]=AvailMem(MEMF_FAST|MEMF_LARGEST);v[11]=0;for(i=0;i<64;++i)if(b->selections[i].leased&&client_recent(b->selections[i].lease_seen,now))++v[11];m->count=m->total=12;m->offset=0;for(i=0;i<12;++i){memset(&m->items[i],0,sizeof(m->items[i]));strcpy(m->items[i].id,keys[i]);strcpy(m->items[i].name,labels[i]);sprintf(m->items[i].state,"%lu",v[i]);}m->result=AMIHASS_OK;return;}if(!strcmp(m->domain,"connection")){static const char *labels[]={"network.running","connection.error","curl.code","http.code"};int codes[4];codes[0]=amihass_network_running();codes[1]=b->last_error;codes[2]=b->last_curl;codes[3]=b->last_http;m->count=m->total=4;m->offset=0;for(i=0;i<4;++i){memset(&m->items[i],0,sizeof(m->items[i]));strcpy(m->items[i].id,labels[i]);sprintf(m->items[i].state,"%d",codes[i]);}m->result=AMIHASS_OK;return;}if(!strcmp(m->domain,"clients")){client_snapshot(b,m,now);return;}
 values[0]=b->diagnostics.received;values[1]=b->diagnostics.replies;values[2]=b->diagnostics.received_bytes;values[3]=b->diagnostics.sent_bytes;
 values[4]=b->diagnostics.queries;values[5]=b->diagnostics.lists;values[6]=b->diagnostics.actions;values[7]=b->diagnostics.accepted;
 values[8]=b->diagnostics.action_http;values[9]=b->diagnostics.poll_http;values[10]=b->diagnostics.catalog_http;values[11]=b->diagnostics.http_ok;
 values[12]=b->diagnostics.http_failed;values[13]=b->diagnostics.body_bytes;values[14]=b->diagnostics.cache_hits;values[15]=b->diagnostics.timeouts;
 values[16]=now-b->service_started;values[17]=b->connection.poll;values[18]=0;values[19]=b->queued;values[20]=b->retry>now ? b->retry-now:0;values[21]=b->job!=NULL;values[22]=0;values[23]=b->diagnostics.invalid;values[24]=b->diagnostics.health_http;
 for(i=0;i<SELECTED;++i)if(active_subscription(&b->subscriptions[i],now))++values[18];
 for(i=0;i<4;++i)if(b->pages[i].used && now>=b->pages[i].saved && now-b->pages[i].saved<=60UL)++values[22];
 if(m->offset<0 || m->offset>=25)m->offset=0;m->total=25;m->count=25-m->offset;if(m->count>AMIHASS_PAGE)m->count=AMIHASS_PAGE;
 for(i=0;i<m->count;++i){index=(int)m->offset+i;memset(&m->items[i],0,sizeof(m->items[i]));strcpy(m->items[i].id,keys[index]);strcpy(m->items[i].name,names[index]);sprintf(m->items[i].state,"%lu",values[index]);}
 m->result=AMIHASS_OK;
}
static void handle(struct Backend *b,struct AmiHASSMessage *m)
{
 int i,index;unsigned int interval;char path[96],body[180],area[96];unsigned long now=seconds();
 ++b->diagnostics.received;b->diagnostics.received_bytes+=m->msg.mn_Length;
 if(m->magic!=AMIHASS_MAGIC || m->abi!=AMIHASS_ABI || m->msg.mn_Length!=sizeof(*m)){++b->diagnostics.invalid;reply(b,m);return;}
 m->item.id[95]=0;strcpy(area,m->item.id);m->owner[19]=0;m->entity[95]=0;m->action[19]=0;m->domain[23]=0;m->filter[63]=0;m->result=AMIHASS_WAIT;m->count=m->total=0;b->last_client=now;connection_refresh(b,now);
 refresh_leases(b,now);if(m->kind==AMIHASS_UNSUBSCRIBE){release_lease(b,m,now);m->result=AMIHASS_OK;reply(b,m);return;}
 if(m->kind==AMIHASS_SUBSCRIBE&&(((ULONG)m->refresh&~(65535UL<<8))||!m->owner[0]||!amihass_entity_id(m->entity)||(AMIHASS_QUERY_MAX_AGE(m->refresh)&&(AMIHASS_QUERY_MAX_AGE(m->refresh)<5||AMIHASS_QUERY_MAX_AGE(m->refresh)>3600)))){m->result=AMIHASS_UNSUPPORTED;reply(b,m);return;}
 client_record(b,m,now);if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE))++b->diagnostics.queries;else if(m->kind==AMIHASS_LIST || m->kind==AMIHASS_DEVICES || m->kind==AMIHASS_AREAS)++b->diagnostics.lists;else if(m->kind==AMIHASS_ACTION)++b->diagnostics.actions;
 if(m->kind==AMIHASS_DIAGNOSTICS){diagnostics_snapshot(b,m,now);reply(b,m);return;}
 if(b->stopping && m->kind!=AMIHASS_STATUS && m->kind!=AMIHASS_STOP){m->result=AMIHASS_FAILED;reply(b,m);return;}
 if(m->kind==AMIHASS_STATUS){
  memset(&m->status,0,sizeof(m->status));m->status.uptime=now-b->service_started;m->status.last_contact=b->last_success;m->status.connected=b->sample_mode || (b->connected && b->network);
  strcpy(m->status.server,b->sample_mode ? "sample://amihass":b->connection.url);m->status.queued=b->queued;
  for(i=0;i<SELECTED;++i)if(active_subscription(&b->subscriptions[i],now))++m->status.subscriptions;
  memset(&m->item,0,sizeof(m->item));strcpy(m->item.state,connection_state(b,now));if(b->last_error<0)strncpy(m->item.name,ah_connection_text(b->last_error),sizeof(m->item.name)-1);m->age=b->last_success && now>=b->last_success ? now-b->last_success:0;
  m->count=m->total=3;for(i=0;i<3;++i)memset(&m->items[i],0,sizeof(m->items[i]));
  strcpy(m->items[0].id,"reconnect.enabled");sprintf(m->items[0].state,"%d",b->reconnect);
  strcpy(m->items[1].id,"retry.interval");sprintf(m->items[1].state,"%u",b->retry_interval);
  strcpy(m->items[2].id,"retry.remaining");sprintf(m->items[2].state,"%lu",b->reconnect && b->retry>now ? b->retry-now:0);
  m->result=AMIHASS_OK;reply(b,m);return;
 }
 if(m->kind==AMIHASS_STOP){if(!b->stopping)ah_log_event(AH_LOG_SERVICE_STOPPING,0);b->stopping=1;b->queued=0;m->result=AMIHASS_OK;reply(b,m);return;}
 if(m->kind==AMIHASS_RELOAD || m->kind==AMIHASS_RECONNECT){reload(b);m->result=AMIHASS_OK;reply(b,m);return;}
 if(b->sample_mode){int kind=m->kind;if(kind==AMIHASS_SUBSCRIBE)m->kind=AMIHASS_QUERY;ah_sample_request(&b->sample,m,now);m->kind=kind;if(kind==AMIHASS_SUBSCRIBE&&(m->result==AMIHASS_OK||m->result==AMIHASS_UNAVAILABLE)){interval=AMIHASS_QUERY_MAX_AGE(m->refresh);if(!interval)interval=b->connection.poll;index=subscription(b,m->owner,m->entity);if(interval<5||interval>3600||index<0||!select_lease(b,m,now,interval))m->result=AMIHASS_BUSY;}if(m->kind==AMIHASS_ACTION && m->result==AMIHASS_WAIT)++b->diagnostics.accepted;reply(b,m);return;}
 if(((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE) || m->kind==AMIHASS_ACTION) && strstr(m->entity,".amihass_sample_")){m->result=AMIHASS_UNCONFIGURED;reply(b,m);return;}
 if(!b->connection.url[0]){m->result=AMIHASS_UNCONFIGURED;reply(b,m);return;}
 if(m->kind==AMIHASS_LIST || m->kind==AMIHASS_DEVICES || m->kind==AMIHASS_AREAS){
  int cached;if(m->kind==AMIHASS_AREAS){area[0]=m->domain[0]=m->filter[0]=m->entity[0]=0;}if(!amihass_area_valid(area)){m->result=AMIHASS_FAILED;reply(b,m);return;}if(m->kind==AMIHASS_DEVICES){m->entity[0]=0;}else if(m->entity[0] && !amihass_device_id(m->entity)){m->result=AMIHASS_FAILED;reply(b,m);return;}
  cached=cached_page(b,m->kind,m->entity,m->domain,m->filter,area,m->offset);
  if(cached>=0){struct CatalogCache *p=&b->pages[cached];if(m->refresh)p->used=0;else{++b->diagnostics.cache_hits;m->result=AMIHASS_OK;m->count=p->count;m->total=p->total;for(i=0;i<p->count;++i)m->items[i]=p->items[i];reply(b,m);return;}}
  /* An overlapping browser never invalidates another caller's in-flight page. */
  if(b->job && b->phase==2 && (strcmp(area,b->catalog_area) || m->kind!=b->catalog_kind || strcmp(m->entity,b->catalog_device) || strcmp(m->domain,b->catalog_domain) || strcmp(m->filter,b->catalog_filter) || m->offset!=b->catalog_offset)){reply(b,m);return;}
  if(m->offset<0 || m->offset>100000)m->offset=0;
  if((m->refresh && b->catalog_state!=AMIHASS_WAIT) || strcmp(area,b->catalog_area) || m->kind!=b->catalog_kind || strcmp(m->entity,b->catalog_device) || strcmp(m->domain,b->catalog_domain) || strcmp(m->filter,b->catalog_filter) || m->offset!=b->catalog_offset){
   strcpy(b->catalog_area,area);b->catalog_kind=m->kind;strcpy(b->catalog_device,m->entity);strcpy(b->catalog_domain,m->domain);strcpy(b->catalog_filter,m->filter);b->catalog_offset=m->offset;++b->catalog_generation;b->catalog_state=AMIHASS_WAIT;b->catalog_count=0;
  }
  if(!b->catalog_state || (b->catalog_state==AMIHASS_FAILED && b->connected))b->catalog_state=AMIHASS_WAIT;m->result=b->catalog_state;m->total=b->catalog_total;m->count=b->catalog_count;
  for(i=0;i<b->catalog_count;++i)m->items[i]=b->catalog[i];
 }else if(((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE) || m->kind==AMIHASS_ACTION) && m->owner[0] && amihass_entity_id(m->entity)){
  index=subscription(b,m->owner,m->entity);if(index<0)m->result=AMIHASS_BUSY;
  else{struct Subscription *s=&b->subscriptions[index];s->seen=now;interval=b->connection.poll;if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE)&&AMIHASS_QUERY_MAX_AGE(m->refresh)){interval=AMIHASS_QUERY_MAX_AGE(m->refresh);if(interval<5||interval>3600){m->result=AMIHASS_UNSUPPORTED;reply(b,m);return;}}
   if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE)){
    if(m->refresh & (AMIHASS_QUERY_ONCE|AMIHASS_QUERY_FRESH)){if(((m->refresh&AMIHASS_QUERY_FRESH)&&s->result!=AMIHASS_WAIT) || s->result==AMIHASS_FAILED || !s->updated || now<s->updated || now-s->updated>=interval){s->once=1;s->due=0;if(b->connected)s->result=AMIHASS_WAIT;}}
    else if(!select_lease(b,m,now,interval)){m->result=AMIHASS_BUSY;reply(b,m);return;}
   }
   m->item=s->value;m->count=amihass_control_items(&s->controls,m->items,AMIHASS_PAGE);m->age=s->updated ? now-s->updated:0;m->result=s->result;
   if(s->result==AMIHASS_OK && !amihass_available(&s->value))m->result=AMIHASS_UNAVAILABLE;
   if(s->updated && m->age>interval*2UL+30UL)m->result=AMIHASS_STALE;
   if((m->kind==AMIHASS_QUERY||m->kind==AMIHASS_SUBSCRIBE) && m->result==AMIHASS_OK)++b->diagnostics.cache_hits;
   if(m->kind==AMIHASS_ACTION){
    if(!amihass_action(m->entity,m->action,path,body)||!amihass_control_allowed(&s->controls,m->action))m->result=AMIHASS_UNSUPPORTED;
    else if(s->result!=AMIHASS_OK || m->result==AMIHASS_STALE)m->result=AMIHASS_STALE;
    else if(!amihass_available(&s->value))m->result=AMIHASS_UNAVAILABLE;
    else if(!b->connected || !b->network || now<b->retry)m->result=AMIHASS_FAILED;
    else if(b->queued>=QUEUED)m->result=AMIHASS_BUSY;
    else{++b->diagnostics.accepted;strcpy(b->queue[b->queued].entity,m->entity);strcpy(b->queue[b->queued++].action,m->action);m->result=AMIHASS_WAIT;s->result=AMIHASS_WAIT;}
   }
  }
 }else m->result=AMIHASS_UNCONFIGURED;reply(b,m);
}
static void schedule(struct Backend *b,const char *program)
{
 unsigned long now=seconds();int i,index;char path[128],body[180],catalogue[4096];
 refresh_leases(b,now);if(b->sample_mode)return;
 connection_refresh(b,now);
 if(b->stopping || b->job || present(FETCH_GUARD) || !b->configured || !b->network || now<b->retry)return;
 if(!b->connected){
  if(b->attempted && !b->reconnect)return;b->attempted=1;
  if(!job_start(b,program,"/api/",NULL,4,-1))connection_failed(b,now);return;
 }
 if(b->queued){
  for(index=0;index<SELECTED;++index)if(!strcmp(b->subscriptions[index].entity,b->queue[0].entity))break;
  if(index<SELECTED && amihass_action(b->queue[0].entity,b->queue[0].action,path,body)){
   if(!job_start(b,program,path,b->queue[0].entity,3,index)){b->subscriptions[index].result=AMIHASS_FAILED;connection_failed(b,now);return;}
  }
  --b->queued;memmove(b->queue,b->queue+1,(unsigned int)b->queued*sizeof(b->queue[0]));return;
 }
 if(b->unit_needed&&(!b->unit_attempted||now>=b->unit_retry)){b->unit_attempted=1;b->unit_retry=now+60;job_start(b,program,"/api/config",NULL,5,-1);return;}
 index=-1;
 for(i=0;i<SELECTED;++i){int candidate=(b->round+i)%SELECTED;struct Subscription *s=&b->subscriptions[candidate];
  if(active_subscription(s,now)&&now>=s->due){if(index<0)index=candidate;if(s->urgent){index=candidate;break;}}}
 if(index>=0&&(b->subscriptions[index].urgent||b->catalog_state!=AMIHASS_WAIT||b->prefer_state)){
  sprintf(path,"/api/states/%s",b->subscriptions[index].entity);
  if(!job_start(b,program,path,NULL,1,index)){b->subscriptions[index].result=AMIHASS_FAILED;connection_failed(b,now);}
  b->round=(index+1)%SELECTED;b->prefer_state=0;return;
 }
 if(b->catalog_state==AMIHASS_WAIT){if(!(b->catalog_kind==AMIHASS_AREAS ? amihass_areas_request(b->catalog_offset,catalogue,sizeof(catalogue)):b->catalog_kind==AMIHASS_DEVICES ? amihass_devices_area_request(b->catalog_area,b->catalog_domain,b->catalog_filter,b->catalog_offset,catalogue,sizeof(catalogue)):amihass_device_area_page_request(b->catalog_device,b->catalog_area,b->catalog_domain,b->catalog_filter,b->catalog_offset,catalogue,sizeof(catalogue)))||!job_start(b,program,"/api/template",catalogue,2,-1)){b->catalog_state=AMIHASS_FAILED;connection_failed(b,now);}b->prefer_state=1;return;}
 if(now>=b->next_health){if(!job_start(b,program,"/api/",NULL,4,-1))connection_failed(b,now);return;}

}
static int backend_run(const char *program,int restoring)
{
 struct Backend *b;struct AmiHASSMessage *m;struct AHWait sleeper;struct AHRexx rexx;int owner=0;
 ((struct Process *)FindTask(NULL))->pr_WindowPtr=(APTR)-1;
 if(!restoring && present("AmiHASS.Maintenance"))return 20;
 b=AllocVec(sizeof(*b),MEMF_PUBLIC|MEMF_CLEAR);if(!b)return 20;b->port=CreateMsgPort();if(!b->port){FreeVec(b);return 20;}
 Forbid();if(!FindPort(AMIHASS_PORT)){b->port->mp_Node.ln_Name=AMIHASS_PORT;AddPort(b->port);owner=1;}Permit();if(!owner){DeleteMsgPort(b->port);FreeVec(b);return 0;}
 ah_log_event(AH_LOG_SERVICE_STARTED,0);
 memset(&sleeper,0,sizeof(sleeper));ah_rexx_open(&rexx);reload(b);b->last_client=seconds();b->service_started=seconds();
 for(;;){ah_memory_sample(&b->memory,AvailMem(MEMF_FAST));ah_rexx_step(&rexx);while((m=(struct AmiHASSMessage *)GetMsg(b->port))!=NULL)handle(b,m);job_events(b);schedule(b,program);reclaim_idle(b);if(b->stopping || (SetSignal(0L,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C))break;
  ah_wait(&sleeper,ah_rexx_signals(&rexx)|(1UL<<b->port->mp_SigBit)|(b->job ? 1UL<<b->job->mp_SigBit:0));}
 ah_rexx_close(&rexx);Forbid();RemPort(b->port);Permit();while((m=(struct AmiHASSMessage *)GetMsg(b->port))!=NULL){m->result=AMIHASS_FAILED;reply(b,m);}DeleteMsgPort(b->port);job_close(b,0);ah_wait_close(&sleeper);FreeVec(b);ah_log_event(AH_LOG_SERVICE_STOPPED,0);return 0;
}
int amihass_backend(const char *program){return backend_run(program,0);}
int amihass_backend_restore(const char *program){return backend_run(program,1);}
int amihass_worker(const char *parent)
{if(!parent || strncmp(parent,"AmiHASS.Job.",12) || strlen(parent)>70)return 20;return fetch_worker(parent);}
