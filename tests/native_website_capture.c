/* QA-only: photograph actual app windows from the guest screen bitmap.
 * Uses isolated sample configuration and real input.device events, never
 * modifies production window rendering or substitutes screenshot contents. */
#include <exec/types.h>
#include <dos/dostags.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>
#include "amihass.h"
#include "amihass_config.h"
#include "dashboard.h"
struct IntuitionBase *IntuitionBase=NULL;
struct GfxBase *GfxBase=NULL;
struct Library *CyberGfxBase=NULL;
long __stack=65536;
/* CyberGraphX-compatible ReadPixelArray ABI, also provided by Picasso96. */
ULONG capture_rgb(__reg("a6") struct Library *,__reg("a0") APTR,
 __reg("d0") UWORD,__reg("d1") UWORD,__reg("d2") UWORD,
 __reg("a1") struct RastPort *,__reg("d3") UWORD,__reg("d4") UWORD,
 __reg("d5") UWORD,__reg("d6") UWORD,__reg("d7") UBYTE)="\tjsr\t-120(a6)";
static struct Screen *screen;
static struct Window *find_window(const char *title)
{struct Window *w;Forbid();for(w=screen->FirstWindow;w;w=w->NextWindow)if(w->Title&&!strcmp((const char *)w->Title,title))break;Permit();return w;}
static struct Window *await_window(const char *title)
{struct Window *w;int i;for(i=0;i<300;++i){w=find_window(title);if(w){WindowToFront(w);ActivateWindow(w);Delay(25);return w;}Delay(5);}printf("FAIL: window did not open: %s\n",title);return NULL;}
static int input_event(struct InputEvent *event)
{struct MsgPort *p;struct IOStdReq *io;int ok=0;p=CreateMsgPort();if(!p)return 0;io=(struct IOStdReq *)CreateIORequest(p,sizeof(*io));if(!io){DeleteMsgPort(p);return 0;}if(!OpenDevice("input.device",0,(struct IORequest *)io,0)){io->io_Command=IND_WRITEEVENT;io->io_Data=event;io->io_Length=sizeof(*event);ok=DoIO((struct IORequest *)io)==0;CloseDevice((struct IORequest *)io);}DeleteIORequest((struct IORequest *)io);DeleteMsgPort(p);return ok;}
static int gadget_click(struct Window *w,unsigned int id)
{struct Gadget *g;struct InputEvent e;struct IEPointerPixel pixel;int x,y;
 for(g=w->FirstGadget;g;g=g->NextGadget)if(g->GadgetID==id)break;if(!g)return 0;
 x=w->LeftEdge+g->LeftEdge+10;y=w->TopEdge+g->TopEdge+g->Height/2;
 WindowToFront(w);ActivateWindow(w);memset(&pixel,0,sizeof(pixel));pixel.iepp_Screen=screen;pixel.iepp_Position.X=x;pixel.iepp_Position.Y=y;
 memset(&e,0,sizeof(e));e.ie_Class=IECLASS_NEWPOINTERPOS;e.ie_SubClass=IESUBCLASS_PIXEL;e.ie_EventAddress=&pixel;if(!input_event(&e))return 0;Delay(5);
 memset(&e,0,sizeof(e));e.ie_Class=IECLASS_RAWMOUSE;e.ie_Code=IECODE_LBUTTON;e.ie_Qualifier=IEQUALIFIER_LEFTBUTTON;if(!input_event(&e))return 0;Delay(5);e.ie_Code|=IECODE_UP_PREFIX;e.ie_Qualifier=0;return input_event(&e);
}
static int escape_window(const char *title)
{struct Window *w;struct InputEvent e;int i;w=find_window(title);if(!w)return 1;WindowToFront(w);ActivateWindow(w);Delay(5);memset(&e,0,sizeof(e));e.ie_Class=IECLASS_RAWKEY;e.ie_Code=0x45;if(!input_event(&e))return 0;e.ie_Code|=IECODE_UP_PREFIX;input_event(&e);for(i=0;i<100;++i){if(!find_window(title))return 1;Delay(5);}return 0;}
static int launch(const char *command)
{BPTR in=Open("NIL:",MODE_OLDFILE),out=Open("NIL:",MODE_NEWFILE);LONG rc;if(!in||!out){if(in)Close(in);if(out)Close(out);return 0;}rc=SystemTags((STRPTR)command,SYS_Input,in,SYS_Output,out,SYS_Asynch,TRUE,NP_StackSize,131072UL,TAG_DONE);if(rc<0){Close(in);Close(out);}return rc>=0;}
static int capture(struct Window *w,const char *name)
{
 struct BitMap *copy=NULL,*scratch=NULL;struct RastPort rp,temp;UBYTE *line=NULL,*rgbline=NULL;ULONG palette[768];unsigned int width,height,depth,colors=0,x,y;FILE *f=NULL;char path[180];int ok=0;
 if(!w)return 0;WindowToFront(w);ActivateWindow(w);Delay(50);width=w->Width;height=w->Height;depth=GetBitMapAttr(screen->RastPort.BitMap,BMA_DEPTH);
 if(!width||!height||depth<1||depth>32||w->LeftEdge<0||w->TopEdge<0||w->LeftEdge+width>screen->Width||w->TopEdge+height>screen->Height)return 0;
 if(depth>8&&!CyberGfxBase)return 0;
 copy=AllocBitMap(width,height,depth,0,screen->RastPort.BitMap);if(!copy)goto done;
 BltBitMap(screen->RastPort.BitMap,w->LeftEdge,w->TopEdge,copy,0,0,width,height,0xc0,0xff,NULL);WaitBlit();
 InitRastPort(&rp);rp.BitMap=copy;temp=rp;temp.Layer=NULL;
 rgbline=AllocVec(width*3UL,MEMF_PUBLIC);if(!rgbline)goto done;
 if(depth<=8){colors=1U<<depth;GetRGB32(screen->ViewPort.ColorMap,0,colors,palette);scratch=AllocBitMap(width,1,depth,0,copy);line=AllocVec((width+15U)&~15U,MEMF_PUBLIC);if(!scratch||!line)goto done;temp.BitMap=scratch;}
 sprintf(path,"AutoRun:screenshots/%s.ppm",name);f=fopen(path,"wb");if(!f)goto done;fprintf(f,"P6\n%u %u\n255\n",width,height);
 for(y=0;y<height;++y){if(depth>8){if(capture_rgb(CyberGfxBase,rgbline,0,0,width*3,&rp,0,y,width,1,0)!=width)goto done;}else{if(ReadPixelLine8(&rp,0,y,width,line,&temp)!=(LONG)width)goto done;for(x=0;x<width;++x){unsigned int pen=line[x];if(pen>=colors)goto done;rgbline[x*3]=palette[pen*3]>>24;rgbline[x*3+1]=palette[pen*3+1]>>24;rgbline[x*3+2]=palette[pen*3+2]>>24;}}
 if(fwrite(rgbline,3,width,f)!=width)goto done;
 }
 ok=1;printf("CAPTURE: %s %ux%u depth %u; actual guest window bitmap.\n",name,width,height,depth);
 done:if(f)fclose(f);if(line)FreeVec(line);if(rgbline)FreeVec(rgbline);if(scratch)FreeBitMap(scratch);if(copy)FreeBitMap(copy);return ok;
}
static int sample_ready(void)
{struct AmiHASSClient c;struct AmiHASSMessage *m;int i,ok=0;memset(&c,0,sizeof(c));for(i=0;i<100;++i){if(!c.pending&&!amihass_send(&c,AMIHASS_STATUS,"Website.Capture",NULL,NULL,0,0,NULL,NULL)){Delay(5);continue;}m=amihass_reply(&c);if(m){ok=m->status.connected&&!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE);FreeVec(m);if(ok)break;}Delay(5);}amihass_client_close(&c);return ok;}
int main(void)
{
 struct AmiHASSConfig config;struct AHDashboardBook *book;struct Window *w;BPTR lock;int ok=0;
 setvbuf(stdout,NULL,_IONBF,0);puts("QA: isolated, credential-free sample-data website captures.");
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",39);GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",39);CyberGfxBase=OpenLibrary("cybergraphics.library",41);if(!IntuitionBase||!GfxBase)goto done;screen=LockPubScreen("Workbench");if(!screen)goto done;
 lock=CreateDir("AutoRun:screenshots");if(lock)UnLock(lock);
 amihass_config_defaults(&config);config.sample_mode=1;config.boot=0;config.token[0]=0;config.connection.url[0]=0;strcpy(config.service_dir,"QATests:");strcpy(config.prefs_dir,"QATests:");if(!amihass_config_save(&config))goto done;amihass_erase(&config,sizeof(config));
 book=AllocVec(sizeof(*book),MEMF_PUBLIC|MEMF_CLEAR);if(!book)goto done;ah_dashboard_book_defaults(book);ah_dashboard_template(&book->pages[0],2);strcpy(book->pages[0].tiles[0].action,"Controls");if(!ah_dashboard_book_save(book,AH_DASH_FILE)){FreeVec(book);goto done;}FreeVec(book);
 if(!launch("QATests:AmiHASSService SERVICE")||!sample_ready())goto done;
 if(!launch("QATests:AmiHASSPrefs"))goto done;w=await_window("AmiHASS Preferences");Delay(150);if(!capture(w,"preferences"))goto done;if(!escape_window("AmiHASS Preferences"))goto done;
 if(!launch("QATests:AmiHASSBrowser"))goto done;w=await_window("AmiHASS Browser 0.1 - Devices");Delay(150);if(!w||!gadget_click(w,6))goto done;w=await_window("AmiHASS Browser 0.1 - Entities");Delay(200);if(!capture(w,"device-browser"))goto done;
 if(!gadget_click(w,7))goto done;w=await_window("AmiHASS - Select room");Delay(100);if(!capture(w,"room-selector"))goto done;if(!escape_window("AmiHASS - Select room")||!escape_window("AmiHASS Browser 0.1 - Entities"))goto done;
 if(!launch("QATests:AmiHASSDashboard"))goto done;w=await_window("AmiHASS Dashboard 0.1");Delay(300);if(!capture(w,"dashboard"))goto done;if(!escape_window("AmiHASS Dashboard 0.1"))goto done;
 if(!launch("QATests:AmiHASSPrefs ABOUT"))goto done;w=await_window("About AmiHASS");if(!capture(w,"about"))goto done;if(!escape_window("About AmiHASS"))goto done;
 ok=1;
 done:if(screen)UnlockPubScreen(NULL,screen);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(GfxBase)CloseLibrary((struct Library *)GfxBase);if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);puts(ok ? "PASS: genuine Preferences, Browser, room selector, Dashboard and About screenshots.":"FAIL: native website capture.");return ok ? 0:10;
}
