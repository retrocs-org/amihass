/* Live SDK client with an explicitly selected offline sample provider. */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <stdio.h>
#include <string.h>
#include "nd_version.h"
#include "nd_panel.h"
#include "browser_data.h"
#include "browser_live.h"
#include "control_summary.h"
#include <ctype.h>
const char version[]=ND_VER_STRING("AmiHASSBrowser");
long __stack=65536;
static unsigned long memory_min=~0UL;
static void sample_memory(void){unsigned long n=AvailMem(MEMF_FAST);if(n<memory_min)memory_min=n;}
struct IntuitionBase *IntuitionBase=NULL;
struct Library *GadToolsBase=NULL;
struct Browser {
 struct Screen *screen;struct Window *window;struct Gadget *gadgets,*list,*search,*type,*area,*status,*details,*source,*banner,*previous,*next,*back;APTR visual;
 struct List labels;struct Node nodes[AH_BROWSER_MAX];char rows[AH_BROWSER_MAX][100],summary[100];
 unsigned int indices[AH_BROWSER_MAX],count,selected;int sample,device_view,visible_rows,name_columns,provider_mode;char selected_device[96],device_name[64],device_search[64],device_type[24];
 struct AHBrowserLive live;char banner_text[96],room_id[96],room_name[64];const char *room_labels[4];
};
/* Keep stable IDs separate from display labels, including duplicate room names. */
static const char *sample_display_state(const struct AHBrowserEntity *entity)
{static struct AmiHASSEntity value;char json[700];snprintf(json,sizeof(json),"{\"entity_id\":\"%s\",\"state\":\"%s\",\"attributes\":%s}",entity->id,entity->state,entity->attributes);if(!amihass_state(json,&value))return entity->state;return amihass_display_state(&value);}
static void set_room(struct Browser *b,const char *id,const char *name)
{
 char copy[96],label[64];strncpy(copy,id,sizeof(copy)-1);copy[95]=0;strncpy(label,name,sizeof(label)-1);label[63]=0;
 strcpy(b->room_id,copy);strcpy(b->room_name,label);strcpy(b->live.area,copy);
 b->room_labels[0]="All rooms";b->room_labels[1]=copy[0] ? b->room_name:"Choose room...";
 b->room_labels[2]=copy[0] ? "Choose room...":NULL;b->room_labels[3]=NULL;
 if(b->window)GT_SetGadgetAttrs(b->area,b->window,NULL,GTCY_Labels,(ULONG)b->room_labels,GTCY_Active,copy[0] ? 1:0,TAG_DONE);
}
/* One 16-room page at a time; the modal remains responsive during transport. */
static int choose_room(struct Browser *b,int selftest)
{
 struct RoomPage {struct AmiHASSEntity items[AMIHASS_PAGE];const char *labels[AMIHASS_PAGE+2];} *page;
 struct AmiHASSClient client;struct AmiHASSMessage *m;struct NDPanelField fields[2];struct NDPanel *panel;
 static const char *buttons[]={"Previous","Next","Refresh","Select","Close"};
 int action,offset=0,count=0,total=0,loading=1,refresh=0,next=0,ticks=0,started=0,chosen=0,valid=0,i;
 unsigned int selected;char status[96];
 page=AllocVec(sizeof(*page),MEMF_PUBLIC|MEMF_CLEAR);if(!page)return 0;
 memset(&client,0,sizeof(client));strcpy(status,"Loading rooms from the bridge...");
 fields[0].label="Room";fields[0].value="";fields[0].limit=ND_PANEL_CHOICE;
 fields[1].label="Status";fields[1].value=status;fields[1].limit=0;
 panel=nd_panel_open("AmiHASS - Select room",fields,2,buttons,5);if(!panel){FreeVec(page);return 0;}
 ah_browser_live_close(&b->live);
 page->labels[0]="All rooms";nd_panel_choices(panel,0,page->labels,0);
 for(;;){
  ++ticks;
  if(loading && client.pending){
   m=amihass_reply(&client);
   if(m){
    if(m->kind!=AMIHASS_AREAS){strcpy(status,"Unexpected room reply - Refresh to retry");loading=0;}
    else if(m->result==AMIHASS_OK && m->offset==offset && m->total<=100000 && m->count<=AMIHASS_PAGE){
     count=m->count;total=m->total;page->labels[0]="All rooms";
     for(i=0;i<count;++i){page->items[i]=m->items[i];page->items[i].id[95]=page->items[i].name[63]=0;page->labels[i+1]=page->items[i].name;}
     page->labels[count+1]=NULL;selected=0;for(i=0;i<count;++i)if(!strcmp(b->room_id,page->items[i].id))selected=i+1;
     nd_panel_choices(panel,0,page->labels,selected);valid=1;loading=0;
     if(count)sprintf(status,"Rooms %d-%d of %d",offset+1,offset+count,total);else strcpy(status,"No rooms configured - use All rooms");
    }else if(m->result==AMIHASS_WAIT || m->result==AMIHASS_BUSY)next=ticks+5;
    else{loading=0;strncpy(status,m->result==AMIHASS_UNSUPPORTED ? "Update the bridge to browse rooms":amihass_problem(m->result),sizeof(status)-1);status[95]=0;}
    FreeVec(m);
   }else if(!client.pending){loading=0;strcpy(status,"Room request timed out - Refresh to retry");}
  }
  if(loading && ticks-started>600){amihass_client_close(&client);loading=0;strcpy(status,"Room request timed out - Refresh to retry");}
  if(loading && !client.pending && ticks>=next){if(!amihass_send(&client,AMIHASS_AREAS,"AmiHASS.Rooms",NULL,NULL,offset,refresh,NULL,NULL)){loading=0;strcpy(status,"Start the bridge in Preferences; All rooms is available");}refresh=0;}
  nd_panel_set(panel,1,status);nd_panel_enable(panel,100,valid&&!loading&&offset>0);nd_panel_enable(panel,101,valid&&!loading&&offset+count<total);nd_panel_enable(panel,102,!loading);
  selected=nd_panel_choice(panel,0);nd_panel_enable(panel,103,selected==0 || (valid&&!loading&&selected<=(unsigned int)count));
  if(selftest){if(!loading){action=-1;if(valid)for(i=0;i<count;++i)if(!strcmp(page->items[i].id,"study")){nd_panel_choices(panel,0,page->labels,i+1);action=103;break;}}else action=0;}
  else action=nd_panel_event(panel);
  if(action==-1 || action==104)break;
  if(action==103){selected=nd_panel_choice(panel,0);if(selected==0){set_room(b,"","");chosen=1;break;}if(valid&&!loading&&selected<=(unsigned int)count){set_room(b,page->items[selected-1].id,page->items[selected-1].name);chosen=1;break;}}
  if((action==100 && offset>0) || (action==101 && valid && offset+count<total) || action==102){
   if(action==100)offset-=AMIHASS_PAGE;else if(action==101)offset+=AMIHASS_PAGE;
   amihass_client_close(&client);loading=1;valid=0;count=0;refresh=action==102;started=ticks;next=ticks;strcpy(status,"Loading rooms from the bridge...");page->labels[1]=NULL;nd_panel_choices(panel,0,page->labels,0);
  }
  Delay(5);
 }
 amihass_client_close(&client);nd_panel_close(panel);FreeVec(page);set_room(b,b->room_id,b->room_name);return chosen;
}
static int show_details(const struct AHBrowserEntity *entity,int selftest)
{
 struct NDPanelField fields[12];struct NDPanel *panel;char domain[96],parts[7][51];unsigned int n,i,at,length;int event;
 static const char *buttons[]={"Close"};
 if(!entity || !ah_browser_domain(entity,domain,sizeof(domain)))return 0;
 fields[0].label="Entity ID";fields[0].value=entity->id;fields[0].limit=0;
 fields[1].label="Name";fields[1].value=entity->name;fields[1].limit=0;
 fields[2].label="Type";fields[2].value=domain;fields[2].limit=0;
 fields[3].label="State";fields[3].value=sample_display_state(entity);fields[3].limit=0;
 fields[4].label="Source";fields[4].value="Sample data (read-only)";fields[4].limit=0;
 length=(unsigned int)strlen(entity->attributes);if(length>350)return 0;
 n=5;for(at=0;at<length;at+=50){i=length-at;if(i>50)i=50;memcpy(parts[n-5],entity->attributes+at,i);parts[n-5][i]=0;fields[n].label=n==5 ? "Attributes":"";fields[n].value=parts[n-5];fields[n].limit=0;++n;}
 panel=nd_panel_open("AmiHASS Browser - Sample entity",fields,n,buttons,1);if(!panel)return 0;
 if(!selftest)for(;;){event=nd_panel_event(panel);if(event==-1 || event==100)break;Delay(5);}
 nd_panel_close(panel);return 1;
}
static void publish_rows(struct Browser *b)
{
 unsigned int i;
 if(!b->sample && b->provider_mode!=b->live.sample_mode){b->provider_mode=b->live.sample_mode;b->selected_device[0]=0;b->device_view=1;set_room(b,"","");GT_SetGadgetAttrs(b->source,b->window,NULL,GTCY_Active,0,TAG_DONE);GT_SetGadgetAttrs(b->search,b->window,NULL,GTST_String,(ULONG)"",TAG_DONE);GT_SetGadgetAttrs(b->type,b->window,NULL,GTST_String,(ULONG)"All",TAG_DONE);}
 GT_SetGadgetAttrs(b->list,b->window,NULL,GTLV_Labels,~0UL,TAG_DONE);
 b->labels.lh_Head=(struct Node *)&b->labels.lh_Tail;b->labels.lh_Tail=NULL;b->labels.lh_TailPred=(struct Node *)&b->labels.lh_Head;
 if(!b->sample)b->count=b->live.count;
 for(i=0;i<b->count;++i){
  if(b->sample){const struct AHBrowserEntity *e=ah_browser_entity(b->indices[i]);sprintf(b->rows[i],"%-*.*s %.20s",b->name_columns,b->name_columns,e->name,sample_display_state(e));}
  else sprintf(b->rows[i],"%-*.*s %.20s",b->name_columns,b->name_columns,b->live.items[i].name,amihass_display_state(&b->live.items[i]));
  b->nodes[i].ln_Name=b->rows[i];AddTail(&b->labels,&b->nodes[i]);
 }
 if(b->selected>=b->count)b->selected=0;
 if(b->sample){sprintf(b->summary,"%u sample entities - read-only",b->count);strcpy(b->banner_text,"Sample data - offline");}
 else{
  if(b->live.sample_mode)strcpy(b->banner_text,"Sample bridge - simulated data");
  else if(b->selected_device[0])sprintf(b->banner_text,"Device: %.24s",b->device_name);
  else strcpy(b->banner_text,b->live.connected ? (b->device_view ? "Devices - catalogue only":"All entities - catalogue only"):"Live bridge - not connected");
  if(b->live.error[0])strcpy(b->summary,b->live.error);
  else if(b->live.loading)strcpy(b->summary,"Loading entities from Home Assistant...");
  else if(b->count)sprintf(b->summary,"%d-%d of %d %s - page %d",b->live.offset+1,b->live.offset+b->live.count,b->live.total,b->device_view ? "devices":"entities",b->live.offset/b->live.page_size+1);
  else strcpy(b->summary,"No matching entities");
 }
 GT_SetGadgetAttrs(b->list,b->window,NULL,GTLV_Labels,(ULONG)&b->labels,GTLV_Top,0,GTLV_Selected,b->count ? b->selected:~0UL,TAG_DONE);
 GT_SetGadgetAttrs(b->details,b->window,NULL,GA_Disabled,!b->count || (!b->sample && b->live.loading),TAG_DONE);
 GT_SetGadgetAttrs(b->previous,b->window,NULL,GA_Disabled,b->sample || b->live.loading || b->live.offset<=0,TAG_DONE);
 GT_SetGadgetAttrs(b->next,b->window,NULL,GA_Disabled,b->sample || b->live.loading || !b->count || b->live.offset+b->live.count>=b->live.total,TAG_DONE);
 GT_SetGadgetAttrs(b->back,b->window,NULL,GA_Disabled,!b->selected_device[0],TAG_DONE);
 GT_SetGadgetAttrs(b->type,b->window,NULL,GA_Disabled,FALSE,TAG_DONE);
 GT_SetGadgetAttrs(b->status,b->window,NULL,GTTX_Text,(ULONG)b->summary,TAG_DONE);
 GT_SetGadgetAttrs(b->banner,b->window,NULL,GTTX_Text,(ULONG)b->banner_text,TAG_DONE);b->live.dirty=0;
}
static void filter_rows(struct Browser *b,const char *query)
{
 char domain[24];unsigned int i;
 const char *text=((struct StringInfo *)b->type->SpecialInfo)->Buffer;
 b->live.page_size=b->visible_rows;strcpy(b->live.area,b->room_id);
 for(i=0;text[i] && i<23;++i)domain[i]=(char)tolower((unsigned char)text[i]);domain[i]=0;if(!strcmp(domain,"all"))domain[0]=0;
 b->selected=0;
 if(b->sample){b->count=ah_browser_filter(domain,query,b->indices,AH_BROWSER_MAX);if(b->count>AH_BROWSER_MAX)b->count=AH_BROWSER_MAX;}
 else if(!(b->device_view ? ah_browser_live_devices_type(&b->live,domain,query,0,1):ah_browser_live_device_page(&b->live,b->selected_device,domain,query,0,1))){strcpy(b->live.error,"Use an entity type such as sensor, or All");b->live.terminal_error=1;b->live.dirty=1;}
 publish_rows(b);
}
static void return_devices(struct Browser *b)
{b->selected_device[0]=0;b->device_view=1;GT_SetGadgetAttrs(b->search,b->window,NULL,GTST_String,(ULONG)b->device_search,TAG_DONE);GT_SetGadgetAttrs(b->type,b->window,NULL,GTST_String,(ULONG)(b->device_type[0] ? b->device_type:"All"),TAG_DONE);filter_rows(b,b->device_search);}
static int show_live_details(struct Browser *b,int selftest)
{
 struct NDPanelField fields[8];struct NDPanel *panel;struct AHBrowserEntity e;char domain[96],source[96],capabilities[96];int event=0,ok=1,ticks=0;
 static const char *buttons[]={"Close"};
 ah_browser_live_details(&b->live,(int)b->selected);e.id=b->live.detail.id;e.name=b->live.detail.name;e.state=b->live.detail.state;e.attributes="";
 if(!ah_browser_domain(&e,domain,sizeof(domain)))return 0;
 fields[0].label="Entity ID";fields[0].value=e.id;fields[0].limit=0;
 fields[1].label="Name";fields[1].value=e.name;fields[1].limit=0;
 fields[2].label="Type";fields[2].value=domain;fields[2].limit=0;
 fields[3].label="State";fields[3].value=e.state;fields[3].limit=0;
 fields[4].label="Unit";fields[4].value=b->live.detail.unit;fields[4].limit=0;
 fields[5].label="Controls";fields[5].value="Checking supported controls...";fields[5].limit=0;
 fields[6].label="Updated";fields[6].value=b->live.detail.updated;fields[6].limit=0;
 fields[7].label="Source";fields[7].value="Refreshing live state...";fields[7].limit=0;
 panel=nd_panel_open("AmiHASS Browser - Live entity",fields,8,buttons,1);if(!panel){ah_browser_live_cancel_details(&b->live);return 0;}
 for(;;){
  sample_memory();ah_browser_live_step(&b->live);++ticks;
  if(!selftest && ticks%50==0 && !b->live.detail_loading && !b->live.client.pending)ah_browser_live_details(&b->live,(int)b->selected);
  nd_panel_set(panel,1,b->live.detail.name);nd_panel_set(panel,3,amihass_display_state(&b->live.detail));nd_panel_set(panel,4,b->live.detail.unit);ah_control_summary(b->live.detail.id,&b->live.controls,b->live.detail_result==AMIHASS_OK,capabilities);nd_panel_set(panel,5,capabilities);nd_panel_set(panel,6,b->live.detail.updated);
  if(b->live.error[0])strcpy(source,b->live.error);
  else if(b->live.detail_loading)strcpy(source,"Refreshing live state...");
  else if(b->live.sample_mode)sprintf(source,"Sample simulation; age %lu s",b->live.age);
  else sprintf(source,"Live %sreading; age %lu s",b->live.detail_result==AMIHASS_STALE ? "stale ":"",b->live.age);
  nd_panel_set(panel,7,source);
  if(selftest && (!b->live.detail_loading || ticks>600)){ok=b->live.detail_result==AMIHASS_OK || b->live.detail_result==AMIHASS_UNAVAILABLE || b->live.detail_result==AMIHASS_STALE;break;}
  event=nd_panel_event(panel);if(event==-1 || event==100)break;Delay(5);
 }
 nd_panel_close(panel);ah_browser_live_cancel_details(&b->live);publish_rows(b);return ok;
}
static struct Browser *open_browser(int sample)
{
 struct Browser *b;struct NewGadget ng;struct Gadget *last;struct TextAttr font={(STRPTR)"topaz.font",8,0,0};int width,height,top;unsigned int i;
 b=AllocVec(sizeof(*b),MEMF_PUBLIC|MEMF_CLEAR);if(!b)return NULL;
 b->sample=sample;b->device_view=!sample;ah_browser_live_init(&b->live);
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",39);GadToolsBase=OpenLibrary("gadtools.library",39);if(!IntuitionBase || !GadToolsBase)goto failed;
 b->screen=LockPubScreen("Workbench");if(!b->screen)goto failed;
 width=b->screen->Width-24;if(width>620)width=620;height=b->screen->Height-b->screen->BarHeight-20;if(height>360)height=360;if(width<400 || height<220)goto failed;
 top=b->screen->WBorTop+b->screen->Font->ta_YSize+8;
 b->visual=GetVisualInfo(b->screen,TAG_DONE);if(!b->visual)goto failed;
 last=CreateContext(&b->gadgets);if(!last)goto failed;
 memset(&ng,0,sizeof(ng));ng.ng_VisualInfo=b->visual;ng.ng_TextAttr=&font;
 ng.ng_LeftEdge=12;ng.ng_TopEdge=top;ng.ng_Width=width-268;ng.ng_Height=16;ng.ng_GadgetID=2;
 b->banner=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)b->banner_text,TAG_DONE);if(!last)goto failed;
 ng.ng_LeftEdge=width-248;ng.ng_Width=56;ng.ng_Height=20;ng.ng_GadgetID=106;ng.ng_GadgetText="Back";ng.ng_Flags=PLACETEXT_IN;
 b->back=last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto failed;
 {static STRPTR modes[]={"Devices","All entities",NULL};ng.ng_LeftEdge=width-184;ng.ng_Width=172;ng.ng_Height=20;ng.ng_GadgetID=6;ng.ng_GadgetText=NULL;ng.ng_Flags=0;
 b->source=last=CreateGadget(CYCLE_KIND,last,&ng,GTCY_Labels,(ULONG)modes,GTCY_Active,0,TAG_DONE);if(!last)goto failed;}
 ng.ng_LeftEdge=68;ng.ng_TopEdge=top+24;ng.ng_Width=width-360;ng.ng_Height=20;ng.ng_GadgetText="Search";ng.ng_Flags=PLACETEXT_LEFT;ng.ng_GadgetID=3;
 b->search=last=CreateGadget(STRING_KIND,last,&ng,GTST_MaxChars,63,GTST_String,(ULONG)"",TAG_DONE);if(!last)goto failed;
 ng.ng_LeftEdge=width-242;ng.ng_Width=148;ng.ng_GadgetText="Type";ng.ng_Flags=PLACETEXT_LEFT;ng.ng_GadgetID=4;
 b->type=last=CreateGadget(STRING_KIND,last,&ng,GTST_MaxChars,23,GTST_String,(ULONG)"All",TAG_DONE);if(!last)goto failed;
 ng.ng_LeftEdge=width-86;ng.ng_Width=74;ng.ng_GadgetText="Search";ng.ng_Flags=PLACETEXT_IN;ng.ng_GadgetID=100;
 last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto failed;
 ng.ng_LeftEdge=68;ng.ng_TopEdge=top+50;ng.ng_Width=width-80;ng.ng_Height=20;ng.ng_GadgetText="Room";ng.ng_Flags=PLACETEXT_LEFT;ng.ng_GadgetID=7;
 b->room_labels[0]="All rooms";b->room_labels[1]="Choose room...";
 b->area=last=CreateGadget(CYCLE_KIND,last,&ng,GTCY_Labels,(ULONG)b->room_labels,GTCY_Active,0,TAG_DONE);if(!last)goto failed;
 b->labels.lh_Head=(struct Node *)&b->labels.lh_Tail;b->labels.lh_TailPred=(struct Node *)&b->labels.lh_Head;
 b->name_columns=(width-48)/8-21;if(b->name_columns>45)b->name_columns=45;if(b->name_columns<20)b->name_columns=20;
 b->visible_rows=(height-top-142-4)/8;if(b->visible_rows<1)b->visible_rows=1;if(b->visible_rows>32)b->visible_rows=32;
 ng.ng_LeftEdge=12;ng.ng_TopEdge=top+78;ng.ng_Width=width-24;ng.ng_Height=height-top-142;ng.ng_GadgetText=NULL;ng.ng_Flags=0;ng.ng_GadgetID=1;
 b->list=last=CreateGadget(LISTVIEW_KIND,last,&ng,GTLV_Labels,(ULONG)&b->labels,GTLV_ShowSelected,NULL,TAG_DONE);if(!last)goto failed;
 ng.ng_TopEdge=height-56;ng.ng_Height=16;ng.ng_GadgetID=5;
 b->status=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)b->summary,TAG_DONE);if(!last)goto failed;
 {static const char *labels[]={"Previous","Next","Refresh","Open","Close"};static const int ids[]={103,104,105,101,102};
 for(i=0;i<5;++i){ng.ng_TopEdge=height-32;ng.ng_Height=22;ng.ng_Width=(width-24)/5-6;ng.ng_LeftEdge=12+(int)i*(width-24)/5;ng.ng_GadgetText=(STRPTR)labels[i];ng.ng_Flags=PLACETEXT_IN;ng.ng_GadgetID=ids[i];
 last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto failed;if(i==0)b->previous=last;if(i==1)b->next=last;if(i==3)b->details=last;}}
 b->window=OpenWindowTags(NULL,WA_Title,(ULONG)(sample ? "AmiHASS Browser 0.2 - Sample data":"AmiHASS Browser 0.2 - Devices"),WA_PubScreen,(ULONG)b->screen,WA_Left,(b->screen->Width-width)/2,WA_Top,b->screen->BarHeight+10,WA_Width,width,WA_Height,height,WA_Flags,WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE|WFLG_SMART_REFRESH,WA_IDCMP,LISTVIEWIDCMP|STRINGIDCMP|CYCLEIDCMP|BUTTONIDCMP|IDCMP_CLOSEWINDOW|IDCMP_RAWKEY,WA_Gadgets,(ULONG)b->gadgets,TAG_DONE);
 if(!b->window)goto failed;GT_RefreshWindow(b->window,NULL);filter_rows(b,"");return b;
