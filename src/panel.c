#define IntuitionBase NDPanelIntuitionBase
#define GadToolsBase NDPanelGadToolsBase
#define GfxBase NDPanelGfxBase
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <devices/inputevent.h>
#include "nd_panel.h"
#include "amihass_logo.h"
struct IntuitionBase *IntuitionBase=NULL;
struct Library *GadToolsBase=NULL;
struct GfxBase *GfxBase=NULL;
struct NDPanel {unsigned int save_id;unsigned long choices,sliders;unsigned int choice_index[12];int source_index,secret_field,secret_shown;char secret[1025];struct Gadget *secret_button;
    struct Screen *screen;struct Window *window;
    struct Gadget *gadgets,*fields[12],*boot,*reconnect,*retry,*source;APTR visual;
    UWORD background;int group_top;
    unsigned int count,limits[12],retry_index;char title[128],text[12][160],labels[12][32];
};
void nd_panel_close(struct NDPanel *p)
{
    if(p){
        if(p->window)CloseWindow(p->window);
        if(p->gadgets){unsigned int i;for(i=0;i<p->count;++i)if(p->limits[i] && p->fields[i] && p->fields[i]->SpecialInfo){
            struct StringInfo *info=(struct StringInfo *)p->fields[i]->SpecialInfo;
            if(info->Buffer){volatile unsigned char *bytes=(volatile unsigned char *)info->Buffer;unsigned int n=info->MaxChars;while(n--)*bytes++=0;}
        }}
        if(p->gadgets){unsigned int i;for(i=0;i<p->count;++i)if(p->limits[i]&&p->fields[i]&&p->fields[i]->SpecialInfo){struct StringInfo *info=(struct StringInfo *)p->fields[i]->SpecialInfo;if(info->UndoBuffer){volatile unsigned char *bytes=(volatile unsigned char *)info->UndoBuffer;unsigned int n=info->MaxChars;while(n--)*bytes++=0;}}}
        if(p->gadgets)FreeGadgets(p->gadgets);
        if(p->visual)FreeVisualInfo(p->visual);
        if(p->screen)UnlockPubScreen(NULL,p->screen);
        {volatile unsigned char *bytes=(volatile unsigned char *)p->secret;unsigned int n=sizeof(p->secret);while(n--)*bytes++=0;}
        FreeVec(p);
    }
    if(GadToolsBase){CloseLibrary(GadToolsBase);GadToolsBase=NULL;}
    if(GfxBase){CloseLibrary((struct Library *)GfxBase);GfxBase=NULL;}
    if(IntuitionBase){CloseLibrary((struct Library *)IntuitionBase);IntuitionBase=NULL;}
}
static void panel_logo(struct NDPanel *p)
{
 struct DrawInfo *di;unsigned int y,x,end;UWORD pens[4];int left,top;
 if(strcmp(p->title,"About AmiHASS"))return;
 di=GetScreenDrawInfo(p->screen);if(!di)return;
 pens[0]=di->dri_Pens[BACKGROUNDPEN];pens[1]=di->dri_Pens[SHADOWPEN];pens[2]=di->dri_Pens[SHINEPEN];pens[3]=di->dri_Pens[FILLPEN];
 left=(p->window->Width-48)/2;top=p->screen->WBorTop+p->screen->Font->ta_YSize+7;
 for(y=0;y<48;++y)for(x=0;x<48;x=end){unsigned int colour=ah_logo_pixels[y][x];for(end=x+1;end<48&&ah_logo_pixels[y][end]==colour;++end);if(colour){SetAPen(p->window->RPort,pens[colour]);RectFill(p->window->RPort,left+x,top+y,left+end-1,top+y);}}
 FreeScreenDrawInfo(p->screen,di);
}
static struct NDPanel *panel_open(const char *title,const struct NDPanelField *fields,
                              unsigned int count,const char **buttons,unsigned int buttons_count,int prefs,int boot,int server)
{
    struct NDPanel *p;struct Gadget *last;
    struct NewGadget ng;struct TextAttr font={(STRPTR)"topaz.font",8,0,0};
    unsigned int i;int w,top,bw,label_width=76,n,right,controls_y,height;int trace=!strcmp(title,"Dashboard choice controls QA");
    if(!count || count>12 || buttons_count>(prefs ? 6:5) || !buttons_count)return NULL;
    p=(struct NDPanel *)AllocVec(sizeof(*p),MEMF_PUBLIC|MEMF_CLEAR);if(!p)return NULL;
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",39);
    GadToolsBase=OpenLibrary("gadtools.library",39);
    GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",39);
    if(!IntuitionBase || !GadToolsBase || !GfxBase)goto fail;if(trace){puts("QA: panel libraries open");fflush(stdout);}
    p->screen=LockPubScreen("Workbench");if(!p->screen)goto fail;
    {struct DrawInfo *info=GetScreenDrawInfo(p->screen);
     if(info){p->background=info->dri_Pens[BACKGROUNDPEN];FreeScreenDrawInfo(p->screen,info);}}
    w=p->screen->Width-24;if(w>(prefs || buttons_count==5 ? 620:560))w=prefs || buttons_count==5 ? 620:560;if(w<280)goto fail;
    p->visual=GetVisualInfo(p->screen,TAG_DONE);if(!p->visual)goto fail;
    for(i=0;i<count;++i){n=(int)strlen(fields[i].label)*8+16;if(n>label_width)label_width=n;}
    if(label_width>w/2)label_width=w/2;
    for(i=0;i<buttons_count;++i)if(!strcmp(buttons[i],"Save"))p->save_id=100+i;
    if(server&&count>1){p->secret_field=2;strncpy(p->secret,fields[1].value,sizeof(p->secret)-1);}
    p->count=count;strncpy(p->title,title,sizeof(p->title)-1);
    last=CreateContext(&p->gadgets);if(!last)goto fail;
    memset(&ng,0,sizeof(ng));ng.ng_VisualInfo=p->visual;ng.ng_TextAttr=&font;
    top=p->screen->WBorTop+p->screen->Font->ta_YSize+10;
    if(!strcmp(title,"About AmiHASS"))top+=54;
    if(top+(int)count*20+40>p->screen->Height-p->screen->BarHeight-10)goto fail;
    for(i=0;i<count;++i){
        ng.ng_LeftEdge=label_width;ng.ng_TopEdge=top+(int)i*20;ng.ng_Width=(prefs && w>=600 ? w-276:w)-label_width-16;ng.ng_Height=16;
        strncpy(p->labels[i],fields[i].label,sizeof(p->labels[i])-1);
        ng.ng_GadgetText=p->labels[i];ng.ng_Flags=PLACETEXT_LEFT;ng.ng_GadgetID=i+1;
        p->limits[i]=fields[i].limit;if(p->secret_field==(int)i+1)ng.ng_Width-=66;
        if(fields[i].limit==ND_PANEL_PERCENT||fields[i].limit==ND_PANEL_BYTE){unsigned int maximum=fields[i].limit==ND_PANEL_BYTE ? 255:100,level=(unsigned int)atoi(fields[i].value);if(level>maximum)level=maximum;p->limits[i]=0;p->sliders|=1UL<<i;p->choice_index[i]=level;ng.ng_Width-=40;last=CreateGadget(SLIDER_KIND,last,&ng,GTSL_Min,0,GTSL_Max,maximum,GTSL_Level,level,GTSL_LevelFormat,(ULONG)"%ld",GTSL_MaxLevelLen,3,GTSL_LevelPlace,PLACETEXT_RIGHT,TAG_DONE);
        }else if(fields[i].limit==ND_PANEL_CHOICE){ng.ng_Height=20;static STRPTR initial[]={"Read only",NULL};p->limits[i]=0;p->choices|=1UL<<i;last=CreateGadget(CYCLE_KIND,last,&ng,GTCY_Labels,(ULONG)initial,GTCY_Active,0,TAG_DONE);
        }else if(fields[i].limit==ND_PANEL_CHECKBOX){
            p->limits[i]=0;ng.ng_Width=26;
            last=CreateGadget(CHECKBOX_KIND,last,&ng,GTCB_Checked,fields[i].value[0]=='1',TAG_DONE);
        }else if(fields[i].limit)
            last=CreateGadget(STRING_KIND,last,&ng,GTST_String,(ULONG)(p->secret_field==(int)i+1 ? (p->secret[0] ? "Token hidden - Show to edit":""):fields[i].value),GA_Disabled,p->secret_field==(int)i+1,
                             GTST_MaxChars,fields[i].limit-1,TAG_DONE);
        else {
            strncpy(p->text[i],fields[i].value,sizeof(p->text[i])-1);
            last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)p->text[i],TAG_DONE);
        }
        if(trace){printf("QA: panel field %u created\n",i);fflush(stdout);}if(!last)goto fail;p->fields[i]=last;
    }
    if(server){
        ng.ng_LeftEdge=w-76;ng.ng_TopEdge=top+20;ng.ng_Width=60;ng.ng_Height=16;ng.ng_GadgetID=114;ng.ng_GadgetText="Show";ng.ng_Flags=PLACETEXT_IN;
        p->secret_button=last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto fail;
        static STRPTR intervals[]={"1 minute","10 minutes","15 minutes","30 minutes",NULL};
        ng.ng_LeftEdge=12;ng.ng_TopEdge=top+(int)count*20+4;ng.ng_Width=26;ng.ng_Height=18;
        ng.ng_GadgetID=111;ng.ng_GadgetText="Automatic reconnect";ng.ng_Flags=PLACETEXT_RIGHT;
        p->reconnect=last=CreateGadget(CHECKBOX_KIND,last,&ng,GTCB_Checked,boot,TAG_DONE);if(!last)goto fail;
        ng.ng_LeftEdge=w-166;ng.ng_Width=154;ng.ng_Height=22;ng.ng_GadgetID=112;ng.ng_GadgetText="Retry every";ng.ng_Flags=PLACETEXT_LEFT;
        p->retry=last=CreateGadget(CYCLE_KIND,last,&ng,GTCY_Labels,(ULONG)intervals,GTCY_Active,0,GA_Disabled,!boot,TAG_DONE);if(!last)goto fail;
        top+=30;
        p->group_top=top+(int)count*20+38;
        for(i=0;i<5;++i){
            ng.ng_Height=22;ng.ng_GadgetID=100+i;ng.ng_GadgetText=(STRPTR)buttons[i];ng.ng_Flags=PLACETEXT_IN;
            ng.ng_TopEdge=top+(int)count*20+8;
            if(i<2){ng.ng_LeftEdge=12+(int)i*136;ng.ng_Width=128;}
            else{ng.ng_TopEdge+=36;ng.ng_LeftEdge=i==2 ? 20:w-220+(int)(i-3)*108;ng.ng_Width=i==2 ? 152:100;}
            last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto fail;
        }
        height=ng.ng_TopEdge+ng.ng_Height+12;
    }else if(prefs){
        right=w>=600 ? w-256:12;
        controls_y=top+(w>=600 ? 0:(int)count*20+40);
        {static STRPTR sources[]={"Server data","Sample data",NULL};
        ng.ng_LeftEdge=104;ng.ng_TopEdge=top+(w>=600 ? 104:(int)count*20+8);ng.ng_Width=w>=600 ? w-396:w-120;ng.ng_Height=22;
        ng.ng_GadgetID=113;ng.ng_GadgetText="Data source";ng.ng_Flags=PLACETEXT_LEFT;
        p->source=last=CreateGadget(CYCLE_KIND,last,&ng,GTCY_Labels,(ULONG)sources,GTCY_Active,0,TAG_DONE);if(!last)goto fail;}

        for(i=0;i<2;++i){
            ng.ng_LeftEdge=right+(int)i*116;ng.ng_TopEdge=controls_y;
            ng.ng_Width=108;ng.ng_Height=22;ng.ng_GadgetID=100+i;
            ng.ng_GadgetText=(STRPTR)buttons[i];ng.ng_Flags=PLACETEXT_IN;
            last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto fail;
        }
        ng.ng_LeftEdge=right;ng.ng_TopEdge=controls_y+34;ng.ng_Width=26;ng.ng_Height=18;
        ng.ng_GadgetID=106;ng.ng_GadgetText="Start at system boot";ng.ng_Flags=PLACETEXT_RIGHT;
        p->boot=last=CreateGadget(CHECKBOX_KIND,last,&ng,GTCB_Checked,boot,TAG_DONE);if(!last)goto fail;
        ng.ng_LeftEdge=right;ng.ng_TopEdge=controls_y+62;ng.ng_Width=224;ng.ng_Height=22;
        ng.ng_GadgetID=107;ng.ng_GadgetText="Show server logs";ng.ng_Flags=PLACETEXT_IN;
        last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto fail;
        for(i=0;i<2;++i){
            ng.ng_LeftEdge=right+(int)i*116;ng.ng_TopEdge=controls_y+88;ng.ng_Width=108;ng.ng_Height=20;
            ng.ng_GadgetID=108+i;ng.ng_GadgetText=i ? "Diagnostics":"About";
            last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto fail;
        }
        ng.ng_LeftEdge=right;ng.ng_TopEdge=controls_y+114;ng.ng_Width=224;ng.ng_Height=22;
        ng.ng_GadgetID=110;ng.ng_GadgetText="Device Browser";
        last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto fail;
        controls_y=w>=600 ? top+(int)count*20+64:controls_y+154;
        for(i=2;i<6;++i){
            ng.ng_LeftEdge=12+(int)(i-2)*(w-24)/4;ng.ng_TopEdge=controls_y;
            ng.ng_Width=(w-24)/4-6;ng.ng_Height=22;ng.ng_GadgetID=100+i;
            ng.ng_GadgetText=(STRPTR)buttons[i];ng.ng_Flags=PLACETEXT_IN;
            last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto fail;
        }
        height=controls_y+34;
    }else{
    bw=(w-32)/(int)buttons_count;
    for(i=0;i<buttons_count;++i){
        ng.ng_LeftEdge=12+(int)i*bw;ng.ng_TopEdge=top+(int)count*20+6;
        ng.ng_Width=bw-6;ng.ng_Height=20;ng.ng_GadgetID=100+i;
        ng.ng_GadgetText=(STRPTR)buttons[i];ng.ng_Flags=PLACETEXT_IN;
        last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto fail;
    }
    height=ng.ng_TopEdge+ng.ng_Height+12;
    }
    if(height>p->screen->Height-p->screen->BarHeight-10)goto fail;
    if(trace){puts("QA: opening choice window");fflush(stdout);}
    p->window=OpenWindowTags(NULL,WA_Title,(ULONG)p->title,WA_PubScreen,(ULONG)p->screen,
        WA_Left,(p->screen->Width-w)/2,WA_Top,p->screen->BarHeight+10,WA_Width,w,
        WA_Height,height,
        WA_Flags,WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE|WFLG_SIMPLE_REFRESH,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|IDCMP_REFRESHWINDOW|IDCMP_RAWKEY|CYCLEIDCMP,
        WA_Gadgets,(ULONG)p->gadgets,TAG_DONE);
    if(!p->window)goto fail;if(trace){puts("QA: rendering choice window");fflush(stdout);}GT_RefreshWindow(p->window,NULL);panel_logo(p);
    if(server)DrawBevelBox(p->window->RPort,12,p->group_top,168,34,GT_VisualInfo,(ULONG)p->visual,TAG_DONE);
    if(p->limits[0])ActivateGadget(p->fields[0],p->window,NULL);return p;
