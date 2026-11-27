/* AmiHASSService HTTP: the native HTTP worker. One persistent process keeps
 * a connection to Home Assistant open between requests (HTTP/1.1
 * keep-alive, TLS through AmiSSL), so a request no longer starts curl and
 * repeats the TCP and TLS handshakes. It reads the same request.conf the
 * service writes for curl and answers in the same shape (response file,
 * HTTP code in fetch.log, curl-style exit code), so the service handles the
 * result exactly as before. The service falls back to curl when this
 * worker is missing or fails at the TLS level. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nd_reply.h"
#include "nd_interaction.h"
#define FETCH_PROGRESS_FILE "request.conf"
#include "fetch_worker.h"
#include "http_native.h"
#include "ami2ha/net.h"
#define AH_HTTP_IDLE 45 /* Home Assistant (aiohttp) drops idle connections after about 75 s */
struct Conn {a2h_socket s;char host[128];int port,tls,open;time_t used;};
static char request_text[AH_HTTP_REQUEST_BYTES],conf_text[AH_HTTP_CONFIG_BYTES],packet[2048];
static int file_sink(void *user,const char *data,unsigned long n){return fwrite(data,1,(size_t)n,(FILE *)user)==(size_t)n;}
static void conn_close(struct Conn *c){if(c->open)net_disconnect(&c->s);net_socket_init(&c->s);c->open=0;}
static int curl_for(int net){return net==NET_ERR_HOST?6:net==NET_ERR_TLS?35:net==NET_ERR_CERT?60:7;}
/* Connect (and with TLS, handshake) before the deadline; a curl exit code. */
static int conn_open(struct Conn *c,const struct AHHttpRequest *q,time_t deadline)
{int rc,rd,wr;conn_close(c);rc=net_connect(&c->s,q->host,q->port,q->tls);if(rc<0){conn_close(c);return curl_for(rc);}
 while(rc!=NET_OK){if(time(NULL)>deadline){conn_close(c);return 28;}net_wait(&c->s,net_want_write(&c->s)||net_connect_pending(&c->s),SIGBREAKF_CTRL_C,500,&rd,&wr);
  if(SetSignal(0L,0L)&SIGBREAKF_CTRL_C){conn_close(c);return 42;}rc=net_connect_done(&c->s);if(rc<0){conn_close(c);return curl_for(rc);}}
 strcpy(c->host,q->host);c->port=q->port;c->tls=q->tls;c->open=1;return 0;}
/* One request on the kept connection. Returns a curl exit code and sets the
 * HTTP status. A kept connection that the server already closed is opened
 * again once, but only when not a byte of the answer had arrived. */
