/* See http_native.h. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_native.h"
/* "key = \"value\"" with \" and \\ escapes, as amihass_request writes. */
static int conf_value(const char *p,const char *end,char *out,unsigned long cap,unsigned long *len)
{unsigned long n=0;if(p==end||*p++!='"')return 0;while(p<end&&*p!='"'){if(*p=='\\'){++p;if(p==end)return 0;}if(n+1>=cap)return 0;out[n++]=*p++;}if(p==end||p+1!=end)return 0;out[n]=0;if(len)*len=n;return 1;}
static int url_parse(const char *url,struct AHHttpRequest *r)
{const char *p,*slash,*colon;unsigned long n;char *end;long port;
 if(!strncmp(url,"https://",8)){r->tls=1;p=url+8;}else if(!strncmp(url,"http://",7)){r->tls=0;p=url+7;}else return 0;
 slash=strchr(p,'/');if(!slash)return 0;colon=memchr(p,':',(size_t)(slash-p));n=(unsigned long)((colon?colon:slash)-p);if(!n||n>=sizeof(r->host))return 0;memcpy(r->host,p,n);r->host[n]=0;
 for(n=0;r->host[n];++n)if(!isalnum((unsigned char)r->host[n])&&!strchr(".-[]",r->host[n]))return 0;
 if(colon){port=strtol(colon+1,&end,10);if(end!=slash||port<1||port>65535)return 0;r->port=(int)port;}else r->port=r->tls?443:80;
 if(strlen(slash)>=sizeof(r->path))return 0;strcpy(r->path,slash);for(n=0;r->path[n];++n)if((unsigned char)r->path[n]<=32||(unsigned char)r->path[n]>126)return 0;return 1;}
int ah_http_conf_parse(const char *conf,struct AHHttpRequest *r)
{
 char value[1100];const char *p=conf,*e;unsigned long n;int url=0;memset(r,0,sizeof(*r));
 /* Parse bounded spans directly. A valid template line is longer than 1200
  * bytes; copying it to a short stack buffer rejected Browser discovery. */
 if(!conf||strlen(conf)>=AH_HTTP_CONFIG_BYTES)return 0;
 while(*p){e=strchr(p,'\n');if(!e)e=p+strlen(p);n=(unsigned long)(e-p);if(!n){p=*e ? e+1:e;continue;}
  if(n>=6&&!strncmp(p,"url = ",6)){if(url||!conf_value(p+6,e,value,sizeof(value),NULL)||!url_parse(value,r))return 0;url=1;}
  else if(n>=9&&!strncmp(p,"header = ",9)){if(r->headers>=AH_HTTP_HEADERS||!conf_value(p+9,e,r->header[r->headers],sizeof(r->header[0]),NULL)||!strchr(r->header[r->headers],':'))return 0;for(n=0;r->header[r->headers][n];++n)if((unsigned char)r->header[r->headers][n]<32)return 0;++r->headers;}
  else if(n==16&&!strncmp(p,"request = \"POST\"",16))r->post=1;
  else if(n>=7&&!strncmp(p,"data = ",7)){if(!conf_value(p+7,e,r->body,sizeof(r->body),&r->body_len))return 0;r->post=1;}
  else return 0;
  p=*e ? e+1:e;}
 return url;
}
unsigned long ah_http_build(const struct AHHttpRequest *r,char *out,unsigned long cap)
{unsigned long n;unsigned int i;char host[140];
 if(r->port==(r->tls?443:80))strcpy(host,r->host);else sprintf(host,"%s:%d",r->host,r->port);
 n=strlen(r->path)+strlen(host)+r->body_len+160;for(i=0;i<r->headers;++i)n+=strlen(r->header[i])+2;if(n>=cap)return 0;
 sprintf(out,"%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: AmiHASS/0.3\r\nAccept: */*\r\nAccept-Encoding: identity\r\nConnection: keep-alive\r\n",r->post?"POST":"GET",r->path,host);
 for(i=0;i<r->headers;++i){strcat(out,r->header[i]);strcat(out,"\r\n");}
 if(r->post)sprintf(out+strlen(out),"Content-Length: %lu\r\n",r->body_len);strcat(out,"\r\n");n=strlen(out);if(r->post){memcpy(out+n,r->body,r->body_len);n+=r->body_len;}out[n]=0;return n;}
