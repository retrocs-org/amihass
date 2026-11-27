/* Native shared connection setup and an asynchronous paged entity chooser. */
#define IntuitionBase AmiHASSIntuitionBase
#define GadToolsBase AmiHASSGadToolsBase
#define AslBase AmiHASSAslBase
#include <exec/types.h>
#include <dos/dos.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/asl.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ah_locale.h"
#include "service.h"
#include "wait.h"
#include "amihass_config.h"
#include "nd_panel.h"
#include "server_profiles.h"
#include "preferences.h"
#include "ah_log.h"
#include "connectivity.h"
struct IntuitionBase *IntuitionBase=NULL;
struct Library *GadToolsBase=NULL,*AslBase=NULL;
static int token_file(char token[1025])
{
 struct FileRequester *fr;char path[520],raw[1030];FILE *f;size_t n;int ok=0;
 AslBase=OpenLibrary("asl.library",39);if(!AslBase)return 0;
 fr=AllocAslRequestTags(ASL_FileRequest,ASLFR_TitleText,(ULONG)"Import Home Assistant token text file",TAG_DONE);
 if(fr){if(AslRequest(fr,NULL) && strlen(fr->fr_Drawer)<500){strcpy(path,fr->fr_Drawer);if(AddPart(path,fr->fr_File,sizeof(path))){f=fopen(path,"rb");if(f){n=fread(raw,1,sizeof(raw)-1,f);raw[n]=0;if(!ferror(f) && fgetc(f)==EOF && !memchr(raw,0,n))ok=amihass_token(raw,token);fclose(f);}}}FreeAslRequest(fr);}
 amihass_erase(raw,sizeof(raw));CloseLibrary(AslBase);AslBase=NULL;return ok;
}
/* Server Settings saves explicitly; General Settings edits the main draft. */
/* Connection drafts stay in memory until Save; selecting a sidebar row
 * does not switch the running service or write its credentials. */
static void server_draft_read(struct AHServerProfile *item,struct NDPanel *panel)
{strncpy(item->name,nd_panel_get(panel,4),32);item->name[32]=0;strncpy(item->url,nd_panel_get(panel,0),159);item->url[159]=0;amihass_erase(item->token,sizeof(item->token));strncpy(item->token,nd_panel_get(panel,1),1024);item->token[1024]=0;item->reconnect=nd_panel_reconnect(panel);item->retry_interval=nd_panel_retry(panel);}
static void server_draft_new(struct AHServerProfiles *list,unsigned int *selected,const struct AmiHASSPreferences *prefs,int current)
{struct AHServerProfile *item;unsigned int n;char name[33];if(list->count>=AH_SERVERS)return;
 *selected=list->count++;item=&list->items[*selected];memset(item,0,sizeof(*item));
 for(n=1;n<=AH_SERVERS+1;++n){sprintf(name,"Connection %u",n);if(ah_servers_find(list,name)<0)break;}strcpy(item->name,name);
 strcpy(item->url,current ? prefs->connection.url:"https://homeassistant.local:8123");if(current)strcpy(item->token,prefs->token);item->reconnect=current ? prefs->reconnect:1;item->retry_interval=current ? prefs->retry_interval:60;}
