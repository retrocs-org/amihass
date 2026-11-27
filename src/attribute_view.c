/* Read-only list of every attribute of one entity (SDK ATTRIBUTES), with a
 * 24-hour summary of its numeric history (SDK HISTORY) when it has one. */
#define IntuitionBase AHAttrIntuitionBase
#define GadToolsBase AHAttrGadToolsBase
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <stdio.h>
#include <string.h>
#include "ah_locale.h"
#include "amihass.h"
#include "amihass_core.h"
#include "wait.h"
#include "attribute_view.h"
struct IntuitionBase *IntuitionBase=NULL;
struct Library *GadToolsBase=NULL;
#define LINES 72
struct AttrView {struct Screen *screen;struct Window *window;struct Gadget *gadgets,*list,*status;APTR visual;struct List labels;struct Node nodes[LINES];char lines[LINES][160],summary[100];int count;};
static void add_line(struct AttrView *v,const char *line)
{unsigned int i;if(v->count>=LINES)return;strncpy(v->lines[v->count],line,159);v->lines[v->count][159]=0;for(i=0;v->lines[v->count][i];++i)if((unsigned char)v->lines[v->count][i]<32)v->lines[v->count][i]=' ';++v->count;}
static void publish(struct AttrView *v,const char *summary)
{int i;v->labels.lh_Head=(struct Node *)&v->labels.lh_Tail;v->labels.lh_Tail=NULL;v->labels.lh_TailPred=(struct Node *)&v->labels.lh_Head;
 if(!v->count)add_line(v,AH_TR(MSG_ATTR_NONE));for(i=0;i<v->count;++i){v->nodes[i].ln_Name=v->lines[i];AddTail(&v->labels,&v->nodes[i]);}
 strncpy(v->summary,summary,sizeof(v->summary)-1);GT_SetGadgetAttrs(v->list,v->window,NULL,GTLV_Labels,(ULONG)&v->labels,GTLV_Top,0,TAG_DONE);GT_SetGadgetAttrs(v->status,v->window,NULL,GTTX_Text,(ULONG)ah_tx((const char *)(v->summary)),TAG_DONE);}
/* GadTools lists must be detached before their nodes or text change. */
static void detach(struct AttrView *v){GT_SetGadgetAttrs(v->list,v->window,NULL,GTLV_Labels,~0UL,TAG_DONE);}
/* Smallest, largest and latest of the numeric history, as text. */
static void hundredths(long v,char *out){sprintf(out,"%s%ld.%02ld",v<0?"-":"",(v<0?-v:v)/100,(v<0?-v:v)%100);}
struct HistorySum {long lo,hi;int n;char last[40];};
static void history_add(struct HistorySum *h,const struct AmiHASSMessage *m)
{int i;long x;for(i=0;i<m->count;++i)if(amihass_history_value(m->items[i].state,&x)){if(!h->n||x<h->lo)h->lo=x;if(!h->n||x>h->hi)h->hi=x;++h->n;strcpy(h->last,m->items[i].state);}}
static void history_line(struct AttrView *v,const struct HistorySum *h,const char *unit)
{char line[160],a[24],b[24];if(!h->n)return;hundredths(h->lo,a);hundredths(h->hi,b);
 sprintf(line,AH_TR(MSG_ATTR_HISTORY),h->last,unit[0]?" ":"",unit,a,b);add_line(v,"");add_line(v,line);}
