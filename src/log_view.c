/* Read-only viewer for AmiHASS service and connection events. */
#define IntuitionBase AHLogsIntuitionBase
#define GadToolsBase AHLogsGadToolsBase
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
#include "ah_log.h"
#include "preferences.h"
#include "nd_panel.h"
#include "diagnostic_report.h"
struct IntuitionBase *IntuitionBase=NULL;
struct Library *GadToolsBase=NULL;
struct LogView {
 struct Screen *screen;struct Window *window;struct Gadget *gadgets,*list,*status;APTR visual;
 struct List labels;struct Node nodes[AH_LOG_LINES];char lines[AH_LOG_LINES][160],summary[100];int count,first,columns,client_mode;
};
static void add_log_line(struct LogView *view,const char *line)
{
 unsigned int length=(unsigned int)strlen(line),at=0,n,i,last_space;int slot;
 while(at<length){
  n=length-at;if(n>(unsigned int)view->columns)n=view->columns;
  if(at+n<length){last_space=0;for(i=0;i<n;++i)if(line[at+i]==' ')last_space=i;if(last_space>n/2)n=last_space;}
  if(view->count<AH_LOG_LINES)slot=view->count++;
  else{slot=view->first;view->first=(view->first+1)%AH_LOG_LINES;}
  memcpy(view->lines[slot],line+at,n);view->lines[slot][n]=0;
  at+=n;while(line[at]==' ')++at;
 }
}
static void read_log(struct LogView *view,const char *path)
{
 FILE *file=fopen(path,"rb");char line[160];int ch;unsigned int n,i;
 if(!file)return;
 while(fgets(line,sizeof(line),file)){
  n=(unsigned int)strlen(line);if(n && line[n-1]!='\n'){while((ch=fgetc(file))!=EOF && ch!='\n'){} }
  line[strcspn(line,"\r\n")]=0;if(!line[0])continue;
  for(i=0;line[i];++i)if((unsigned char)line[i]<32 || (unsigned char)line[i]==127)line[i]=' ';
  add_log_line(view,line);
 }
 fclose(file);
}
static void publish_lines(struct LogView *view,const char *summary,int tail)
{
 int i,index;
 view->labels.lh_Head=(struct Node *)&view->labels.lh_Tail;view->labels.lh_Tail=NULL;view->labels.lh_TailPred=(struct Node *)&view->labels.lh_Head;
 if(view->count){for(i=0;i<view->count;++i){index=(view->first+i)%AH_LOG_LINES;view->nodes[i].ln_Name=view->lines[index];AddTail(&view->labels,&view->nodes[i]);}}
 else{strcpy(view->lines[0],"No data available.");view->nodes[0].ln_Name=view->lines[0];AddTail(&view->labels,&view->nodes[0]);}
 strncpy(view->summary,summary,sizeof(view->summary)-1);view->summary[sizeof(view->summary)-1]=0;
 GT_SetGadgetAttrs(view->list,view->window,NULL,GTLV_Labels,(ULONG)&view->labels,GTLV_Top,tail && view->count>0 ? view->count-1:0,TAG_DONE);
 GT_SetGadgetAttrs(view->status,view->window,NULL,GTTX_Text,(ULONG)view->summary,TAG_DONE);
}
static void refresh_log(struct LogView *view)
{
 char summary[100];GT_SetGadgetAttrs(view->list,view->window,NULL,GTLV_Labels,~0UL,TAG_DONE);
 view->count=view->first=0;read_log(view,AH_LOG_PREVIOUS);read_log(view,AH_LOG_FILE);
 if(view->count)sprintf(summary,"%d recent log lines - AmiHASS service and connections",view->count);
 else{add_log_line(view,"No logs yet. Start the service or test a connection.");strcpy(summary,"AmiHASS local logs - no events recorded yet");}
 publish_lines(view,summary,1);
}
static int start_diagnostics(struct LogView *view,struct AmiHASSClient *client)
{
 amihass_client_close(client);GT_SetGadgetAttrs(view->list,view->window,NULL,GTLV_Labels,~0UL,TAG_DONE);view->count=view->first=0;
 if(!amihass_send(client,AMIHASS_DIAGNOSTICS,"AmiHASS.Diagnostics",NULL,NULL,0,0,view->client_mode==1 ? "clients":view->client_mode==2 ? "memory":NULL,NULL)){
  add_log_line(view,"Service stopped or unavailable. Start it to collect diagnostics.");publish_lines(view,"No live service - counters are not available",0);return 0;
 }
 GT_SetGadgetAttrs(view->status,view->window,NULL,GTTX_Text,(ULONG)"Reading bridge diagnostics...",TAG_DONE);return 1;
}
static int export_snapshot(const char *path,struct NDPanel *panel)
{struct AmiHASSClient c;struct AmiHASSMessage *m;FILE *f;char temp[340];int phase=0,offset=0,ticks,event,ok=0,rounds=0;memset(&c,0,sizeof(c));if(strlen(path)<4||strcmp(path+strlen(path)-4,".txt"))return 0;sprintf(temp,"%s.new",path);f=fopen(temp,"wb");if(!f)return 0;
 for(;rounds++<12;){if(!amihass_send(&c,phase==0 ? AMIHASS_STATUS:AMIHASS_DIAGNOSTICS,"AmiHASS.Report",NULL,NULL,offset,0,phase==2 ? "connection":phase==3 ? "memory":NULL,NULL))break;m=NULL;for(ticks=0;ticks<400;++ticks){m=amihass_reply(&c);if(m)break;event=nd_panel_event(panel);if(event==-1||event==101)break;Delay(5);}if(!m)break;if(m->result!=AMIHASS_OK||(phase>0&&m->count<=0)){FreeVec(m);break;}if(phase==0){ok=ah_report_status(f,m->item.state);phase=1;}else{ok=ah_report_items(f,m->items,m->count);if(phase==3){FreeVec(m);break;}if(phase==2){phase=3;offset=0;FreeVec(m);continue;}offset=m->offset+m->count;if(offset>=m->total){phase=2;offset=0;}}FreeVec(m);if(!ok)break;ok=0;}
 amihass_client_close(&c);if(fclose(f))ok=0;if(ok){char backup[340];FILE *old;int existed=0;sprintf(backup,"%s.old",path);old=fopen(path,"rb");if(old){fclose(old);remove(backup);if(rename(path,backup))ok=0;else existed=1;}if(ok&&rename(temp,path)){if(existed)rename(backup,path);ok=0;}if(ok&&existed)remove(backup);}if(!ok)remove(temp);return ok;}