failed:
 if(b->gadgets)FreeGadgets(b->gadgets);if(b->visual)FreeVisualInfo(b->visual);if(b->screen)UnlockPubScreen(NULL,b->screen);FreeVec(b);return NULL;
}
static void close_browser(struct Browser *b)
{if(b){ah_browser_live_close(&b->live);if(b->window)CloseWindow(b->window);if(b->gadgets)FreeGadgets(b->gadgets);if(b->visual)FreeVisualInfo(b->visual);if(b->screen)UnlockPubScreen(NULL,b->screen);FreeVec(b);}if(GadToolsBase)CloseLibrary(GadToolsBase);if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);GadToolsBase=NULL;IntuitionBase=NULL;}
int main(int argc,char **argv)
{
 struct Browser *b;struct IntuiMessage *msg;ULONG cls;UWORD id,code;int done=0,ok=1,loops;unsigned int i;char query[64];
 int selftest=argc==2 && !strcmp(argv[1],"SELFTEST"),live_test=argc==2 && !strcmp(argv[1],"LIVE-SELFTEST"),sample=0;
 if(argc>1 && !selftest && !live_test && !sample){puts("AmiHASSBrowser [SELFTEST|LIVE-SELFTEST]");return 20;}
 if(live_test){sample_memory();printf("MEMORY Browser before open: Fast free %lu bytes.\n",AvailMem(MEMF_FAST));}
 b=open_browser(sample||selftest);if(!b){close_browser(NULL);puts("Cannot open AmiHASS Browser (400x240 screen required).");return 20;}
 if(selftest){
  ok=b->count==ah_browser_count();filter_rows(b,"TEMPERATURE");ok=ok && b->count==1;
  GT_SetGadgetAttrs(b->type,b->window,NULL,GTST_String,(ULONG)"light",TAG_DONE);filter_rows(b,"");ok=ok && b->count==2;
  GT_SetGadgetAttrs(b->type,b->window,NULL,GTST_String,(ULONG)"All",TAG_DONE);
  filter_rows(b,"no-such-fixture");ok=ok && !b->count && (b->details->Flags&GFLG_DISABLED);
  filter_rows(b,"");ok=ok && b->count==ah_browser_count() && !(b->details->Flags&GFLG_DISABLED);
  for(i=0;i<ah_browser_count() && ok;++i)ok=show_details(ah_browser_entity(i),1);
  close_browser(b);puts(ok ? "PASS: native sample browser, search, type filters and details.":"FAIL: sample browser self-test.");return ok ? 0:10;
 }
 if(live_test){
  for(loops=0;loops<600;++loops){sample_memory();ah_browser_live_step(&b->live);if(b->live.dirty)publish_rows(b);if(!b->live.wanted&&!b->live.loading&&b->live.count)break;Delay(5);}
  ok=loops<600 && b->live.count>0 && b->live.total>=b->live.count;
  if(ok){unsigned int named=0;for(i=0;i<(unsigned int)b->live.count;++i)if(strcmp(b->live.items[i].name,b->live.items[i].id))++named;
   ok=b->live.count==(b->live.total<b->visible_rows ? b->live.total:b->visible_rows) && named==(unsigned int)b->live.count;
   printf("PASS: live Browser displays %d of %d devices; %u friendly names, %d visible rows.\n",b->live.count,b->live.total,named,b->visible_rows);
   strcpy(b->selected_device,b->live.items[0].id);strcpy(b->device_name,b->live.items[0].name);b->device_view=0;
   ah_browser_live_device_page(&b->live,b->selected_device,"","",0,0);
   for(loops=0;loops<600;++loops){sample_memory();ah_browser_live_step(&b->live);if(b->live.dirty)publish_rows(b);if(!b->live.wanted&&!b->live.loading&&b->live.count)break;Delay(5);}
   ok=loops<600 && b->live.count>0 && strchr(b->live.items[0].id,'.')!=NULL;
   if(ok){b->selected=0;ok=show_live_details(b,1);}
   b->selected_device[0]=0;
   ah_browser_live_page(&b->live,"","",0,0);for(loops=0;loops<600;++loops){sample_memory();ah_browser_live_step(&b->live);if(b->live.dirty)publish_rows(b);if(!b->live.wanted&&!b->live.loading&&b->live.count)break;Delay(5);}ok=ok&&loops<600;
   printf("PASS: all-entities path exposes %d entities.\n",b->live.total);
   if(ok && b->live.total>b->live.count){ah_browser_live_page(&b->live,"","",b->live.page_size,0);for(loops=0;loops<600;++loops){sample_memory();ah_browser_live_step(&b->live);if(b->live.dirty)publish_rows(b);if(!b->live.wanted&&!b->live.loading&&b->live.count)break;Delay(5);}ok=loops<600&&b->live.offset==b->live.page_size;}
   if(ok){strncpy(query,b->live.items[0].id,32);query[32]=0;ah_browser_live_page(&b->live,"",query,0,0);for(loops=0;loops<600;++loops){sample_memory();ah_browser_live_step(&b->live);if(b->live.dirty)publish_rows(b);if(!b->live.wanted&&!b->live.loading&&b->live.count)break;Delay(5);}ok=loops<600&&b->live.count>0;}
   if(ok){b->selected=0;ok=show_live_details(b,1);}
  }
  if(ok&&b->live.sample_mode){b->selected_device[0]=0;b->device_view=1;ok=choose_room(b,1)&&!strcmp(b->room_id,"study")&&!strcmp(b->room_name,"Study");if(!ok){close_browser(b);puts("FAIL: native room selector.");return 10;}GT_SetGadgetAttrs(b->type,b->window,NULL,GTST_String,(ULONG)"light",TAG_DONE);filter_rows(b,"");for(loops=0;loops<600;++loops){ah_browser_live_step(&b->live);if(b->live.dirty)publish_rows(b);if(!b->live.wanted&&!b->live.loading)break;Delay(5);}ok=loops<600&&b->live.total==1&&!strcmp(b->live.items[0].name,"Sample desk");if(ok){strcpy(b->selected_device,b->live.items[0].id);strcpy(b->device_type,"light");strcpy(b->device_search,"desk");b->device_view=0;GT_SetGadgetAttrs(b->type,b->window,NULL,GTST_String,(ULONG)"All",TAG_DONE);return_devices(b);for(loops=0;loops<600;++loops){ah_browser_live_step(&b->live);if(b->live.dirty)publish_rows(b);if(!b->live.wanted&&!b->live.loading)break;Delay(5);}ok=loops<600&&b->live.total==1&&!strcmp(b->live.area,"study")&&!strcmp(b->live.domain,"light");}if(ok)puts("PASS: native Browser room/type filters and Back preserve bounded discovery.");}
  close_browser(b);printf("MEMORY Browser after close: Fast free %lu, minimum %lu bytes.\n",AvailMem(MEMF_FAST),memory_min);puts(ok ? "PASS: live Browser paging, server search and selected-entity state details; read-only.":"FAIL: live Browser self-test.");return ok ? 0:10;
 }
 while(!done){
  if(!b->sample){sample_memory();ah_browser_live_step(&b->live);if(b->live.dirty)publish_rows(b);}
  while((msg=GT_GetIMsg(b->window->UserPort))!=NULL){cls=msg->Class;code=msg->Code;id=cls==IDCMP_GADGETUP && msg->IAddress ? ((struct Gadget *)msg->IAddress)->GadgetID:0;GT_ReplyIMsg(msg);
   if(cls==IDCMP_CLOSEWINDOW || (cls==IDCMP_RAWKEY && code==0x45) || id==102){done=1;break;}
   if(id==1 && code<b->count)b->selected=code;
   if(id==6 && code<2){int previous=b->live.sample_mode;ah_browser_live_close(&b->live);ah_browser_live_init(&b->live);b->live.sample_mode=previous;b->sample=0;b->device_view=code==0;b->selected_device[0]=0;SetWindowTitles(b->window,(STRPTR)(b->sample ? "AmiHASS Browser 0.2 - Sample data":(b->device_view ? "AmiHASS Browser 0.2 - Devices":"AmiHASS Browser 0.2 - Entities")),(STRPTR)-1);filter_rows(b,((struct StringInfo *)b->search->SpecialInfo)->Buffer);}
   if(id==7){if(code==0)set_room(b,"","");else choose_room(b,0);filter_rows(b,((struct StringInfo *)b->search->SpecialInfo)->Buffer);}
   if(id==3 || id==4 || id==100 || id==105)filter_rows(b,((struct StringInfo *)b->search->SpecialInfo)->Buffer);
   if(id==106 && b->selected_device[0])return_devices(b);
   if(!b->sample && !b->live.loading && (id==103 || id==104)){
    int offset=b->live.offset+(id==103 ? -b->live.page_size:b->live.page_size);
    if(offset>=0 && (id==103 || offset<b->live.total))b->device_view ? ah_browser_live_devices_type(&b->live,b->live.domain,b->live.filter,offset,0):ah_browser_live_device_page(&b->live,b->selected_device,b->live.domain,b->live.filter,offset,0);
   }
   if(id==101 && b->selected<b->count){if(b->sample)show_details(ah_browser_entity(b->indices[b->selected]),0);else if(!b->live.loading && b->selected<(unsigned int)b->live.count){
     if(b->device_view){strcpy(b->device_type,((struct StringInfo *)b->type->SpecialInfo)->Buffer);strcpy(b->device_search,((struct StringInfo *)b->search->SpecialInfo)->Buffer);strcpy(b->selected_device,b->live.items[b->selected].id);strcpy(b->device_name,b->live.items[b->selected].name);b->device_view=0;GT_SetGadgetAttrs(b->search,b->window,NULL,GTST_String,(ULONG)"",TAG_DONE);GT_SetGadgetAttrs(b->type,b->window,NULL,GTST_String,(ULONG)"All",TAG_DONE);filter_rows(b,"");}
     else show_live_details(b,0);
    }}
  }
  Delay(5);
 }
 close_browser(b);return 0;
}