int amihass_connection_edit(struct AmiHASSPreferences *prefs)
{
 struct NDPanel *panel=NULL;struct NDPanelField fields[5];struct AmiHASSConnection candidate;struct AHServerProfiles *draft,*stored;struct AHServerProfile *item;
 const char *names[AH_SERVERS];char original[AH_SERVERS][33],token[1025];unsigned int selected=0,i;int action,result=0,forget=prefs->forget,test,delete_pending=-1,match=0,forgotten[AH_SERVERS];
 draft=AllocVec(sizeof(*draft),MEMF_PUBLIC|MEMF_CLEAR);stored=AllocVec(sizeof(*stored),MEMF_PUBLIC|MEMF_CLEAR);if(!draft||!stored)goto done;
 memset(original,0,sizeof(original));memset(forgotten,0,sizeof(forgotten));ah_servers_load(draft,AH_SERVERS_FILE,AH_SERVERS_TOKENS);
 for(i=0;i<draft->count;++i){strcpy(original[i],draft->items[i].name);if(!strcmp(draft->items[i].url,prefs->connection.url)&&!strcmp(draft->items[i].token,prefs->token)){selected=i;match=1;draft->items[i].reconnect=prefs->reconnect;draft->items[i].retry_interval=prefs->retry_interval;}}
 if(!match&&draft->count<AH_SERVERS)server_draft_new(draft,&selected,prefs,1);
 if(!draft->count)goto done;forgotten[selected]=forget;
 for(;;){
  item=&draft->items[selected];
  if(!panel){for(i=0;i<draft->count;++i)names[i]=draft->items[i].name;
   fields[0].label="Server URL";fields[0].value=item->url;fields[0].limit=sizeof(item->url);
   fields[1].label="Token";fields[1].value=item->token;fields[1].limit=sizeof(item->token);
   fields[2].label="Transport";fields[2].value="From the URL scheme";fields[2].limit=0;
   fields[3].label="Connection";fields[3].value="Test or Save the selected connection";fields[3].limit=0;
   fields[4].label="Name";fields[4].value=item->name;fields[4].limit=sizeof(item->name);
   panel=nd_panel_server_editor(fields,names,draft->count,selected,item->reconnect,item->retry_interval);if(!panel)break;
  }
  nd_panel_set(panel,2,!strncmp(nd_panel_get(panel,0),"https://",8) ? "HTTPS (encrypted)":!strncmp(nd_panel_get(panel,0),"http://",7) ? "HTTP (unencrypted)":"Use http:// or https://");
  action=nd_panel_event(panel);if(action==-1||action==104)break;
  if(action==118||action==105){server_draft_read(item,panel);forgotten[selected]=forget;
   if(action==105){if(draft->count>=AH_SERVERS){nd_panel_beep(panel);continue;}server_draft_new(draft,&selected,prefs,0);}
   else selected=nd_panel_server_selected(panel);
   nd_panel_close(panel);panel=NULL;forget=forgotten[selected];delete_pending=-1;continue;
  }
  if(action==117){if(delete_pending!=(int)selected){delete_pending=(int)selected;nd_panel_set(panel,3,"Press Delete again to remove connection");continue;}
   if(original[selected][0]){BPTR lock;ah_servers_load(stored,AH_SERVERS_FILE,AH_SERVERS_TOKENS);ah_servers_remove(stored,original[selected]);lock=CreateDir("ENVARC:AmiHASS");if(lock)UnLock(lock);if(!ah_servers_save(stored,AH_SERVERS_FILE,AH_SERVERS_TOKENS)){nd_panel_title(panel,"Could not delete saved connection");nd_panel_beep(panel);continue;}}
   amihass_erase(item,sizeof(*item));memmove(item,item+1,(draft->count-selected-1)*sizeof(*item));memmove(original[selected],original[selected+1],(draft->count-selected-1)*sizeof(original[0]));memmove(&forgotten[selected],&forgotten[selected+1],(draft->count-selected-1)*sizeof(forgotten[0]));--draft->count;memset(&draft->items[draft->count],0,sizeof(*item));memset(original[draft->count],0,sizeof(original[0]));forgotten[draft->count]=0;
   if(!draft->count)server_draft_new(draft,&selected,prefs,0);else if(selected>=draft->count)selected=draft->count-1;
   nd_panel_close(panel);panel=NULL;forget=forgotten[selected];delete_pending=-1;continue;
  }
  if(action)delete_pending=-1;
  if(action==100){if(token_file(token)){forget=0;nd_panel_set_input(panel,1,token);nd_panel_set(panel,3,"Not Connected");}else nd_panel_title(panel,"Could not import token text file");}
  else if(action==101){amihass_erase(token,sizeof(token));forget=1;nd_panel_set_input(panel,1,"");nd_panel_set(panel,3,"Not Configured");}
  else if(action==5){server_draft_read(item,panel);for(i=0;i<draft->count;++i)names[i]=draft->items[i].name;nd_panel_servers(panel,names,draft->count,selected);}
  else if(action==102||action==103){
   server_draft_read(item,panel);candidate=prefs->connection;strcpy(candidate.url,item->url);amihass_url_clean(candidate.url);if(strcmp(candidate.url,item->url)){strcpy(item->url,candidate.url);nd_panel_set_input(panel,0,candidate.url);}candidate.allow_http=!strncmp(candidate.url,"http://",7);
   if(item->token[0])forget=0;amihass_erase(token,sizeof(token));
   if(!(action==103&&forget)){
    if(action==102&&!amihass_network_running()){ah_log_event(AH_LOG_TEST_NO_NETWORK,0);nd_panel_set(panel,3,"Failed - TCP/IP network not running");nd_panel_beep(panel);continue;}
    if(!amihass_url(candidate.url,candidate.allow_http)){ah_log_event(AH_LOG_TEST_CONFIG,0);nd_panel_set(panel,3,"Failed - server URL is invalid");nd_panel_beep(panel);continue;}
    if(!amihass_token(item->token,token)){ah_log_event(AH_LOG_TEST_CONFIG,0);nd_panel_set(panel,3,"Failed - token is invalid or too short");nd_panel_beep(panel);continue;}
    if(action==102){nd_panel_set(panel,3,"Testing connection...");test=amihass_connection_test_dialog(&candidate,token,panel,104);if(test==AH_TEST_CANCELLED)break;nd_panel_title(panel,test==AH_TEST_OK ? "AmiHASS - Server Settings":ah_connection_text(test));nd_panel_set(panel,3,ah_connection_text(test));if(test!=AH_TEST_OK)nd_panel_beep(panel);continue;}
   }
   ah_servers_load(stored,AH_SERVERS_FILE,AH_SERVERS_TOKENS);
   if(forget){if(original[selected][0])ah_servers_remove(stored,original[selected]);}
   else{
    int collision=ah_servers_find(stored,item->name);if(collision>=0&&strcmp(item->name,original[selected])){nd_panel_title(panel,"Connection name already exists");nd_panel_beep(panel);continue;}
    if(!ah_servers_set(stored,item->name,candidate.url,token)||!ah_servers_options(stored,item->name,item->reconnect,item->retry_interval)){nd_panel_title(panel,"Enter a connection name, URL and token");nd_panel_beep(panel);continue;}
    if(original[selected][0]&&strcmp(original[selected],item->name))ah_servers_remove(stored,original[selected]);
   }
   {BPTR lock=CreateDir("ENVARC:AmiHASS");if(lock)UnLock(lock);}
   if(!ah_servers_save(stored,AH_SERVERS_FILE,AH_SERVERS_TOKENS)){nd_panel_title(panel,"Could not save connection; check ENVARC:");nd_panel_beep(panel);continue;}
   if(amihass_server_save_options(prefs,&candidate,token,forget,item->reconnect,item->retry_interval)){result=1;break;}
   ah_log_event(AH_LOG_SAVE_FAILED,0);nd_panel_title(panel,"Connection saved; could not activate it");nd_panel_beep(panel);
  }
  nd_panel_wait(panel,0,5);
 }
 done:nd_panel_close(panel);if(draft){ah_servers_erase(draft);FreeVec(draft);}if(stored){ah_servers_erase(stored);FreeVec(stored);}amihass_erase(token,sizeof(token));return result;
}
/* Local availability checks only: never contact a server or read credentials. */
static int dependency_path(const char *path)
{BPTR lock=Lock((STRPTR)path,ACCESS_READ);if(!lock)return 0;UnLock(lock);return 1;}
static int dependencies_panel(int selftest)
{struct NDPanelField fields[6];struct NDPanel *p;struct Library *ssl;static const char *buttons[]={"Check again","Close"};char ca[520],clock[80];struct DateStamp stamp;int event,found;
 for(;;){fields[0].label="curl";fields[0].value=dependency_path("C:curl") ? "C:curl found; check HTTPS with C:curl --version":"Missing C:curl - install a Classic m68k curl build";fields[0].limit=0;
 ssl=OpenLibrary("amisslmaster.library",0);fields[1].label="AmiSSL";fields[1].value=ssl ? "Master library opens; curl must support this version":"Master library unavailable; required by AmiSSL curl builds";fields[1].limit=0;if(ssl)CloseLibrary(ssl);
 ca[0]=0;found=GetVar("CURL_CA_BUNDLE",ca,sizeof(ca),0)>0;if(!found){ca[0]=0;found=GetVar("SSL_CERT_FILE",ca,sizeof(ca),0)>0;}found=found&&dependency_path(ca);fields[2].label="Certificates";fields[2].value=found ? "Configured CA path found; trust verified by Test connection":dependency_path("AmiSSL:Certs") ? "AmiSSL:Certs found; curl CA setup still needs verification":"No CA path found; install certificates for your curl build";fields[2].limit=0;
 DateStamp(&stamp);snprintf(clock,sizeof(clock),"%s",stamp.ds_Days<16801 ? "Clock appears old - correct the Amiga date/time":"Check Amiga date/time against your current local clock");fields[3].label="Clock";fields[3].value=clock;fields[3].limit=0;
 fields[4].label="Help";fields[4].value="See REQUIREMENTS.txt for downloads and installation.";fields[4].limit=0;fields[5].label="Validation";fields[5].value="Availability only. Test connection verifies HTTPS and trust.";fields[5].limit=0;
 p=nd_panel_open("AmiHASS - dependencies",fields,6,buttons,2);if(!p)return 0;if(selftest){nd_panel_close(p);return 1;}for(;;){event=nd_panel_event(p);if(event==-1||event==100||event==101)break;nd_panel_wait(p,0,50);}nd_panel_close(p);if(event!=100)return 1;}
}
static int health_panel(int selftest)
{struct NDPanelField f[10];struct NDPanel *p;struct AmiHASSConfig config;struct Library *ssl;char service[180],settings[160],ca[520];int i,live,loaded,found,event;static const char *buttons[]={"Check again","Close"};
 for(;;){Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();loaded=amihass_config_load(&config);
 snprintf(service,sizeof(service),"%s | %.150s",live ? "Running":"Stopped",config.service_dir[0] ? config.service_dir:"Service directory not configured");
 snprintf(settings,sizeof(settings),"%s%s",!loaded ? "Unreadable / invalid":dependency_path(AMIHASS_CONFIG_FILE) ? "Present":"Defaults (file absent)",config.recovered ? " - recovered backup; save to repair":"");
 f[0].label="Service";f[0].value=service;f[1].label="Config path";f[1].value=AMIHASS_CONFIG_FILE;f[2].label="Config";f[2].value=settings;
 f[3].label="curl";f[3].value=dependency_path("C:curl") ? "C:curl found":"Missing C:curl; see REQUIREMENTS.txt";
 ssl=OpenLibrary("amisslmaster.library",0);f[4].label="AmiSSL";f[4].value=ssl ? "Master library opens":"Master library unavailable";if(ssl)CloseLibrary(ssl);
 ca[0]=0;found=GetVar("CURL_CA_BUNDLE",ca,sizeof(ca),0)>0;if(!found)found=GetVar("SSL_CERT_FILE",ca,sizeof(ca),0)>0;
 f[5].label="Certificates";f[5].value=found&&dependency_path(ca) ? "Configured CA path found":dependency_path("AmiSSL:Certs") ? "Certificates found; use Test connection":"No CA path found; install curl certificates";
 f[6].label="Network";f[6].value=amihass_network_running() ? "TCP/IP interface has an assigned address":"No ready TCP/IP interface";
 f[7].label="Server";f[7].value=config.sample_mode ? "Sample data selected":config.token[0]&&config.connection.url[0] ? "Configured; use Test connection":"Not configured; open Server Settings";
 f[8].label="Executable";f[8].value="Service executable not found";{char path[540];strcpy(path,config.service_dir);if(path[0]&&AddPart(path,"AmiHASSService",sizeof(path))&&dependency_path(path))f[8].value="Service executable found";}
 f[9].label="Checks";f[9].value="Local checks; server access is not tested.";
 for(i=0;i<10;++i)f[i].limit=0;amihass_erase(&config,sizeof(config));p=nd_panel_open("AmiHASS - installation health",f,10,buttons,2);if(!p)return 0;if(selftest){nd_panel_close(p);return 1;}
 for(;;){event=nd_panel_event(p);if(event==-1||event==100||event==101)break;nd_panel_wait(p,0,50);}nd_panel_close(p);if(event!=100)return 1;}
}
void amihass_health_ui(void){health_panel(0);}
static void dependencies_ui(void){dependencies_panel(0);}
int amihass_dependencies_check(void){int ok=dependencies_panel(1)&&health_panel(1);if(ok)puts("PASS: native local dependency panel; no network request.");return ok;}
void amihass_recovery_notice(const char *what)
{struct NDPanelField fields[2];struct NDPanel *p;static const char *buttons[]={"Continue"};int event;fields[0].label="Recovered";fields[0].value=what;fields[0].limit=0;fields[1].label="Next";fields[1].value="Loaded a complete backup. Save to keep it; Close leaves it unsaved.";fields[1].limit=0;p=nd_panel_open("AmiHASS - recovered configuration",fields,2,buttons,1);if(!p)return;for(;;){event=nd_panel_event(p);if(event==-1||event==100)break;nd_panel_wait(p,0,50);}nd_panel_close(p);}
int amihass_general_edit(struct AmiHASSPreferences *prefs)
{
 struct NDPanel *panel;struct NDPanelField fields[10];char poll[12],budget[12],*end;unsigned long interval,limit;int action,result=0,draft_low=prefs->low_memory,draft_push=prefs->push_updates,draft_remote=prefs->remote_security;
 static const char *buttons[]={"Apply","Close","Dependencies"};
 sprintf(poll,"%u",prefs->connection.poll);sprintf(budget,"%u",prefs->background_budget);
reopen_general:
 fields[0].label="Poll seconds";fields[0].value=poll;fields[0].limit=sizeof(poll);
 fields[1].label="Range";fields[1].value="10-3600 seconds; budget 0 = unlimited";fields[1].limit=0;
 fields[2].label="Low memory mode";fields[2].value=draft_low ? "1":"0";fields[2].limit=ND_PANEL_CHECKBOX;
 fields[3].label="Memory";fields[3].value="Reclaim unused libraries after 30 s idle.";fields[3].limit=0;
 fields[4].label="Polling preset";fields[4].value="";fields[4].limit=ND_PANEL_CHOICE;fields[5].label="Background/min";fields[5].value=budget;fields[5].limit=sizeof(budget);
 fields[6].label="Live updates";fields[6].value=draft_push ? "1":"0";fields[6].limit=ND_PANEL_CHECKBOX;fields[7].label="Transport";fields[7].value="Selected entities only; Low memory uses REST.";fields[7].limit=0;
 fields[8].label="Script doors";fields[8].value=draft_remote ? "1":"0";fields[8].limit=ND_PANEL_CHECKBOX;fields[9].label="Security";fields[9].value="Off: only Dashboard opens locks and covers.";fields[9].limit=0;
 panel=nd_panel_open("AmiHASS - General Settings",fields,10,buttons,3);
 if(panel){static const char *presets[]={"Economy (60s)","Balanced (30s)","Responsive (10s)","Custom",NULL};unsigned int preset=atoi(poll)==60 ? 0:atoi(poll)==30 ? 1:atoi(poll)==10 ? 2:3;nd_panel_choices(panel,4,presets,preset);for(;;){action=nd_panel_event(panel);if(action==-1 || action==101)break;
  if(action==5){unsigned int selected=nd_panel_choice(panel,4);if(selected<3){strcpy(poll,selected==0 ? "60":selected==1 ? "30":"10");nd_panel_set_input(panel,0,poll);}}
  if(action==102){strncpy(budget,nd_panel_get(panel,5),sizeof(budget)-1);budget[sizeof(budget)-1]=0;strncpy(poll,nd_panel_get(panel,0),sizeof(poll)-1);poll[sizeof(poll)-1]=0;draft_push=nd_panel_toggle(panel,6);draft_low=nd_panel_toggle(panel,2);draft_remote=nd_panel_toggle(panel,8);nd_panel_close(panel);dependencies_ui();goto reopen_general;}
  if(action==100){const char *number=nd_panel_get(panel,0);interval=strtoul(number,&end,10);
   if(!*number || *end || interval<10 || interval>3600){nd_panel_title(panel,"Use a poll interval from 10 to 3600 seconds");nd_panel_beep(panel);continue;}
   {const char *number=nd_panel_get(panel,5);limit=strtoul(number,&end,10);if(!*number||*end||limit>120){nd_panel_title(panel,"Background budget: 0 unlimited, or 1 to 120 per minute");continue;}}
   if(prefs->background_budget!=(unsigned int)limit){prefs->background_budget=(unsigned int)limit;prefs->dirty=1;}
   if(prefs->connection.poll!=(unsigned int)interval){prefs->connection.poll=(unsigned int)interval;prefs->dirty=1;}
   if(prefs->low_memory!=nd_panel_toggle(panel,2)){prefs->low_memory=nd_panel_toggle(panel,2);prefs->dirty=1;}
   if(prefs->push_updates!=nd_panel_toggle(panel,6)){prefs->push_updates=nd_panel_toggle(panel,6);prefs->dirty=1;}
   if(prefs->remote_security!=nd_panel_toggle(panel,8)){prefs->remote_security=nd_panel_toggle(panel,8);prefs->dirty=1;}
   result=1;break;
  }nd_panel_wait(panel,0,5);}
  nd_panel_close(panel);
 }
 return result;
}
void amihass_connection_ui(void)
{amihass_preferences_ui(1);}
struct Chooser {struct Screen *screen;struct Window *window;struct Gadget *gadgets,*list,*filter,*status;APTR visual;struct List labels;struct Node nodes[AMIHASS_PAGE];char lines[AMIHASS_PAGE][160],status_text[160];struct AmiHASSEntity items[AMIHASS_PAGE];int count,total,offset,selected;};
static void empty_labels(struct Chooser *p)
{p->labels.lh_Head=(struct Node *)&p->labels.lh_Tail;p->labels.lh_Tail=NULL;p->labels.lh_TailPred=(struct Node *)&p->labels.lh_Head;}
static void show(struct Chooser *p,const struct AmiHASSMessage *m)
{
 int i;char *status=p->status_text;GT_SetGadgetAttrs(p->list,p->window,NULL,GTLV_Labels,~0UL,TAG_DONE);empty_labels(p);p->count=m->count;p->total=m->total;p->selected=-1;
 for(i=0;i<p->count;++i){p->items[i]=m->items[i];sprintf(p->lines[i],ah_txf("%.40s  [%.80s]"),m->items[i].name,m->items[i].id);p->nodes[i].ln_Name=p->lines[i];AddTail(&p->labels,&p->nodes[i]);}
 GT_SetGadgetAttrs(p->list,p->window,NULL,GTLV_Labels,(ULONG)&p->labels,GTLV_Selected,~0UL,TAG_DONE);
 if(m->result==AMIHASS_OK)sprintf(status,ah_txf("%ld entities; page %d. Select a row, then Choose."),m->total,p->offset/AMIHASS_PAGE+1);else strcpy(status,amihass_problem(m->result));
 GT_SetGadgetAttrs(p->status,p->window,NULL,GTTX_Text,(ULONG)ah_tx((const char *)(status)),TAG_DONE);
}
int amihass_choose(const char *domain,char entity[96])
{
 struct Chooser *p;struct AmiHASSClient c;struct AmiHASSMessage *m;struct NewGadget ng;struct Gadget *last;struct IntuiMessage *im;struct TextAttr font={(STRPTR)"topaz.font",8,0,0};
 char filter[64]="";int i,w,h,top,result=0,done=0,request=1,refresh=0,poll=0;ULONG cls;UWORD id,code;static const char *buttons[]={"Previous","Next","Select","Server Settings","Close"};
 memset(&c,0,sizeof(c));p=AllocVec(sizeof(*p),MEMF_PUBLIC|MEMF_CLEAR);if(!p)return 0;
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",39);GadToolsBase=OpenLibrary("gadtools.library",39);if(!IntuitionBase || !GadToolsBase)goto finished;
 p->screen=LockPubScreen("Workbench");if(!p->screen)goto finished;w=p->screen->Width-20;if(w>700)w=700;if(w<400)goto finished;top=p->screen->WBorTop+p->screen->Font->ta_YSize+8;h=p->screen->Height-p->screen->BarHeight-12;if(h>350)h=350;if(h<180)goto finished;
 p->visual=GetVisualInfo(p->screen,TAG_DONE);if(!p->visual)goto finished;last=CreateContext(&p->gadgets);if(!last)goto finished;empty_labels(p);
 memset(&ng,0,sizeof(ng));ng.ng_VisualInfo=p->visual;ng.ng_TextAttr=&font;ng.ng_LeftEdge=64;ng.ng_TopEdge=top;ng.ng_Width=w-80;ng.ng_Height=16;ng.ng_GadgetID=1;ng.ng_GadgetText=(STRPTR)ah_tx((const char *)("Filter"));ng.ng_Flags=PLACETEXT_LEFT;
 p->filter=last=CreateGadget(STRING_KIND,last,&ng,GTST_String,(ULONG)filter,GTST_MaxChars,63,TAG_DONE);if(!last)goto finished;
 ng.ng_LeftEdge=12;ng.ng_TopEdge=top+24;ng.ng_Width=w-24;ng.ng_Height=h-top-100;ng.ng_GadgetID=2;ng.ng_GadgetText=(STRPTR)ah_tx((const char *)(NULL));
 p->list=last=CreateGadget(LISTVIEW_KIND,last,&ng,GTLV_Labels,(ULONG)&p->labels,GTLV_ShowSelected,0,TAG_DONE);if(!last)goto finished;
 ng.ng_TopEdge=h-68;ng.ng_Height=16;ng.ng_GadgetID=3;p->status=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)ah_tx((const char *)("Loading entities...")),TAG_DONE);if(!last)goto finished;
 for(i=0;i<5;++i){ng.ng_LeftEdge=12+i*(w-24)/5;ng.ng_TopEdge=h-42;ng.ng_Width=(w-24)/5-5;ng.ng_Height=20;ng.ng_GadgetID=100+i;ng.ng_GadgetText=(STRPTR)ah_tx((const char *)((STRPTR)buttons[i]));ng.ng_Flags=PLACETEXT_IN;last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto finished;}
 p->window=OpenWindowTags(NULL,WA_Title,(ULONG)ah_tx((const char *)("AmiHASS - choose entity / scene / group")),WA_PubScreen,(ULONG)p->screen,WA_Left,(p->screen->Width-w)/2,WA_Top,p->screen->BarHeight+4,WA_Width,w,WA_Height,h,WA_Flags,WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE|WFLG_SMART_REFRESH,WA_IDCMP,LISTVIEWIDCMP|STRINGIDCMP|BUTTONIDCMP|IDCMP_CLOSEWINDOW|IDCMP_RAWKEY,WA_Gadgets,(ULONG)p->gadgets,TAG_DONE);if(!p->window)goto finished;GT_RefreshWindow(p->window,NULL);
 while(!done){
  if(request && !c.pending){if(amihass_send(&c,AMIHASS_LIST,NULL,NULL,NULL,p->offset,refresh,domain,filter)){request=0;refresh=0;}else{GT_SetGadgetAttrs(p->status,p->window,NULL,GTTX_Text,(ULONG)ah_tx((const char *)("Start the AmiHASS service before browsing")),TAG_DONE);}}
  m=amihass_reply(&c);if(m){show(p,m);if(m->result==AMIHASS_WAIT)poll=10;FreeVec(m);}if(poll>0 && --poll==0)request=1;
  while((im=GT_GetIMsg(p->window->UserPort))!=NULL){cls=im->Class;code=im->Code;id=cls==IDCMP_GADGETUP && im->IAddress ? ((struct Gadget *)im->IAddress)->GadgetID:0;GT_ReplyIMsg(im);
   if(cls==IDCMP_CLOSEWINDOW || (cls==IDCMP_RAWKEY && code==0x45) || id==104){done=1;break;}
   if(cls==IDCMP_REFRESHWINDOW){GT_BeginRefresh(p->window);GT_EndRefresh(p->window,TRUE);}
   if(id==2){p->selected=code<p->count ? code:-1;}
   if(id==1){strncpy(filter,((struct StringInfo *)p->filter->SpecialInfo)->Buffer,sizeof(filter)-1);p->offset=0;request=1;}
   if(id==100 && p->offset>=AMIHASS_PAGE){p->offset-=AMIHASS_PAGE;request=1;}
   if(id==101 && p->offset+p->count<p->total){p->offset+=AMIHASS_PAGE;request=1;}
   if(id==102 && p->selected>=0 && p->selected<p->count){strcpy(entity,p->items[p->selected].id);result=1;done=1;}
   if(id==103){amihass_connection_ui();p->offset=0;refresh=1;request=1;}
  }ah_port_wait(p->window->UserPort,amihass_client_signal(&c),5);
 }
finished:amihass_client_close(&c);if(p->window)CloseWindow(p->window);if(p->gadgets)FreeGadgets(p->gadgets);if(p->visual)FreeVisualInfo(p->visual);if(p->screen)UnlockPubScreen(NULL,p->screen);if(GadToolsBase)CloseLibrary(GadToolsBase);GadToolsBase=NULL;if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);IntuitionBase=NULL;FreeVec(p);return result;
}
