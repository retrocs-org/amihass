/* Native read-only WebSocket worker. Secrets never appear in arguments/logs.
 * All action delivery and action confirmation remain on the REST scheduler. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "amihass.h"
#include "amihass_config.h"
#include "push_worker.h"
#include "nd_reply.h"
#include "ami2ha/net.h"
#include "ami2ha/ws.h"
#include "ami2ha/http.h"
struct Worker {const char *parent;unsigned long generation;struct AHPushMessage *selection;int ready,note_op;struct AHNote note;};
static struct AHPushMessage *exchange(struct Worker *w,int kind,const struct AmiHASSEntity *e,const struct AHControls *c,int error)
{struct AHPushMessage *m,*answer=NULL;struct MsgPort *reply,*parent;int i;reply=nd_reply_port();if(!reply)return NULL;m=AllocVec(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);if(!m){nd_reply_delete(reply);return NULL;}m->msg.mn_Length=sizeof(*m);m->msg.mn_ReplyPort=reply;m->magic=AH_PUSH_MAGIC;m->kind=kind;m->ready=w->ready;m->generation=w->generation;m->error=error;if(e)m->entity=*e;if(c)m->controls=*c;if(kind==4){m->note=w->note;m->note_op=w->note_op;}
 Forbid();parent=FindPort((STRPTR)w->parent);if(parent)PutMsg(parent,(struct Message*)m);Permit();if(!parent){FreeVec(m);nd_reply_delete(reply);return NULL;}for(i=0;i<50;++i){answer=(struct AHPushMessage*)GetMsg(reply);if(answer)break;Delay(1);}if(!answer){nd_reply_abandon(reply);return NULL;}nd_reply_delete(reply);return answer;}
static int update(void *user,const struct AmiHASSEntity *e,const struct AHControls *c)
{struct Worker *w=user;struct AHPushMessage *m=exchange(w,2,e,c,0);int ok=m&&!m->stop;if(m)FreeVec(m);return ok;}
static int note(void *user,int op,const struct AHNote *n)
{struct Worker *w=user;struct AHPushMessage *m;int ok;memset(&w->note,0,sizeof(w->note));if(n)w->note=*n;w->note_op=op;m=exchange(w,4,NULL,NULL,0);ok=m&&!m->stop;if(m)FreeVec(m);return ok;}
static int frame(a2h_buf *out,ws_opcode op,const void *p,size_t n)
{return ws_build_frame(out,op,p,n,(unsigned long)time(NULL)^(unsigned long)FindTask(NULL)^(unsigned long)out->len);}
static int subscribe(a2h_buf *out,const struct AHPushEntity *e)
{a2h_buf select;char head[64];int rc;buf_init(&select);sprintf(head,"{\"id\":%lu,\"type\":\"subscribe_entities\",\"entity_ids\":[",e->sub);buf_append_str(&select,head);buf_append_json_string(&select,e->id);buf_append_str(&select,"]}");rc=!select.failed&&frame(out,WS_OP_TEXT,select.data,select.len);buf_free(&select);return rc;}
/* Follow the service's watched set on the open connection: release dropped
 * entities and subscribe new ones, instead of reconnecting (TLS is costly). */
static int sync_selection(struct AHPushState *state,const struct AHPushMessage *m,a2h_buf *out,int live)
{unsigned int i;int k,wanted;unsigned long sub;char id[96],request[96];
 for(i=0;i<state->count;){wanted=0;for(k=0;k<m->count;++k)if(!strcmp(m->ids[k],state->entities[i].id)){wanted=1;break;}if(wanted){++i;continue;}
  strcpy(id,state->entities[i].id);if(!ah_push_remove(state,id,&sub))return 0;if(live){sprintf(request,"{\"id\":%lu,\"type\":\"unsubscribe_events\",\"subscription\":%lu}",state->next_id++,sub);if(!frame(out,WS_OP_TEXT,request,strlen(request)))return 0;}}
 for(k=0;k<m->count;++k){int at;if(ah_push_find(state,m->ids[k])>=0)continue;at=ah_push_add(state,m->ids[k]);if(at>=0&&live&&!subscribe(out,&state->entities[at]))return 0;}
 return 1;}