fail:nd_panel_close(p);return NULL;
}
void nd_panel_raise(struct NDPanel *p)
{if(p && p->window){WindowToFront(p->window);ActivateWindow(p->window);}}
int nd_panel_event(struct NDPanel *p)
{
    struct IntuiMessage *msg;ULONG cls;UWORD code,id,qual;
    if(!p)return 0;
    while((msg=GT_GetIMsg(p->window->UserPort))!=NULL){
        cls=msg->Class;code=msg->Code;qual=msg->Qualifier;
        id=cls==IDCMP_GADGETUP ? ((struct Gadget *)msg->IAddress)->GadgetID:0;
        GT_ReplyIMsg(msg);
        if(cls==IDCMP_RAWKEY&&code==0x21&&(qual&(IEQUALIFIER_CONTROL|IEQUALIFIER_RCOMMAND))&&p->save_id&&nd_panel_enabled(p,p->save_id))return p->save_id;
        if(cls==IDCMP_CLOSEWINDOW || (cls==IDCMP_RAWKEY && code==0x45))return -1;
        if(cls==IDCMP_REFRESHWINDOW){GT_BeginRefresh(p->window);panel_logo(p);if(p->group_top)DrawBevelBox(p->window->RPort,12,p->group_top,168,34,GT_VisualInfo,(ULONG)p->visual,TAG_DONE);GT_EndRefresh(p->window,TRUE);}
        if(id>0&&id<=p->count&&((p->choices|p->sliders)&(1UL<<(id-1))))p->choice_index[id-1]=code;
        if(id==114){nd_panel_show_secret(p,!p->secret_shown);return 0;}
        if(id==111)nd_panel_enable(p,112,nd_panel_reconnect(p));
        if(id==112 && code<4)p->retry_index=code;
        if(id==113 && code<2)p->source_index=code;
        if(id)return (int)id;
    }
    return 0;
}
const char *nd_panel_get(struct NDPanel *p,unsigned int index)
{if(p&&p->secret_field==(int)index+1&&!p->secret_shown)return p->secret;return p && index<p->count && p->limits[index] && p->fields[index]->SpecialInfo ?
    ((struct StringInfo *)p->fields[index]->SpecialInfo)->Buffer:"";}
