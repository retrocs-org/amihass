"""Native HTTP: curl-config parsing, request text, and response parsing (length, chunked, close, limits, 1xx, split feeds)."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "http_native.h"
#include "amihass_core.h"
static char got[70000];static unsigned long got_n;static int fail_sink;
static int sink(void *u,const char *d,unsigned long n){(void)u;if(fail_sink)return 0;memcpy(got+got_n,d,n);got_n+=n;return 1;}
/* Feed in pieces of `step` bytes to prove the parser is restartable. */
static int run(const char *text,unsigned long len,unsigned long limit,unsigned int step,struct AHHttpReader *r,int eof)
{unsigned long i=0,used,n;int rc=AH_HTTP_MORE;if(!len)len=strlen(text);ah_http_reader_init(r,limit);got_n=0;
 while(i<len&&rc==AH_HTTP_MORE){n=len-i<step?len-i:step;rc=ah_http_feed(r,text+i,n,&used,sink,NULL);i+=used;}
 if(rc==AH_HTTP_MORE&&eof)rc=ah_http_eof(r);return rc;}
int main(void){struct AmiHASSConnection c;struct AHHttpRequest q;char conf[AH_HTTP_CONFIG_BYTES],req[AH_HTTP_REQUEST_BYTES];unsigned long n;struct AHHttpReader r;unsigned int step;static char big[70000];
 memset(&c,0,sizeof(c));strcpy(c.url,"https://ha.example:8123/prefix");
 assert(amihass_request(&c,"test-only-token-for-local-fixture","/api/states/light.desk",NULL,conf,sizeof(conf)));
 assert(ah_http_conf_parse(conf,&q)&&q.tls&&q.port==8123&&!strcmp(q.host,"ha.example")&&!strcmp(q.path,"/prefix/api/states/light.desk")&&q.headers==2&&!q.post);
 n=ah_http_build(&q,req,sizeof(req));assert(n&&!strncmp(req,"GET /prefix/api/states/light.desk HTTP/1.1\r\nHost: ha.example:8123\r\n",65)&&strstr(req,"Authorization: Bearer test-only-token-for-local-fixture\r\n")&&strstr(req,"Connection: keep-alive")&&!strcmp(req+n-4,"\r\n\r\n"));
 assert(amihass_control_request(&c,"test-only-token-for-local-fixture","light.desk","Toggle",conf,sizeof(conf))&&ah_http_conf_parse(conf,&q)&&q.post&&q.body_len==strlen(q.body)&&strstr(q.body,"light.desk"));
 n=ah_http_build(&q,req,sizeof(req));assert(strstr(req,"POST /prefix/api/services/")&&strstr(req,"Content-Length: ")&&!strcmp(req+n-q.body_len,q.body));
 strcpy(c.url,"http://10.0.0.2");c.allow_http=1;assert(amihass_request(&c,"test-only-token-for-local-fixture","/api/",NULL,conf,sizeof(conf))&&ah_http_conf_parse(conf,&q)&&!q.tls&&q.port==80);n=ah_http_build(&q,req,sizeof(req));assert(strstr(req,"Host: 10.0.0.2\r\n"));
 assert(!ah_http_conf_parse("url = \"ftp://x/\"\n",&q)&&!ah_http_conf_parse("url = \"http://x/\"\nproxy = \"y\"\n",&q)&&!ah_http_conf_parse("url = \"http://a b/\"\n",&q)&&!ah_http_conf_parse("header = \"X: y\"\n",&q)&&!ah_http_conf_parse("url = \"http://x:99999/\"\n",&q));

 /* Pass real production discovery and batch bodies through the actual native
  * config parser and wire builder, not only short action requests. */
 {char body[4096],ids[AMIHASS_BATCH][96],known[AMIHASS_BATCH][40],token[1025];unsigned int k;int kind;unsigned long maximum=0;
 memset(token,'T',1024);token[1024]=0;strcpy(c.url,"https://ha.example:8123/prefix");c.allow_http=0;
 for(k=0;k<AMIHASS_BATCH;++k){sprintf(ids[k],"sensor.long_fixture_entity_%02u",k);strcpy(known[k],"2026-11-24T18:00:00.123456+00:00");}
 for(kind=0;kind<7;++kind){
  if(kind==0)assert(amihass_devices_request("",0,body,sizeof(body)));
  if(kind==1)assert(amihass_devices_scope_request("study","lighting","light","desk lamp",16,body,sizeof(body)));
  if(kind==2)assert(amihass_device_scope_page_request("0123456789abcdef0123456789abcdef","study","lighting","light","desk",0,body,sizeof(body)));
  if(kind==3)assert(amihass_areas_request(0,body,sizeof(body)));
  if(kind==4)assert(amihass_labels_request(0,body,sizeof(body)));
  if(kind==5)assert(amihass_states_request_known((const char(*)[96])ids,(const char(*)[40])known,AMIHASS_BATCH,body,sizeof(body)));
  if(kind==6){unsigned long prefix;strcpy(body,"{\"template\":\"");prefix=strlen(body);memset(body+prefix,'x',4093-prefix);strcpy(body+4093,"\"}");assert(strlen(body)==4095);}
  if(kind==0)assert(strlen(body)>1024);
  assert(amihass_request(&c,token,"/api/template",body,conf,sizeof(conf)));
  assert(ah_http_conf_parse(conf,&q)&&q.post&&q.body_len==strlen(body)&&!strcmp(q.body,body));
  n=ah_http_build(&q,req,sizeof(req));assert(n&&!memcmp(req+n-q.body_len,body,q.body_len)&&req[n]==0);
  if(q.body_len>maximum)maximum=q.body_len;
 }
 assert(maximum==4095);
 memset(body,'x',sizeof(body)-1);body[sizeof(body)-1]=0;
 sprintf(conf,"url = \"http://fixture.invalid/api/template\"\ndata = \"%sx\"\n",body);
 assert(!ah_http_conf_parse(conf,&q)); /* reject, never truncate */
 puts("Native HTTP: production devices, scoped entities, areas, labels, conditional batches and 4095-byte bodies survive config and wire round trips; oversized bodies rejected.");
 }
 for(step=1;step<=64;step*=4){
  assert(run("HTTP/1.1 200 OK\r\nContent-Type: x\r\nContent-Length: 5\r\n\r\nhello",0,100,step,&r,0)==AH_HTTP_DONE&&r.status==200&&got_n==5&&!memcmp(got,"hello",5)&&!r.close);
  assert(run("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5;x=1\r\npedia\r\n0\r\nX-T: 1\r\n\r\n",0,100,step,&r,0)==AH_HTTP_DONE&&got_n==9&&!memcmp(got,"Wikipedia",9));
  assert(run("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",0,100,step,&r,0)==AH_HTTP_DONE&&r.status==404&&r.close&&!got_n);
  assert(run("HTTP/1.0 200 OK\r\n\r\nuntil close",0,100,step,&r,1)==AH_HTTP_DONE&&got_n==11&&r.close);
  assert(run("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort",0,100,step,&r,1)==AH_HTTP_BAD);
 }
 assert(run("HTTP/1.1 200 OK\r\nContent-Length: 500\r\n\r\n",0,100,8,&r,0)==AH_HTTP_TOO_BIG);
 {static const char head[]="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nFFFF\r\n";memset(big,'a',sizeof(big));memcpy(big,head,sizeof(head)-1);assert(run(big,sizeof(head)-1+65535,1000,512,&r,0)==AH_HTTP_TOO_BIG);}
 assert(run("HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nx",0,100,4,&r,0)==AH_HTTP_BAD&&run("SSH-2.0\r\n",0,100,4,&r,0)==AH_HTTP_BAD&&run("HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\n",0,100,4,&r,0)==AH_HTTP_BAD);
 fail_sink=1;assert(run("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nx",0,100,64,&r,0)==AH_HTTP_SINK);fail_sink=0;
 assert(ah_http_curl_code(AH_HTTP_DONE,200)==0&&ah_http_curl_code(AH_HTTP_DONE,401)==22&&ah_http_curl_code(AH_HTTP_TOO_BIG,200)==63&&ah_http_curl_code(AH_HTTP_BAD,0)==52&&ah_http_curl_code(AH_HTTP_BAD,200)==56);
 puts("Native HTTP: curl config, request text, Content-Length, chunked, 1xx, close-delimited, limits, bad framing and split reads passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'t.c').write_text(program)
    cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'t.c'),str(root/'src/http_native.c'),str(root/'src/core.c'),'-o',str(p/'t')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'t')],check=True)
