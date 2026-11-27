/* Optional ARexx host. Requests use the same asynchronous public SDK. */
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <string.h>
#include "rexx_bridge.h"
struct Library *RexxSysBase=NULL;
static void finish(struct RexxMsg *m,int rc,int detail,const char *text)
{m->rm_Result1=rc;m->rm_Result2=rc ? detail:0;if(!rc&&(m->rm_Action&RXFF_RESULT)){m->rm_Result2=(LONG)CreateArgstring(text,strlen(text));if(!m->rm_Result2)m->rm_Result1=20;}ReplyMsg((struct Message *)m);}
static long now_seconds(void){struct DateStamp d;DateStamp(&d);return d.ds_Days*86400L+d.ds_Minute*60L+d.ds_Tick/TICKS_PER_SECOND;}
/* WAITEVENT: re-read the entity about once a second through the service's
 * shared cache (push updates or polling keep it current) until it changes. */
static int wait_ask(struct AHRexx *h)
{struct AHRexxWait *w=&h->wait;return w->count==1 ? amihass_subscribe(&h->wait_client,"AmiHASS.ARexx",w->ids[0],5):amihass_query_many(&h->wait_client,"AmiHASS.ARexx",(const char (*)[96])w->ids,(unsigned int)w->count,AMIHASS_QUERY_INTERVAL(5),1);}
static void wait_step(struct AHRexx *h)
{struct AmiHASSMessage *r;struct AHRexxWait *w=&h->wait;char text[400];int verdict;long now;if(!h->waiter)return;now=now_seconds();
 if(w->again){if(now==w->asked)return;w->again=0;if(!wait_ask(h)){finish(h->waiter,20,9,"");h->waiter=NULL;}return;}
 r=amihass_reply(&h->wait_client);if(!r){if(!h->wait_client.pending){finish(h->waiter,20,AMIHASS_FAILED,"");h->waiter=NULL;}return;}
 verdict=ah_rexx_wait_check(w,r,now);FreeVec(r);
 if(!verdict){w->again=1;w->asked=now;return;}
 if(verdict==3){finish(h->waiter,10,AMIHASS_UNSUPPORTED,"");h->waiter=NULL;return;}
 snprintf(text,sizeof(text),"result=%s\nid=%s\nstate=%s\nprevious=%s\nupdated=%s",verdict==1?"changed":"timeout",verdict==1||w->count!=1?w->entity:w->ids[0],verdict==1||w->count!=1?w->state:w->states[0],w->previous,w->updated);finish(h->waiter,0,0,text);h->waiter=NULL;}