void nd_panel_set_label(struct NDPanel *p,unsigned int index,const char *label)
{
    struct Gadget *g;struct RastPort *rp;UWORD pen,mode;
    if(!p || index>=p->count || !label || !strcmp(p->labels[index],label))return;
    strncpy(p->labels[index],label,sizeof(p->labels[index])-1);
    p->labels[index][sizeof(p->labels[index])-1]=0;
    g=p->fields[index];rp=p->window->RPort;pen=rp->FgPen;mode=rp->DrawMode;
    SetDrMd(rp,JAM1);SetAPen(rp,p->background);
    RectFill(rp,4,g->TopEdge,g->LeftEdge-5,g->TopEdge+g->Height-1);
    SetAPen(rp,pen);SetDrMd(rp,mode);
    RefreshGList(g,p->window,NULL,1);
}
void nd_panel_set(struct NDPanel *p,unsigned int index,const char *text)
{if(p && index<p->count && strcmp(p->text[index],text)){
    strncpy(p->text[index],text,sizeof(p->text[index])-1);p->text[index][sizeof(p->text[index])-1]=0;
    GT_SetGadgetAttrs(p->fields[index],p->window,NULL,GTTX_Text,(ULONG)p->text[index],TAG_DONE);
}}
void nd_panel_title(struct NDPanel *p,const char *title)
{if(p && strcmp(p->title,title)){strncpy(p->title,title,sizeof(p->title)-1);p->title[sizeof(p->title)-1]=0;SetWindowTitles(p->window,p->title,(STRPTR)-1);}}
void nd_panel_beep(struct NDPanel *p)
{if(IntuitionBase)DisplayBeep(p ? p->screen:NULL);}