static int owned_subscription(const struct AHPushState *state,unsigned long id)
{unsigned int i;for(i=0;i<state->count;++i)if(state->entities[i].sub==id)return 1;return 0;}
int amihass_push_worker(const char *parent)
{struct Worker w;struct AmiHASSConfig *cfg=NULL;struct AHPushState *state=NULL;struct MsgPort *guard=NULL;struct AHPushMessage *m=NULL;struct AHPushMessage *first=NULL;a2h_socket sock;ws_stream stream;a2h_buf input,output,json;http_response response;ws_msg event;char host[160],path[200],key[25],type[32],packet[4096];const char *p,*colon,*slash,*value;unsigned int i;unsigned long id;unsigned long notes=0;int port,tls,rc,rd,wr,stage=0,error=0,opened=0,owns=0;time_t began,last_rx,last_ping,heartbeat;
 if(!parent||strncmp(parent,"AmiHASS.Push.",13)||strlen(parent)>64)return 20;memset(&w,0,sizeof(w));w.parent=parent;net_socket_init(&sock);ws_stream_init(&stream);buf_init(&input);buf_init(&output);buf_init(&json);((struct Process*)FindTask(NULL))->pr_WindowPtr=(APTR)-1;
 guard=CreateMsgPort();if(!guard)goto done;Forbid();if(!FindPort(AH_PUSH_GUARD)){guard->mp_Node.ln_Name=AH_PUSH_GUARD;AddPort(guard);owns=1;}Permit();if(!owns)goto done;
 first=exchange(&w,1,NULL,NULL,0);if(!first||first->stop||first->count<0||first->count>AH_PUSH_SELECTED)goto done;w.generation=first->generation;
 cfg=AllocVec(sizeof(*cfg),MEMF_PUBLIC|MEMF_CLEAR);state=AllocVec(sizeof(*state),MEMF_PUBLIC|MEMF_CLEAR);if(!cfg||!state||!amihass_config_load(cfg)||!cfg->push_updates||cfg->low_memory||cfg->sample_mode||!cfg->token[0]||!amihass_url(cfg->connection.url,cfg->connection.allow_http)||!ah_push_select(state,(const char (*)[96])first->ids,(unsigned int)first->count))goto done;
 FreeVec(first);first=NULL;
 /* scheme://host[:port][/prefix]; the WebSocket lives under the same prefix. */
 tls=!strncmp(cfg->connection.url,"https://",8);p=cfg->connection.url+(tls?8:7);slash=strchr(p,'/');colon=strchr(p,':');if(colon&&slash&&colon>slash)colon=NULL;i=colon?(unsigned int)(colon-p):slash?(unsigned int)(slash-p):(unsigned int)strlen(p);if(i>=sizeof(host))goto done;memcpy(host,p,i);host[i]=0;port=colon?atoi(colon+1):(tls?443:80);if(strlen(slash?slash:"")+15>=sizeof(path))goto done;sprintf(path,"%s/api/websocket",slash?slash:"");
 if(net_lib_open()!=NET_OK)goto failure;opened=1;net_tls_set_verify(1);rc=net_connect(&sock,host,port,tls);if(rc<0)goto failure;began=last_rx=last_ping=heartbeat=time(NULL);
 while(1){time_t now=time(NULL);if(now<began||now-last_rx>90||(!w.ready&&now-began>30))goto failure;
  if(now!=heartbeat){heartbeat=now;m=exchange(&w,1,NULL,NULL,0);if(!m||m->stop||m->generation!=w.generation||m->count<0||m->count>AH_PUSH_SELECTED){if(m)FreeVec(m);goto done;}rc=sync_selection(state,m,&output,stage>=4);FreeVec(m);if(!rc)goto failure;}
  net_wait(&sock,output.len!=0||net_want_write(&sock),SIGBREAKF_CTRL_C,1000,&rd,&wr);if(SetSignal(0L,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)goto done;
  if(net_connect_pending(&sock)){if(!rd&&!wr)continue;rc=net_connect_done(&sock);if(rc<0)goto failure;if(rc!=NET_OK)continue;}
  if(stage==0){ws_make_key(key,(unsigned long)time(NULL)^(unsigned long)FindTask(NULL));if(!http_build_ws_upgrade(&output,host,port,path,key))goto failure;stage=1;}
  if(output.len){long sent=net_send(&sock,output.data,output.len);if(sent<0)goto failure;if(sent>0)buf_consume(&output,(size_t)sent);}
  if(rd||net_pending(&sock)){long n=net_recv(&sock,packet,sizeof(packet));if(n<0)goto failure;if(n>0){last_rx=now;if(!buf_append(&input,packet,(size_t)n))goto failure;}}
  if(stage==1){rc=http_parse_response(buf_cstr(&input),input.len,&response);if(rc<0||input.len>8192)goto failure;if(!rc)continue;if(response.status!=101||!response.has_upgrade||!response.has_connection_upgrade||!ws_check_accept(key,response.accept))goto failure;buf_consume(&input,response.header_len);stage=2;}
  while((rc=ws_stream_next(&stream,&input,&event))!=WS_EV_NONE){if(event.type==WS_EV_SKIPPED)continue; /* one oversized entity stays on REST */
   if(event.type==WS_EV_ERROR||event.type==WS_EV_CLOSE)goto failure;if(event.type==WS_EV_PING){if(!frame(&output,WS_OP_PONG,event.data,event.len))goto failure;continue;}if(event.type==WS_EV_PONG)continue;if(event.type!=WS_EV_MESSAGE||event.op!=WS_OP_TEXT||memchr(event.data,0,event.len))goto failure;
   buf_reset(&json);if(!buf_append(&json,event.data,event.len))goto failure;p=buf_cstr(&json);value=ah_json_end(p);if(!value||strspn(value," \r\n\t")!=strlen(value)||!ah_json_text(ah_json_member(p,"type"),type,sizeof(type),1))goto failure;
   if(stage==2&&!strcmp(type,"auth_required")){a2h_buf auth;buf_init(&auth);buf_append_str(&auth,"{\"type\":\"auth\",\"access_token\":");buf_append_json_string(&auth,cfg->token);buf_append_str(&auth,"}");rc=!auth.failed&&frame(&output,WS_OP_TEXT,auth.data,auth.len);if(auth.data)amihass_erase(auth.data,auth.cap);buf_free(&auth);amihass_erase(cfg->token,sizeof(cfg->token));if(!rc)goto failure;stage=3;}
   else if(!strcmp(type,"auth_invalid")){error=1;goto failure;}
   else if(stage==3&&!strcmp(type,"auth_ok")){for(i=0;i<state->count;++i)if(!subscribe(&output,&state->entities[i]))goto failure;
    /* Persistent notifications ride on the same connection. */notes=state->next_id++;sprintf(packet,"{\"id\":%lu,\"type\":\"persistent_notification/subscribe\"}",notes);if(!frame(&output,WS_OP_TEXT,packet,strlen(packet)))goto failure;stage=4;}
   else if(stage>=4&&!strcmp(type,"result")){value=ah_json_member(p,"success");colon=ah_json_member(p,"id");id=colon?strtoul(colon,NULL,10):0;
    /* A refused subscription leaves that entity on REST; others continue. */
    if(value&&!strncmp(value,"true",4)&&(owned_subscription(state,id)||id==notes)){w.ready=1;stage=5;}}
   else if(stage>=4&&!strcmp(type,"event")){colon=ah_json_member(p,"id");id=colon?strtoul(colon,NULL,10):0;if(notes&&id==notes){if(!ah_push_notifications(ah_json_member(p,"event"),note,&w))goto failure;}else if(!ah_push_event(state,p,update,&w))goto failure;}
   else if(stage>=4&&!strcmp(type,"pong"))continue;
   else goto failure;
  }
  if(now-last_ping>=30){if(!frame(&output,WS_OP_PING,"",0))goto failure;last_ping=now;}
 }
failure:m=exchange(&w,3,NULL,NULL,error);if(m)FreeVec(m);
done:net_disconnect(&sock);if(opened)net_lib_close();if(output.data)amihass_erase(output.data,output.cap);buf_free(&output);buf_free(&input);buf_free(&json);ws_stream_free(&stream);if(cfg){amihass_erase(cfg,sizeof(cfg[0]));FreeVec(cfg);}if(state)FreeVec(state);if(first)FreeVec(first);if(guard){if(owns){Forbid();RemPort(guard);Permit();}DeleteMsgPort(guard);}return 0;}