int amihass_attributes_ui(const char *entity)
{
 struct AmiHASSClient client;struct AmiHASSMessage *reply;struct AttrView *v;struct Gadget *last;struct NewGadget ng;struct IntuiMessage *msg;struct TextAttr font={(STRPTR)"topaz.font",8,0,0};
 ULONG cls;UWORD id,code;int width,height,top,done=0,i,stage=0,offset=0,again=0,tries=0,fresh=0;char title[140],line[160],unit[16];struct HistorySum sum;const char *buttons[2];
 if(!amihass_entity_id(entity))return 0;buttons[0]=AH_TR(MSG_REFRESH);buttons[1]=AH_TR(MSG_CLOSE);memset(&client,0,sizeof(client));memset(&sum,0,sizeof(sum));unit[0]=0;
 v=AllocVec(sizeof(*v),MEMF_PUBLIC|MEMF_CLEAR);if(!v)return 0;
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",39);GadToolsBase=OpenLibrary("gadtools.library",39);if(!IntuitionBase||!GadToolsBase)goto finished;
 v->screen=LockPubScreen(NULL);if(!v->screen)goto finished;
 width=v->screen->Width-24;if(width>640)width=640;if(width<280)goto finished;
 height=v->screen->Height-v->screen->BarHeight-20;if(height>300)height=300;if(height<140)goto finished;
 top=v->screen->WBorTop+v->screen->Font->ta_YSize+8;
 v->visual=GetVisualInfo(v->screen,TAG_DONE);if(!v->visual)goto finished;
 last=CreateContext(&v->gadgets);if(!last)goto finished;
 v->labels.lh_Head=(struct Node *)&v->labels.lh_Tail;v->labels.lh_TailPred=(struct Node *)&v->labels.lh_Head;
 memset(&ng,0,sizeof(ng));ng.ng_VisualInfo=v->visual;ng.ng_TextAttr=&font;
 ng.ng_LeftEdge=12;ng.ng_TopEdge=top;ng.ng_Width=width-24;ng.ng_Height=height-top-66;ng.ng_GadgetID=1;
 v->list=last=CreateGadget(LISTVIEW_KIND,last,&ng,GTLV_Labels,(ULONG)&v->labels,GTLV_ReadOnly,TRUE,TAG_DONE);if(!last)goto finished;
 ng.ng_TopEdge=height-58;ng.ng_Height=16;ng.ng_GadgetID=2;
 v->status=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)ah_tx((const char *)(AH_TR(MSG_ATTR_READING))),TAG_DONE);if(!last)goto finished;
 for(i=0;i<2;++i){ng.ng_LeftEdge=width-228+i*108;ng.ng_TopEdge=height-32;ng.ng_Width=100;ng.ng_Height=22;ng.ng_GadgetID=100+i;ng.ng_GadgetText=(STRPTR)ah_tx((const char *)((STRPTR)buttons[i]));ng.ng_Flags=PLACETEXT_IN;last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto finished;}
 sprintf(title,ah_txf("AmiHASS - %.100s"),entity);
 v->window=OpenWindowTags(NULL,WA_Title,(ULONG)ah_tx((const char *)(title)),WA_PubScreen,(ULONG)v->screen,WA_Left,(v->screen->Width-width)/2,WA_Top,v->screen->BarHeight+12,WA_Width,width,WA_Height,height,
  WA_Flags,WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE|WFLG_SMART_REFRESH,WA_IDCMP,LISTVIEWIDCMP|BUTTONIDCMP|IDCMP_CLOSEWINDOW|IDCMP_RAWKEY,WA_Gadgets,(ULONG)v->gadgets,TAG_DONE);
 if(!v->window)goto finished;GT_RefreshWindow(v->window,NULL);
 /* stage 1 attributes, 2 history, 0 idle; `again` counts down to a re-ask. */
 stage=amihass_attributes(&client,"AmiHASS.Attributes",entity,0,0)?1:0;if(!stage)publish(v,AH_TR(MSG_ATTR_NOT_RUNNING));
 while(!done){
  if(again&&!--again){if(!(stage==1?amihass_attributes(&client,"AmiHASS.Attributes",entity,offset,fresh):amihass_history(&client,"AmiHASS.Attributes",entity,24,0))){publish(v,AH_TR(MSG_ATTR_STOPPED));stage=0;}}
  if(stage&&!again&&(reply=amihass_reply(&client))!=NULL){
   if(reply->result==AMIHASS_WAIT&&++tries<40)again=10;
   else if(stage==1&&reply->result==AMIHASS_OK){fresh=0;tries=0;detach(v);
    for(i=0;i<reply->count;++i){sprintf(line,"%-24.24s %.95s",reply->items[i].id,reply->items[i].name);if(strcmp(reply->items[i].device_class,"string")&&strcmp(reply->items[i].device_class,"number"))sprintf(line+strlen(line),ah_txf(" (%.10s)"),reply->items[i].device_class);add_line(v,line);if(!strcmp(reply->items[i].id,"unit_of_measurement"))strncpy(unit,reply->items[i].name,sizeof(unit)-1);}
    offset=(int)(reply->offset+reply->count);
    if(reply->count>0&&offset<reply->total&&offset<AH_DETAIL_ITEMS){if(!amihass_attributes(&client,"AmiHASS.Attributes",entity,offset,0))stage=0;}
    else{sprintf(v->summary,AH_TR(MSG_ATTR_COUNT),(long)reply->total);if(reply->total>AH_DETAIL_ITEMS)strcat(v->summary,AH_TR(MSG_ATTR_FIRST64));if(!strncmp(entity,"sensor.",7)||!strncmp(entity,"input_number.",13)||!strncmp(entity,"number.",7)){stage=amihass_history(&client,"AmiHASS.Attributes",entity,24,0)?2:0;}else stage=0;strcpy(line,v->summary);publish(v,line);}}
   else if(stage==2&&reply->result==AMIHASS_OK&&reply->count>0&&reply->offset+reply->count<reply->total){history_add(&sum,reply);tries=0;if(!amihass_history(&client,"AmiHASS.Attributes",entity,24,(int)(reply->offset+reply->count)))stage=0;}
   else if(stage==2){stage=0;detach(v);if(reply->result==AMIHASS_OK){history_add(&sum,reply);history_line(v,&sum,unit);}strcpy(line,v->summary);publish(v,line);}
   else{sprintf(line,"%s",reply->result==AMIHASS_WAIT?AH_TR(MSG_ATTR_SLOW):amihass_problem((int)reply->result));if(stage==1){detach(v);v->count=0;publish(v,line);}else GT_SetGadgetAttrs(v->status,v->window,NULL,GTTX_Text,(ULONG)ah_tx((const char *)(v->summary)),TAG_DONE);stage=0;}
   FreeVec(reply);}
  else if(stage&&!again&&!client.pending){publish(v,AH_TR(MSG_ATTR_NO_REPLY));stage=0;}
  while((msg=GT_GetIMsg(v->window->UserPort))!=NULL){cls=msg->Class;code=msg->Code;id=cls==IDCMP_GADGETUP&&msg->IAddress?((struct Gadget *)msg->IAddress)->GadgetID:0;GT_ReplyIMsg(msg);
   if(cls==IDCMP_CLOSEWINDOW||(cls==IDCMP_RAWKEY&&code==0x45)||id==101){done=1;break;}
   if(id==100){amihass_client_close(&client);detach(v);v->count=0;offset=0;memset(&sum,0,sizeof(sum));tries=0;again=0;fresh=1;GT_SetGadgetAttrs(v->status,v->window,NULL,GTTX_Text,(ULONG)ah_tx((const char *)(AH_TR(MSG_ATTR_READING))),TAG_DONE);stage=amihass_attributes(&client,"AmiHASS.Attributes",entity,0,1)?1:0;if(!stage)publish(v,AH_TR(MSG_ATTR_NOT_RUNNING));}}
  ah_port_wait(v->window->UserPort,amihass_client_signal(&client),5);}
finished:
 amihass_client_close(&client);
 if(v->window)CloseWindow(v->window);if(v->gadgets)FreeGadgets(v->gadgets);if(v->visual)FreeVisualInfo(v->visual);if(v->screen)UnlockPubScreen(NULL,v->screen);
 if(GadToolsBase)CloseLibrary(GadToolsBase);GadToolsBase=NULL;if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);IntuitionBase=NULL;FreeVec(v);return 1;
}