unsigned long nd_panel_signals(const struct NDPanel *p)
{return p && p->window ? 1UL<<p->window->UserPort->mp_SigBit:0;}

struct NDPanel *nd_panel_open(const char *title,const struct NDPanelField *fields,
                              unsigned int count,const char **buttons,unsigned int buttons_count)
{return panel_open(title,fields,count,buttons,buttons_count,0,0,0);}
struct NDPanel *nd_panel_preferences(const struct NDPanelField *fields,int boot)
{static const char *buttons[]={"Start","Stop","Server Settings","General Settings","Save","Close"};
 return panel_open("AmiHASS Preferences",fields,4,buttons,6,1,boot,0);}
int nd_panel_checked(struct NDPanel *p)
{return p && p->boot && (p->boot->Flags&GFLG_SELECTED)!=0;}

void nd_panel_enable(struct NDPanel *p,unsigned int id,int enabled)
{struct Gadget *g;if(!p)return;if(p->secret_field==(int)id&&!p->secret_shown)enabled=0;for(g=p->gadgets;g;g=g->NextGadget)if(g->GadgetID==id){if(((g->Flags&GFLG_DISABLED)==0)==(enabled!=0))break;GT_SetGadgetAttrs(g,p->window,NULL,GA_Disabled,!enabled,TAG_DONE);break;}}

