/* Native shared connection setup and an asynchronous paged entity chooser. */
#define IntuitionBase AmiHASSIntuitionBase
#define GadToolsBase AmiHASSGadToolsBase
#define AslBase AmiHASSAslBase
#include <exec/types.h>
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
#include "service.h"
#include "nd_panel.h"
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
int amihass_connection_edit(struct AmiHASSPreferences *prefs)
{
 struct NDPanel *panel;struct NDPanelField fields[4];struct AmiHASSConnection candidate;
 char token[1025];int action,result=0,forget=prefs->forget,test;
 strcpy(token,prefs->token);
 fields[0].label="Server URL";fields[0].value=prefs->connection.url;fields[0].limit=sizeof(prefs->connection.url);
 fields[1].label="Token";fields[1].value=token;fields[1].limit=sizeof(token);
 fields[2].label="Transport";fields[2].value="From the URL scheme";fields[2].limit=0;
 fields[3].label="Connection";fields[3].value="Not Connected";fields[3].limit=0;
 panel=nd_panel_server_settings(fields,prefs->reconnect,prefs->retry_interval);
 if(panel){for(;;){
  const char *url=nd_panel_get(panel,0);
  nd_panel_set(panel,2,!strncmp(url,"https://",8) ? "HTTPS (encrypted)":!strncmp(url,"http://",7) ? "HTTP (unencrypted)":"Use http:// or https://");
  action=nd_panel_event(panel);if(action==-1 || action==104)break;
  if(action==100){if(token_file(token)){forget=0;nd_panel_set_input(panel,1,token);nd_panel_set(panel,3,"Not Connected");}else nd_panel_title(panel,"Could not import token text file");}
  else if(action==101){amihass_erase(token,sizeof(token));forget=1;nd_panel_set_input(panel,1,"");nd_panel_set(panel,3,"Not Configured");}
  else if(action==102 || action==103){
   url=nd_panel_get(panel,0);candidate=prefs->connection;candidate.allow_http=!strncmp(url,"http://",7);strcpy(candidate.url,url);
   if(nd_panel_get(panel,1)[0])forget=0;
   amihass_erase(token,sizeof(token));
   if(!(action==103 && forget)){
    if(action==102 && !amihass_network_running()){ah_log_event(AH_LOG_TEST_NO_NETWORK,0);nd_panel_set(panel,3,"Failed - TCP/IP network not running");nd_panel_beep(panel);continue;}
    if(!amihass_url(url,candidate.allow_http) || !amihass_token(nd_panel_get(panel,1),token)){ah_log_event(AH_LOG_TEST_CONFIG,0);nd_panel_set(panel,3,"Failed - configure server and token");nd_panel_beep(panel);continue;}
    if(action==102){
    nd_panel_set(panel,3,"Testing connection...");
    test=amihass_connection_test_dialog(&candidate,token,panel,104);
    if(test==AH_TEST_CANCELLED)break;
    nd_panel_title(panel,"AmiHASS - Server Settings");
    nd_panel_set(panel,3,ah_connection_text(test));
    if(test!=AH_TEST_OK){nd_panel_beep(panel);continue;}
    continue;
    }
   }
   if(amihass_server_save_options(prefs,&candidate,token,forget,nd_panel_reconnect(panel),nd_panel_retry(panel))){result=1;break;}
   ah_log_event(AH_LOG_SAVE_FAILED,0);
   nd_panel_title(panel,"Could not save server settings; check ENVARC:");nd_panel_beep(panel);
  }Delay(5);}
  nd_panel_close(panel);
 }
 amihass_erase(token,sizeof(token));return result;
}
int amihass_general_edit(struct AmiHASSPreferences *prefs)
{
 struct NDPanel *panel;struct NDPanelField fields[4];char poll[12],*end;unsigned long interval;int action,result=0;
 static const char *buttons[]={"Apply","Close"};
 sprintf(poll,"%u",prefs->connection.poll);
 fields[0].label="Poll seconds";fields[0].value=poll;fields[0].limit=sizeof(poll);
 fields[1].label="Range";fields[1].value="10 to 3600 seconds";fields[1].limit=0;
 fields[2].label="Low memory mode";fields[2].value=prefs->low_memory ? "1":"0";fields[2].limit=ND_PANEL_CHECKBOX;
 fields[3].label="Memory";fields[3].value="Reclaim unused libraries after 30 s idle.";fields[3].limit=0;
 panel=nd_panel_open("AmiHASS - General Settings",fields,4,buttons,2);
 if(panel){for(;;){action=nd_panel_event(panel);if(action==-1 || action==101)break;
  if(action==100){const char *number=nd_panel_get(panel,0);interval=strtoul(number,&end,10);
   if(!*number || *end || interval<10 || interval>3600){nd_panel_title(panel,"Use a poll interval from 10 to 3600 seconds");nd_panel_beep(panel);continue;}
   if(prefs->connection.poll!=(unsigned int)interval){prefs->connection.poll=(unsigned int)interval;prefs->dirty=1;}
   if(prefs->low_memory!=nd_panel_toggle(panel,2)){prefs->low_memory=nd_panel_toggle(panel,2);prefs->dirty=1;}
   result=1;break;
  }Delay(5);}
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
 for(i=0;i<p->count;++i){p->items[i]=m->items[i];sprintf(p->lines[i],"%.40s  [%.80s]",m->items[i].name,m->items[i].id);p->nodes[i].ln_Name=p->lines[i];AddTail(&p->labels,&p->nodes[i]);}
 GT_SetGadgetAttrs(p->list,p->window,NULL,GTLV_Labels,(ULONG)&p->labels,GTLV_Selected,~0UL,TAG_DONE);
 if(m->result==AMIHASS_OK)sprintf(status,"%ld entities; page %d. Select a row, then Choose.",m->total,p->offset/AMIHASS_PAGE+1);else strcpy(status,amihass_problem(m->result));
 GT_SetGadgetAttrs(p->status,p->window,NULL,GTTX_Text,(ULONG)status,TAG_DONE);
}
int amihass_choose(const char *domain,char entity[96])
{
 struct Chooser *p;struct AmiHASSClient c;struct AmiHASSMessage *m;struct NewGadget ng;struct Gadget *last;struct IntuiMessage *im;struct TextAttr font={(STRPTR)"topaz.font",8,0,0};
 char filter[64]="";int i,w,h,top,result=0,done=0,request=1,refresh=0,poll=0;ULONG cls;UWORD id,code;static const char *buttons[]={"Previous","Next","Select","Server Settings","Close"};
 memset(&c,0,sizeof(c));p=AllocVec(sizeof(*p),MEMF_PUBLIC|MEMF_CLEAR);if(!p)return 0;
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",39);GadToolsBase=OpenLibrary("gadtools.library",39);if(!IntuitionBase || !GadToolsBase)goto finished;
 p->screen=LockPubScreen("Workbench");if(!p->screen)goto finished;w=p->screen->Width-20;if(w>700)w=700;if(w<400)goto finished;top=p->screen->WBorTop+p->screen->Font->ta_YSize+8;h=p->screen->Height-p->screen->BarHeight-12;if(h>350)h=350;if(h<180)goto finished;
 p->visual=GetVisualInfo(p->screen,TAG_DONE);if(!p->visual)goto finished;last=CreateContext(&p->gadgets);if(!last)goto finished;empty_labels(p);
 memset(&ng,0,sizeof(ng));ng.ng_VisualInfo=p->visual;ng.ng_TextAttr=&font;ng.ng_LeftEdge=64;ng.ng_TopEdge=top;ng.ng_Width=w-80;ng.ng_Height=16;ng.ng_GadgetID=1;ng.ng_GadgetText="Filter";ng.ng_Flags=PLACETEXT_LEFT;
 p->filter=last=CreateGadget(STRING_KIND,last,&ng,GTST_String,(ULONG)filter,GTST_MaxChars,63,TAG_DONE);if(!last)goto finished;
 ng.ng_LeftEdge=12;ng.ng_TopEdge=top+24;ng.ng_Width=w-24;ng.ng_Height=h-top-100;ng.ng_GadgetID=2;ng.ng_GadgetText=NULL;
 p->list=last=CreateGadget(LISTVIEW_KIND,last,&ng,GTLV_Labels,(ULONG)&p->labels,GTLV_ShowSelected,0,TAG_DONE);if(!last)goto finished;
 ng.ng_TopEdge=h-68;ng.ng_Height=16;ng.ng_GadgetID=3;p->status=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)"Loading entities...",TAG_DONE);if(!last)goto finished;
 for(i=0;i<5;++i){ng.ng_LeftEdge=12+i*(w-24)/5;ng.ng_TopEdge=h-42;ng.ng_Width=(w-24)/5-5;ng.ng_Height=20;ng.ng_GadgetID=100+i;ng.ng_GadgetText=(STRPTR)buttons[i];ng.ng_Flags=PLACETEXT_IN;last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto finished;}
 p->window=OpenWindowTags(NULL,WA_Title,(ULONG)"AmiHASS - choose entity / scene / group",WA_PubScreen,(ULONG)p->screen,WA_Left,(p->screen->Width-w)/2,WA_Top,p->screen->BarHeight+4,WA_Width,w,WA_Height,h,WA_Flags,WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE|WFLG_SMART_REFRESH,WA_IDCMP,LISTVIEWIDCMP|STRINGIDCMP|BUTTONIDCMP|IDCMP_CLOSEWINDOW|IDCMP_RAWKEY,WA_Gadgets,(ULONG)p->gadgets,TAG_DONE);if(!p->window)goto finished;GT_RefreshWindow(p->window,NULL);
 while(!done){
  if(request && !c.pending){if(amihass_send(&c,AMIHASS_LIST,NULL,NULL,NULL,p->offset,refresh,domain,filter)){request=0;refresh=0;}else{GT_SetGadgetAttrs(p->status,p->window,NULL,GTTX_Text,(ULONG)"Start the AmiHASS service before browsing",TAG_DONE);}}
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
  }Delay(5);
 }
finished:amihass_client_close(&c);if(p->window)CloseWindow(p->window);if(p->gadgets)FreeGadgets(p->gadgets);if(p->visual)FreeVisualInfo(p->visual);if(p->screen)UnlockPubScreen(NULL,p->screen);if(GadToolsBase)CloseLibrary(GadToolsBase);GadToolsBase=NULL;if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);IntuitionBase=NULL;FreeVec(p);return result;
}
