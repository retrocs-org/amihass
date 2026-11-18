/* Optional ARexx host. Requests use the same asynchronous public SDK. */
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/rexxsyslib.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <string.h>
#include "rexx_bridge.h"
struct Library *RexxSysBase=NULL;
static void finish(struct RexxMsg *m,int rc,int detail,const char *text)
{m->rm_Result1=rc;m->rm_Result2=rc ? detail:0;if(!rc&&(m->rm_Action&RXFF_RESULT)){m->rm_Result2=(LONG)CreateArgstring(text,strlen(text));if(!m->rm_Result2)m->rm_Result1=20;}ReplyMsg((struct Message *)m);}
void ah_rexx_open(struct AHRexx *h)
{int owned=0;memset(h,0,sizeof(*h));RexxSysBase=OpenLibrary(RXSNAME,36);if(!RexxSysBase)return;h->port=CreateMsgPort();if(h->port){Forbid();if(!FindPort(AH_REXX_PORT)){h->port->mp_Node.ln_Name=AH_REXX_PORT;AddPort(h->port);owned=1;}Permit();}if(!owned){if(h->port)DeleteMsgPort(h->port);h->port=NULL;CloseLibrary(RexxSysBase);RexxSysBase=NULL;}}
void ah_rexx_step(struct AHRexx *h)
{struct RexxMsg *rx;struct AmiHASSMessage request,*reply;char text[4096];int result;if(!h->port)return;
 while((rx=(struct RexxMsg *)GetMsg(h->port))!=NULL){if(rx->rm_Node.mn_Length<sizeof(*rx)){ReplyMsg((struct Message *)rx);continue;}if(!IsRexxMsg(rx)||(rx->rm_Action&0xff000000)!=RXCOMM||!rx->rm_Args[0]){finish(rx,10,1,"");continue;}if(h->pending){finish(rx,5,AMIHASS_BUSY,"");continue;}if(!ah_rexx_parse(rx->rm_Args[0],&request)){finish(rx,10,1,"");continue;}
 if(!request.kind){finish(rx,0,0,"STATUS; DEVICES [filter] [offset]; LIST [domain] [filter] [offset]; ENTITIES device [domain] [filter] [offset]; QUERY entity; ACTION entity action");continue;}
 if(!amihass_send(&h->client,request.kind,request.owner,request.entity[0] ? request.entity:NULL,request.action[0] ? request.action:NULL,request.offset,request.refresh,request.domain,request.filter)){finish(rx,20,9,"");continue;}h->pending=rx;h->kind=request.kind;}
 if(!h->pending)return;reply=amihass_reply(&h->client);if(!reply){if(!h->client.pending){finish(h->pending,20,AMIHASS_FAILED,"");h->pending=NULL;}return;}
 result=reply->result;if(result==AMIHASS_OK||(h->kind==AMIHASS_ACTION&&result==AMIHASS_WAIT)||result==AMIHASS_UNAVAILABLE||result==AMIHASS_STALE){ah_rexx_format(reply,text,sizeof(text));finish(h->pending,0,0,text);}else finish(h->pending,result==AMIHASS_WAIT||result==AMIHASS_BUSY ? 5:10,result,"");FreeVec(reply);h->pending=NULL;}
unsigned long ah_rexx_signals(struct AHRexx *h){return (h->port ? 1UL<<h->port->mp_SigBit:0)|(h->client.port ? 1UL<<h->client.port->mp_SigBit:0);}
void ah_rexx_close(struct AHRexx *h)
{struct RexxMsg *m;if(h->port){Forbid();RemPort(h->port);Permit();while((m=(struct RexxMsg *)GetMsg(h->port))!=NULL)finish(m,20,AMIHASS_FAILED,"");if(h->pending)finish(h->pending,20,AMIHASS_FAILED,"");amihass_client_close(&h->client);DeleteMsgPort(h->port);}memset(h,0,sizeof(*h));if(RexxSysBase){CloseLibrary(RexxSysBase);RexxSysBase=NULL;}}