struct NDPanel *nd_panel_server_settings(const struct NDPanelField *fields,int reconnect,unsigned int retry_interval)
{struct NDPanel *p;unsigned int index=0;static const char *buttons[]={"Import token","Forget token","Test connection","Save","Close"};
 if(retry_interval==600)index=1;else if(retry_interval==900)index=2;else if(retry_interval==1800)index=3;
 p=panel_open("AmiHASS - Server Settings",fields,4,buttons,5,0,reconnect,1);
 if(p){p->retry_index=index;GT_SetGadgetAttrs(p->retry,p->window,NULL,GTCY_Active,index,TAG_DONE);}return p;}

void nd_panel_set_input(struct NDPanel *p,unsigned int index,const char *text)
{if(p && index<p->count && p->limits[index] && text){if(p->secret_field==(int)index+1){unsigned int n;for(n=0;n<sizeof(p->secret);++n)p->secret[n]=0;strncpy(p->secret,text,sizeof(p->secret)-1);if(!p->secret_shown)text=p->secret[0] ? "Token hidden - Show to edit":"";}GT_SetGadgetAttrs(p->fields[index],p->window,NULL,GTST_String,(ULONG)text,TAG_DONE);}}

void nd_panel_service_state(struct NDPanel *p,int running,int busy)
{nd_panel_enable(p,100,!busy && !running);nd_panel_enable(p,101,!busy && running);}
int nd_panel_enabled(struct NDPanel *p,unsigned int id)
{struct Gadget *g;if(!p)return 0;for(g=p->gadgets;g;g=g->NextGadget)if(g->GadgetID==id)return !(g->Flags&GFLG_DISABLED);return 0;}