void ah_rexx_open(struct AHRexx *h)
{int owned=0;memset(h,0,sizeof(*h));RexxSysBase=OpenLibrary(RXSNAME,36);if(!RexxSysBase)return;h->port=CreateMsgPort();if(h->port){Forbid();if(!FindPort(AH_REXX_PORT)){h->port->mp_Node.ln_Name=AH_REXX_PORT;AddPort(h->port);owned=1;}Permit();}if(!owned){if(h->port)DeleteMsgPort(h->port);h->port=NULL;CloseLibrary(RexxSysBase);RexxSysBase=NULL;}}
void ah_rexx_step(struct AHRexx *h)
{struct RexxMsg *rx;struct AmiHASSMessage request,*reply;char text[4096];int result;if(!h->port)return;
 wait_step(h);
 while((rx=(struct RexxMsg *)GetMsg(h->port))!=NULL){if(rx->rm_Node.mn_Length<sizeof(*rx)){ReplyMsg((struct Message *)rx);continue;}if(!IsRexxMsg(rx)||(rx->rm_Action&0xff000000)!=RXCOMM||!rx->rm_Args[0]){finish(rx,10,1,"");continue;}if(h->pending){finish(rx,5,AMIHASS_BUSY,"");continue;}if(!ah_rexx_parse(rx->rm_Args[0],&request)){finish(rx,10,1,"");continue;}
 if(!strcmp(request.domain,AH_REXX_WAIT_DOMAIN)){if(h->waiter){finish(rx,5,AMIHASS_BUSY,"");continue;}{int k;memset(&h->wait,0,sizeof(h->wait));h->wait.count=(int)request.count;for(k=0;k<h->wait.count;++k)strcpy(h->wait.ids[k],request.items[k].id);strcpy(h->wait.target,request.filter);h->wait.deadline=now_seconds()+request.offset;}
  if(!wait_ask(h)){finish(rx,20,9,"");continue;}h->waiter=rx;continue;}
 if(!request.kind){finish(rx,0,0,"STATUS; DEVICES [filter] [offset]; LIST [domain] [filter] [offset]; ENTITIES device [domain] [filter] [offset]; QUERY entity; ACTION entity action; ATTRIBUTES entity [offset]; HISTORY entity [hours] [offset]; WAITEVENT entity[,entity...] [seconds] [state]; NOTIFICATIONS [offset]; NOTIFICATION id");continue;}
 if(!amihass_send(&h->client,request.kind,request.owner,request.entity[0] ? request.entity:NULL,request.action[0] ? request.action:NULL,request.offset,request.refresh,request.domain,request.filter)){finish(rx,20,9,"");continue;}h->pending=rx;h->kind=request.kind;h->request=request;h->tries=0;if(request.kind==AMIHASS_ACTION){amihass_erase(request.filter,sizeof(request.filter));amihass_erase(h->request.filter,sizeof(h->request.filter));}}
 if(!h->pending)return;
 /* ATTRIBUTES/HISTORY answer WAIT while the service fetches; ask again about
  * once a second (up to 30 s) so the script gets one final reply. */
 if(h->again){if(now_seconds()==h->asked)return;h->again=0;if(!amihass_send(&h->client,h->request.kind,h->request.owner,h->request.entity,NULL,h->request.offset,h->request.refresh,h->request.domain,h->request.filter)){finish(h->pending,20,9,"");h->pending=NULL;}return;}
 reply=amihass_reply(&h->client);if(!reply){if(!h->client.pending){finish(h->pending,20,AMIHASS_FAILED,"");h->pending=NULL;}return;}
 /* ACTION succeeds only when the service accepted it (WAIT/OK). A STALE or
 * UNAVAILABLE action was not sent, so scripts must see a non-zero RC. */
 result=reply->result;if(result==AMIHASS_WAIT&&(h->kind==AMIHASS_ATTRIBUTES||h->kind==AMIHASS_HISTORY||h->kind==AMIHASS_NOTIFICATIONS)&&++h->tries<30){h->asked=now_seconds();h->again=1;FreeVec(reply);return;}
 if(result==AMIHASS_OK||(h->kind==AMIHASS_ACTION&&result==AMIHASS_WAIT)||(h->kind!=AMIHASS_ACTION&&(result==AMIHASS_UNAVAILABLE||result==AMIHASS_STALE))){ah_rexx_format(reply,text,sizeof(text));finish(h->pending,0,0,text);}else finish(h->pending,result==AMIHASS_WAIT||result==AMIHASS_BUSY||result==AMIHASS_STALE ? 5:10,result,"");FreeVec(reply);h->pending=NULL;}
unsigned long ah_rexx_signals(struct AHRexx *h){return (h->port ? 1UL<<h->port->mp_SigBit:0)|(h->client.port ? 1UL<<h->client.port->mp_SigBit:0)|(h->wait_client.port ? 1UL<<h->wait_client.port->mp_SigBit:0);}
void ah_rexx_close(struct AHRexx *h)
{struct RexxMsg *m;if(h->port){Forbid();RemPort(h->port);Permit();while((m=(struct RexxMsg *)GetMsg(h->port))!=NULL)finish(m,20,AMIHASS_FAILED,"");if(h->pending)finish(h->pending,20,AMIHASS_FAILED,"");if(h->waiter)finish(h->waiter,20,AMIHASS_FAILED,"");amihass_client_close(&h->client);amihass_client_close(&h->wait_client);DeleteMsgPort(h->port);}memset(h,0,sizeof(*h));if(RexxSysBase){CloseLibrary(RexxSysBase);RexxSysBase=NULL;}}