void ah_http_reader_init(struct AHHttpReader *r,unsigned long limit){memset(r,0,sizeof(*r));r->limit=limit;r->length=(unsigned long)-1;}
static int ci_prefix(const char *s,const char *p){while(*p){if(tolower((unsigned char)*s)!=*p)return 0;++s;++p;}return 1;}
/* stage: 0 status line, 1 headers, 2 body by length, 3 chunk size, 4 chunk data, 5 chunk CRLF, 6 trailers, 7 until close, 8 done */
static int header_line(struct AHHttpReader *r)
{char *v,*end;if(r->stage==0){if(strncmp(r->line,"HTTP/1.",7)||!isdigit((unsigned char)r->line[9])||r->line[8]!=' ')return 0;r->status=atoi(r->line+9);if(r->status<100||r->status>599)return 0;if(r->line[7]=='0')r->close=1;r->stage=1;return 1;}
 if(!r->line[0]){/* end of headers */if(r->status/100==1||r->status==204||r->status==304){r->stage=r->status/100==1?0:8;r->length=0;return 1;}
  if(r->chunked)r->stage=3;else if(r->length!=(unsigned long)-1&&r->head){r->left=r->length;if(r->length>r->limit)return -2;r->stage=r->left?2:8;}else{r->stage=7;r->until_close=1;r->close=1;}return 1;}
 v=strchr(r->line,':');if(!v)return 0;++v;while(*v==' '||*v=='\t')++v;
 if(ci_prefix(r->line,"content-length:")){unsigned long n=strtoul(v,&end,10);if(end==v||*end)return 0;if(r->head&&r->length!=n)return 0;r->length=n;r->head=1;}
 else if(ci_prefix(r->line,"transfer-encoding:")){if(!ci_prefix(v,"chunked"))return 0;r->chunked=1;}
 else if(ci_prefix(r->line,"connection:")&&ci_prefix(v,"close"))r->close=1;
 return 1;}
int ah_http_feed(struct AHHttpReader *r,const char *data,unsigned long n,unsigned long *used,AHHttpSink sink,void *user)
{unsigned long i=0,take;int rc;
 while(i<n&&r->stage!=8){
  if(r->stage==2||r->stage==4||r->stage==7){take=n-i;if(r->stage!=7&&take>r->left)take=r->left;if(r->body+take>r->limit){*used=i;return AH_HTTP_TOO_BIG;}
   if(!sink(user,data+i,take)){*used=i;return AH_HTTP_SINK;}r->body+=take;i+=take;if(r->stage!=7){r->left-=take;if(!r->left)r->stage=r->stage==2?8:5;}continue;}
  /* line-based stages */
  if(data[i]=='\n'){if(r->used&&r->line[r->used-1]=='\r')--r->used;r->line[r->used]=0;r->used=0;++i;
   if(r->stage<=1){rc=header_line(r);if(rc==-2){*used=i;return AH_HTTP_TOO_BIG;}if(!rc){*used=i;return AH_HTTP_BAD;}if(r->stage==0){r->length=(unsigned long)-1;r->head=0;r->chunked=0;}}
   else if(r->stage==3){char *end;unsigned long size=strtoul(r->line,&end,16);if(end==r->line||(*end&&*end!=';'&&*end!=' ')){*used=i;return AH_HTTP_BAD;}r->left=size;r->stage=size?4:6;}
   else if(r->stage==5){if(r->line[0]){*used=i;return AH_HTTP_BAD;}r->stage=3;}
   else if(r->stage==6){if(!r->line[0])r->stage=8;}
   continue;}
  if(r->used+1>=sizeof(r->line)){*used=i;return AH_HTTP_BAD;}r->line[r->used++]=data[i++];}
 *used=i;return r->stage==8?AH_HTTP_DONE:AH_HTTP_MORE;}
int ah_http_eof(struct AHHttpReader *r){if(r->stage==7){r->stage=8;return AH_HTTP_DONE;}return r->stage==8?AH_HTTP_DONE:AH_HTTP_BAD;}
int ah_http_curl_code(int result,int status)
{if(result==AH_HTTP_TOO_BIG)return 63;if(result==AH_HTTP_SINK)return 23;if(result!=AH_HTTP_DONE)return status?56:52;return status>=400?22:0;}
