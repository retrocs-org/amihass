/* Read-only Browser provider. All server access goes through the public SDK. */
#include <string.h>
#include <ctype.h>
#include <proto/exec.h>
#include "browser_live.h"
static void changed_text(char *dest,const char *text,int *dirty)
{if(strcmp(dest,text)){strcpy(dest,text);*dirty=1;}}
static void drop_request(struct AHBrowserLive *p)
{amihass_client_close(&p->client);p->sent_kind=0;}
void ah_browser_live_init(struct AHBrowserLive *p){memset(p,0,sizeof(*p));p->selected=-1;p->page_size=AMIHASS_PAGE;}
void ah_browser_live_close(struct AHBrowserLive *p){drop_request(p);}
static int request_page(struct AHBrowserLive *p,int devices,const char *device,const char *domain,const char *filter,int offset,int refresh)
{
 const unsigned char *c;char domain_copy[24],filter_copy[64],device_copy[96];
 if(!device)device="";if(*device && !amihass_device_id(device))return 0;
 if(!domain)domain="";if(!strcmp(domain,"All"))domain="";if(!filter)filter="";
 if(!amihass_area_valid(p->area)||strlen(domain)>23 || strlen(filter)>63 || offset<0 || offset>100000)return 0;
 for(c=(const unsigned char *)domain;*c;++c)if(!((*c>='a'&&*c<='z') || isdigit(*c) || *c=='_'))return 0;
 strcpy(device_copy,device);strcpy(domain_copy,domain);strcpy(filter_copy,filter);drop_request(p);strcpy(p->domain,domain_copy);strcpy(p->filter,filter_copy);p->offset=offset;p->refresh=refresh;p->page_refresh=refresh;p->devices=devices;strcpy(p->device,device_copy);
 p->count=p->total=0;p->selected=-1;p->wanted=devices ? AMIHASS_DEVICES:AMIHASS_LIST;p->loading=1;p->detail_loading=0;p->detail_result=0;p->terminal_error=0;p->error[0]=0;p->next=p->ticks;p->dirty=1;return 1;
}
int ah_browser_live_page(struct AHBrowserLive *p,const char *domain,const char *filter,int offset,int refresh)
{return request_page(p,0,"",domain,filter,offset,refresh);}
int ah_browser_live_devices_type(struct AHBrowserLive *p,const char *domain,const char *filter,int offset,int refresh)
{return request_page(p,1,"",domain,filter,offset,refresh);}
int ah_browser_live_devices(struct AHBrowserLive *p,const char *filter,int offset,int refresh)
{return request_page(p,1,"","",filter,offset,refresh);}
int ah_browser_live_device_page(struct AHBrowserLive *p,const char *device,const char *domain,const char *filter,int offset,int refresh)
{return request_page(p,0,device,domain,filter,offset,refresh);}
void ah_browser_live_details(struct AHBrowserLive *p,int index)
{
 if(p->devices || index<0 || index>=p->count)return;drop_request(p);strcpy(p->entity,p->items[index].id);p->detail=p->items[index];memset(&p->controls,0,sizeof(p->controls));p->age=0;
 p->wanted=AMIHASS_QUERY;p->detail_loading=1;p->detail_result=AMIHASS_WAIT;p->terminal_error=0;p->error[0]=0;p->next=p->ticks;p->dirty=1;
}
void ah_browser_live_cancel_details(struct AHBrowserLive *p)
{drop_request(p);p->wanted=0;p->detail_loading=0;p->next=p->ticks;p->dirty=1;}
static int send_kind(struct AHBrowserLive *p,int kind)
{
 if(kind==AMIHASS_LIST||kind==AMIHASS_DEVICES){if(!amihass_send_scope(&p->client,kind,"AmiHASS.Browser",kind==AMIHASS_LIST ? p->device:NULL,p->offset+p->count,p->refresh,p->domain,p->filter,p->area,p->label))return 0;}
 else if(!amihass_send(&p->client,kind,"AmiHASS.Browser",kind==AMIHASS_QUERY ? p->entity:kind==AMIHASS_LIST ? p->device:NULL,NULL,(kind==AMIHASS_LIST || kind==AMIHASS_DEVICES) ? p->offset+p->count:p->offset,(kind==AMIHASS_LIST || kind==AMIHASS_DEVICES) ? p->refresh:kind==AMIHASS_QUERY ? AMIHASS_QUERY_ONCE:0,p->domain,p->filter))return 0;
 p->sent_kind=kind;if(kind==AMIHASS_LIST || kind==AMIHASS_DEVICES)p->refresh=0;return 1;
}
static void no_service(struct AHBrowserLive *p)
{p->running=p->connected=p->loading=0;p->terminal_error=0;p->sent_kind=0;p->next=p->ticks+10;changed_text(p->error,"Service stopped - start it in Preferences",&p->dirty);}
static void clean_entity(struct AmiHASSEntity *e)
{e->id[95]=e->name[63]=e->state[39]=e->unit[15]=e->device_class[23]=e->updated[39]=0;}
void ah_browser_live_step(struct AHBrowserLive *p)
{
 struct AmiHASSMessage *m;int kind,i;++p->ticks;
 if(p->sent_kind){
  m=amihass_reply(&p->client);
  if(!m){if(p->client.pending)return;p->sent_kind=0;p->connected=0;p->next=p->ticks+10;changed_text(p->error,"Bridge request timed out - retrying",&p->dirty);return;}
  kind=p->sent_kind;p->sent_kind=0;
  if(m->kind!=kind){FreeVec(m);p->next=p->ticks+10;changed_text(p->error,"Unexpected bridge reply",&p->dirty);return;}
  if(kind==AMIHASS_STATUS){
   strncpy(p->server,m->status.server,sizeof(p->server)-1);p->server[sizeof(p->server)-1]=0;
   p->running=1;if(p->connected!=(m->status.connected!=0)){p->connected=m->status.connected!=0;p->dirty=1;}
   m->item.state[39]=0;
   if(p->sample_mode!=(!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE))){p->sample_mode=!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE);if(p->server_state[0]){p->count=p->total=p->offset=0;p->label[0]=p->device[0]=p->domain[0]=p->filter[0]=0;p->devices=1;p->wanted=AMIHASS_DEVICES;p->loading=1;p->detail_loading=0;p->detail_result=AMIHASS_FAILED;p->dirty=1;}}
   changed_text(p->server_state,m->item.state,&p->dirty);
   if(!p->connected){
    p->terminal_error=0;changed_text(p->error,!strcmp(p->server_state,AMIHASS_SERVER_UNCONFIGURED)||!m->status.server[0] ? "Configure the server in Preferences":!strcmp(p->server_state,AMIHASS_SERVER_NETWORK_DOWN) ? "TCP/IP network is not running":!strcmp(p->server_state,AMIHASS_SERVER_AUTH_REJECTED) ? "Token rejected - update Server Settings or retry":!strcmp(p->server_state,AMIHASS_SERVER_CONNECTING) ? "Connecting to Home Assistant...":!strcmp(p->server_state,AMIHASS_SERVER_RETRY_WAIT) ? "Home Assistant disconnected - waiting for retry":"Home Assistant is not connected",&p->dirty);
    p->next=p->ticks+10;FreeVec(m);return;
   }
   if(!p->terminal_error)changed_text(p->error,"",&p->dirty);FreeVec(m);
   if(p->wanted){if(p->wanted==AMIHASS_LIST || p->wanted==AMIHASS_DEVICES)p->loading=1;if(!send_kind(p,p->wanted))no_service(p);}else p->next=p->ticks+50;return;
  }
  if(kind==AMIHASS_LIST || kind==AMIHASS_DEVICES){
   if(m->result==AMIHASS_OK && m->count>=0 && m->count<=AMIHASS_PAGE && m->total>=m->count && m->total<=100000){
    int take=m->count,limit=p->page_size;if(limit<1 || limit>32)limit=AMIHASS_PAGE;
    if(m->offset!=p->offset+p->count){FreeVec(m);p->wanted=0;p->loading=0;p->terminal_error=1;changed_text(p->error,"Catalogue page changed - refresh",&p->dirty);return;}
    if(take>limit-p->count)take=limit-p->count;p->total=m->total;
    for(i=0;i<take;++i){p->items[p->count+i]=m->items[i];clean_entity(&p->items[p->count+i]);}p->count+=take;p->selected=p->count ? 0:-1;
    if(take && p->count<limit && p->offset+p->count<p->total){p->refresh=p->page_refresh;p->next=p->ticks;}
    else{p->wanted=0;p->loading=0;p->next=p->ticks+50;}p->dirty=1;
   }else if(m->result==AMIHASS_WAIT)p->next=p->ticks+5;
   else{p->wanted=0;p->terminal_error=1;p->loading=0;p->next=p->ticks+10;m->item.name[63]=0;changed_text(p->error,m->result==AMIHASS_FAILED&&m->item.name[0] ? m->item.name:amihass_problem(m->result),&p->dirty);}
  }else if(kind==AMIHASS_QUERY){
   if(m->result==AMIHASS_OK || m->result==AMIHASS_UNAVAILABLE || m->result==AMIHASS_STALE){
    amihass_controls_items(m->items,m->count,&p->controls);p->detail=m->item;clean_entity(&p->detail);p->age=m->age;p->detail_result=m->result;p->detail_loading=0;p->wanted=0;p->next=p->ticks+50;p->dirty=1;
    for(i=0;i<p->count;++i)if(!strcmp(p->items[i].id,p->detail.id))p->items[i]=p->detail;
   }else if(m->result==AMIHASS_WAIT)p->next=p->ticks+5;
   else{p->wanted=0;p->terminal_error=1;p->detail_loading=0;p->detail_result=m->result;p->next=p->ticks+10;changed_text(p->error,amihass_problem(m->result),&p->dirty);}
  }
  FreeVec(m);return;
 }
 if(p->ticks>=p->next && !send_kind(p,AMIHASS_STATUS))no_service(p);
}