static void export_report(void)
{struct NDPanelField fields[3];struct NDPanel *p;static const char *buttons[]={"Export","Close"};char path[300],approved_path[300],status[120];FILE *exists;int event,replace=0;approved_path[0]=0;strcpy(path,"RAM:AmiHASS-Diagnostics.txt");strcpy(status,"Fresh counters and states; no token, URL or response body.");fields[0].label="File";fields[0].value=path;fields[0].limit=sizeof(path);fields[1].label="Includes";fields[1].value="Version, network, error codes and bridge counters.";fields[1].limit=0;fields[2].label="Status";fields[2].value=status;fields[2].limit=0;p=nd_panel_open("AmiHASS - export diagnostics",fields,3,buttons,2);if(!p)return;for(;;){event=nd_panel_event(p);if(event==-1||event==101)break;if(event==1)replace=0;if(event==100){strncpy(path,nd_panel_get(p,0),sizeof(path)-1);path[sizeof(path)-1]=0;exists=fopen(path,"rb");if(exists){fclose(exists);if(!replace||strcmp(path,approved_path)){replace=1;strcpy(approved_path,path);nd_panel_set(p,2,"File exists. Export again to replace; Close cancels.");continue;}}nd_panel_enable(p,1,0);nd_panel_enable(p,100,0);nd_panel_set(p,2,"Collecting read-only snapshot; Close cancels.");event=export_snapshot(path,p);nd_panel_enable(p,1,1);nd_panel_enable(p,100,1);replace=0;nd_panel_set(p,2,event ? "Report saved. No credentials or response bodies included.":"Export failed/cancelled. Use writable .txt path and live service.");}Delay(5);}nd_panel_close(p);}
static int event_view(int diagnostics,int selftest)
{
 struct AmiHASSClient client;struct AmiHASSMessage *reply;int pending=0;
 struct LogView *view;struct Gadget *last;struct NewGadget ng;struct IntuiMessage *msg;struct TextAttr font={(STRPTR)"topaz.font",8,0,0};
 ULONG cls;UWORD id,code;int width,height,top,done=0,i,success=0;static const char *buttons[]={"Refresh","Close"};
 memset(&client,0,sizeof(client));
 view=AllocVec(sizeof(*view),MEMF_PUBLIC|MEMF_CLEAR);if(!view)return 0;view->client_mode=diagnostics==2;
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",39);GadToolsBase=OpenLibrary("gadtools.library",39);if(!IntuitionBase || !GadToolsBase)goto finished;
 view->screen=LockPubScreen("Workbench");if(!view->screen)goto finished;
 width=view->screen->Width-24;if(width>740)width=740;if(width<280)goto finished;
 view->columns=(width-48)/8;if(view->columns>150)view->columns=150;
 height=view->screen->Height-view->screen->BarHeight-20;if(height>340)height=340;if(height<140)goto finished;
 top=view->screen->WBorTop+view->screen->Font->ta_YSize+8;
 view->visual=GetVisualInfo(view->screen,TAG_DONE);if(!view->visual)goto finished;
 last=CreateContext(&view->gadgets);if(!last)goto finished;
 view->labels.lh_Head=(struct Node *)&view->labels.lh_Tail;view->labels.lh_TailPred=(struct Node *)&view->labels.lh_Head;
 memset(&ng,0,sizeof(ng));ng.ng_VisualInfo=view->visual;ng.ng_TextAttr=&font;
 ng.ng_LeftEdge=12;ng.ng_TopEdge=top;ng.ng_Width=width-24;ng.ng_Height=height-top-66;ng.ng_GadgetID=1;
 view->list=last=CreateGadget(LISTVIEW_KIND,last,&ng,GTLV_Labels,(ULONG)&view->labels,GTLV_ReadOnly,TRUE,TAG_DONE);if(!last)goto finished;
 ng.ng_TopEdge=height-58;ng.ng_Height=16;ng.ng_GadgetID=2;
 view->status=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)"Reading AmiHASS service logs...",TAG_DONE);if(!last)goto finished;
 for(i=0;i<2;++i){ng.ng_LeftEdge=width-228+i*108;ng.ng_TopEdge=height-32;ng.ng_Width=100;ng.ng_Height=22;ng.ng_GadgetID=100+i;ng.ng_GadgetText=(STRPTR)buttons[i];ng.ng_Flags=PLACETEXT_IN;
  last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto finished;}
 if(diagnostics){ng.ng_LeftEdge=12;ng.ng_GadgetID=102;ng.ng_GadgetText="Totals/Clients/RAM";ng.ng_Width=148;last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto finished;}
 if(diagnostics){ng.ng_LeftEdge=168;ng.ng_GadgetID=103;ng.ng_GadgetText="Export";ng.ng_Width=80;last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto finished;}
 view->window=OpenWindowTags(NULL,WA_Title,(ULONG)(diagnostics ? "AmiHASS - Diagnostics":"AmiHASS - Server logs"),WA_PubScreen,(ULONG)view->screen,
  WA_Left,(view->screen->Width-width)/2,WA_Top,view->screen->BarHeight+10,WA_Width,width,WA_Height,height,
  WA_Flags,WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE|WFLG_SMART_REFRESH,
  WA_IDCMP,LISTVIEWIDCMP|BUTTONIDCMP|IDCMP_CLOSEWINDOW|IDCMP_RAWKEY,WA_Gadgets,(ULONG)view->gadgets,TAG_DONE);
 if(!view->window)goto finished;GT_RefreshWindow(view->window,NULL);if(diagnostics)pending=start_diagnostics(view,&client);else refresh_log(view);
 while(!done){
  if(diagnostics && pending){
   reply=amihass_reply(&client);
   if(reply){
    if(reply->result!=AMIHASS_OK){add_log_line(view,"Diagnostics unavailable. Update AmiHASSService to the current build.");publish_lines(view,"Service does not provide diagnostic counters",0);pending=0;}
    else{
     int row;char line[160];
     for(row=0;row<reply->count;++row){struct AmiHASSEntity *e=&reply->items[row];if(view->client_mode==1){if(!strcmp(e->device_class,"entity"))snprintf(line,sizeof(line),"  %.95s",e->id);else snprintf(line,sizeof(line),"%.19s: requests %.39s; queries %.15s; lists %.39s; actions %.23s",e->name,e->state,e->unit,e->updated,e->device_class);}else sprintf(line,"%.63s: %.39s",e->name,e->state);add_log_line(view,line);}
     if(reply->offset+reply->count<reply->total){
      pending=amihass_send(&client,AMIHASS_DIAGNOSTICS,"AmiHASS.Diagnostics",NULL,NULL,reply->offset+reply->count,0,view->client_mode==1 ? "clients":view->client_mode==2 ? "memory":NULL,NULL);
      if(!pending)publish_lines(view,"Partial snapshot - service unavailable",0);
     }else{publish_lines(view,view->client_mode==1 ? "Recent 120s; 16 clients/64 selections max. Counts since tracking; last 128 lines.":view->client_mode==2 ? "System-wide Fast bytes; observed HTTP drop, not process allocation. Refresh to update.":"Since service start; includes observer traffic. Refresh to update.",0);success=1;pending=0;}
    }
    FreeVec(reply);
   }else if(!client.pending){add_log_line(view,"Service did not reply. Refresh to retry.");publish_lines(view,"Diagnostic request timed out",0);pending=0;}
  }
  if(selftest&&!pending)done=1;
  while((msg=GT_GetIMsg(view->window->UserPort))!=NULL){
  cls=msg->Class;code=msg->Code;id=cls==IDCMP_GADGETUP && msg->IAddress ? ((struct Gadget *)msg->IAddress)->GadgetID:0;GT_ReplyIMsg(msg);
  if(cls==IDCMP_CLOSEWINDOW || (cls==IDCMP_RAWKEY && code==0x45) || id==101){done=1;break;}
  if(id==103&&diagnostics){amihass_client_close(&client);pending=0;export_report();}
  if(id==102&&diagnostics){view->client_mode=(view->client_mode+1)%3;pending=start_diagnostics(view,&client);}
  if(id==100){if(diagnostics)pending=start_diagnostics(view,&client);else refresh_log(view);}
 }Delay(5);}
finished:
 amihass_client_close(&client);
 if(view->window)CloseWindow(view->window);if(view->gadgets)FreeGadgets(view->gadgets);if(view->visual)FreeVisualInfo(view->visual);if(view->screen)UnlockPubScreen(NULL,view->screen);
 if(GadToolsBase)CloseLibrary(GadToolsBase);GadToolsBase=NULL;if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);IntuitionBase=NULL;FreeVec(view);return success;
}

void amihass_logs_ui(void){event_view(0,0);}
void amihass_diagnostics_ui(void){event_view(1,0);}

int amihass_diagnostics_check(void){return event_view(2,1)&&export_snapshot("AutoRun:diagnostics.txt",NULL);}