static int exchange(struct Conn *c,const struct AHHttpRequest *q,const char *directory,unsigned long limit,int connect_s,int max_s,int *status)
{
 unsigned long length,sent,used;struct AHHttpReader r;char path[200];FILE *out;long n;int attempt,rc,rd,wr,result,received;time_t begin=time(NULL),deadline=begin+max_s;
 *status=0;length=ah_http_build(q,request_text,sizeof(request_text));if(!length)return 3;
 for(attempt=0;attempt<2;++attempt){int reused=c->open&&!strcmp(c->host,q->host)&&c->port==q->port&&c->tls==q->tls;
  if(!reused){rc=conn_open(c,q,begin+connect_s);if(rc)return rc;}
  sprintf(path,"%s/response",directory);out=fopen(path,"wb");if(!out){conn_close(c);return 23;}
  sent=0;received=0;ah_http_reader_init(&r,limit);result=AH_HTTP_MORE;
  while(sent<length){n=net_send(&c->s,request_text+sent,length-sent);if(n>0){sent+=(unsigned long)n;continue;}
   if(n==NET_WOULDBLOCK){if(time(NULL)>deadline)break;net_wait(&c->s,1,SIGBREAKF_CTRL_C,500,&rd,&wr);continue;}break;}
  while(sent==length&&result==AH_HTTP_MORE){
   n=net_recv(&c->s,packet,sizeof(packet));
   if(n>0){received=1;rc=ah_http_feed(&r,packet,(unsigned long)n,&used,file_sink,out);if(rc!=AH_HTTP_MORE)result=rc;continue;}
   if(n==NET_CLOSED){result=received?ah_http_eof(&r):AH_HTTP_BAD;break;}
   if(n==NET_ERROR){result=AH_HTTP_BAD;break;}
   if(time(NULL)>deadline){fclose(out);conn_close(c);*status=r.status;return 28;}
   net_wait(&c->s,net_want_write(&c->s),SIGBREAKF_CTRL_C,500,&rd,&wr);if(SetSignal(0L,0L)&SIGBREAKF_CTRL_C){fclose(out);conn_close(c);return 42;}}
  if(fclose(out))result=AH_HTTP_SINK;
  if(sent==length&&result==AH_HTTP_DONE){*status=r.status;if(r.close)conn_close(c);else c->used=time(NULL);return ah_http_curl_code(result,r.status);}
  conn_close(c);if(sent==length&&received){*status=r.status;return ah_http_curl_code(result,r.status);}
  if(!reused)return sent==length?52:55;/* a fresh connection that failed is a real failure */}
 return 52;
}
static void answer(const char *job,const char *directory,int result)
{struct FetchMessage *m=AllocVec(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);if(!m)return;m->magic=FETCH_MAGIC;m->kind=2;m->result=result;m->msg.mn_Length=sizeof(*m);strcpy(m->directory,directory);if(!fetch_send(job,m)){FreeVec(m);fetch_clean(directory);/* the service gave up on this job */}}
/* command: "<job port> <limit> <connect s> <max s> <allow http>" */
static void serve(struct Conn *c,struct FetchMessage *m)
{
 char job[80],path[200];unsigned long limit;int connect_s,max_s,allow_http,status=0,result;FILE *f;size_t n;struct AHHttpRequest *q;
 if(sscanf(m->command,"%70s %lu %d %d %d",job,&limit,&connect_s,&max_s,&allow_http)!=5||strncmp(job,"AmiHASS.Job.",12)||!FETCH_DIR_VALID(m->directory)||strlen(m->directory)>150)return;
 if(connect_s<1||connect_s>300)connect_s=10;if(max_s<1||max_s>600)max_s=30;
 result=3;q=AllocVec(sizeof(*q),MEMF_PUBLIC|MEMF_CLEAR);
 sprintf(path,"%s/request.conf",m->directory);f=fopen(path,"rb");
 if(f&&q){n=fread(conf_text,1,sizeof(conf_text)-1,f);conf_text[n]=0;if(!ferror(f)&&fgetc(f)==EOF&&ah_http_conf_parse(conf_text,q)){
   if(!q->tls&&!allow_http)result=1;else result=exchange(c,q,m->directory,limit,connect_s,max_s,&status);}}
 if(f)fclose(f);memset(conf_text,0,sizeof(conf_text));memset(request_text,0,sizeof(request_text));if(q){memset(q,0,sizeof(*q));FreeVec(q);}
 sprintf(path,"%s/fetch.log",m->directory);f=fopen(path,"wb");if(f){fprintf(f,"%03d",status);fclose(f);}
 answer(job,m->directory,result);
}
int amihass_http_worker(const char *service)
{
 struct MsgPort *port;struct FetchMessage *m;struct Conn c;int owner=0,quit=0,rd;unsigned long got;time_t now,checked=0;
 if(!service||strcmp(service,"AmiHASS.Service"))return 20;((struct Process *)FindTask(NULL))->pr_WindowPtr=(APTR)-1;
 if(net_lib_open()!=NET_OK)return 10;net_tls_set_verify(1);net_socket_init(&c.s);c.open=0;
 port=CreateMsgPort();if(!port){net_lib_close();return 20;}
 Forbid();if(!FindPort(AH_HTTP_GUARD)){port->mp_Node.ln_Name=AH_HTTP_GUARD;AddPort(port);owner=1;}Permit();
 if(!owner){DeleteMsgPort(port);net_lib_close();return 10;}
 while(!quit){
  got=net_wait2(c.open?&c.s:NULL,0,NULL,0,(1UL<<port->mp_SigBit)|SIGBREAKF_CTRL_C,1000,&rd,NULL,NULL,NULL);
  if(got&SIGBREAKF_CTRL_C)quit=1;
  /* Readable while idle: the server closed the kept connection (or sent
   * something unasked); either way it cannot carry the next request. */
  if(c.open&&rd)conn_close(&c);
  while(!quit&&(m=(struct FetchMessage *)GetMsg(port))!=NULL){if(m->magic==FETCH_MAGIC&&m->msg.mn_Length==sizeof(*m)){if(m->kind==3)serve(&c,m);else if(m->kind==4)quit=1;}FreeVec(m);}
  now=time(NULL);if(c.open&&(now<c.used||now-c.used>AH_HTTP_IDLE))conn_close(&c);
  if(now!=checked){struct MsgPort *p;checked=now;Forbid();p=FindPort((STRPTR)service);Permit();if(!p)quit=1;}}
 Forbid();RemPort(port);Permit();
 /* Requests that arrived meanwhile get an immediate failure, not a timeout. */
 while((m=(struct FetchMessage *)GetMsg(port))!=NULL){char job[80];if(m->kind==3&&sscanf(m->command,"%70s",job)==1)answer(job,m->directory,7);FreeVec(m);}
 DeleteMsgPort(port);conn_close(&c);tls_lib_close();net_lib_close();return 0;
}