int nd_panel_reconnect(struct NDPanel *p){return p && p->reconnect && (p->reconnect->Flags&GFLG_SELECTED)!=0;}
unsigned int nd_panel_retry(struct NDPanel *p){static const unsigned int intervals[]={60,600,900,1800};return p && p->retry_index<4 ? intervals[p->retry_index]:60;}

int nd_panel_toggle(struct NDPanel *p,unsigned int index)
{return p && index<p->count && p->fields[index] && (p->fields[index]->Flags&GFLG_SELECTED)!=0;}

void nd_panel_set_source(struct NDPanel *p,int sample){if(p&&p->source){p->source_index=sample ? 1:0;GT_SetGadgetAttrs(p->source,p->window,NULL,GTCY_Active,p->source_index,TAG_DONE);}}
int nd_panel_source(struct NDPanel *p){return p ? p->source_index:0;}

void nd_panel_choices(struct NDPanel *p,unsigned int field,const char **labels,unsigned int active){if(p&&field<p->count&&(p->choices&(1UL<<field))){p->choice_index[field]=active;GT_SetGadgetAttrs(p->fields[field],p->window,NULL,GTCY_Labels,(ULONG)labels,GTCY_Active,active,TAG_DONE);}}
unsigned int nd_panel_choice(struct NDPanel *p,unsigned int field){return p&&field<p->count ? p->choice_index[field]:0;}

unsigned int nd_panel_level(struct NDPanel *p,unsigned int field)
{return p&&field<p->count&&(p->sliders&(1UL<<field)) ? p->choice_index[field]:0;}
void nd_panel_set_level(struct NDPanel *p,unsigned int field,unsigned int level)
{if(p&&field<p->count&&(p->sliders&(1UL<<field))){p->choice_index[field]=level;GT_SetGadgetAttrs(p->fields[field],p->window,NULL,GTSL_Level,level,TAG_DONE);}}

void nd_panel_show_secret(struct NDPanel *p,int show)
{struct StringInfo *info;unsigned int n;if(!p||!p->secret_field||p->secret_shown==!!show)return;info=(struct StringInfo *)p->fields[p->secret_field-1]->SpecialInfo;if(!info||!info->Buffer)return;
 if(!show){for(n=0;n<sizeof(p->secret);++n)p->secret[n]=0;strncpy(p->secret,info->Buffer,sizeof(p->secret)-1);}
 if(!show){volatile unsigned char *bytes=(volatile unsigned char *)info->Buffer;for(n=0;n<(unsigned int)info->MaxChars;++n)bytes[n]=0;if(info->UndoBuffer){bytes=(volatile unsigned char *)info->UndoBuffer;for(n=0;n<(unsigned int)info->MaxChars;++n)bytes[n]=0;}}
 p->secret_shown=!!show;GT_SetGadgetAttrs(p->fields[p->secret_field-1],p->window,NULL,GTST_String,(ULONG)(show ? p->secret:p->secret[0] ? "Token hidden - Show to edit":""),GA_Disabled,!show,TAG_DONE);
 if(p->secret_button&&p->secret_button->GadgetText)p->secret_button->GadgetText->IText=(UBYTE *)(show ? "Hide":"Show");if(p->secret_button)RefreshGList(p->secret_button,p->window,NULL,1);
}
int nd_panel_secret_shown(struct NDPanel *p){return p&&p->secret_shown;}
