/* Alarm code keypad; see keypad.h. Number keys on the keyboard work too. */
#define IntuitionBase AHPadIntuitionBase
#define GadToolsBase AHPadGadToolsBase
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <string.h>
#include "keypad.h"
#include "ah_locale.h"
struct IntuitionBase *IntuitionBase=NULL;
struct Library *GadToolsBase=NULL;
int ah_keypad(const char *title,char *code,unsigned int cap)
{
 static const char *keys[12]={"1","2","3","4","5","6","7","8","9","Clear","0","OK"};
 struct Screen *screen=NULL;struct Window *w=NULL;struct Gadget *list=NULL,*last,*shown=NULL;APTR visual=NULL;struct NewGadget ng;struct IntuiMessage *m;struct TextAttr font={(STRPTR)"topaz.font",8,0,0};
 char stars[20];unsigned int n=0,i;int result=0,done=0,top;ULONG cls;UWORD code_id,key;
 if(!code||cap<2)return 0;memset(code,0,cap);if(cap>17)cap=17;memset(stars,0,sizeof(stars));
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",39);GadToolsBase=OpenLibrary("gadtools.library",39);if(!IntuitionBase||!GadToolsBase)goto done;
 screen=LockPubScreen(NULL);if(!screen)goto done;visual=GetVisualInfo(screen,TAG_DONE);if(!visual)goto done;
 top=screen->WBorTop+screen->Font->ta_YSize+6;last=CreateContext(&list);if(!last)goto done;
 memset(&ng,0,sizeof(ng));ng.ng_VisualInfo=visual;ng.ng_TextAttr=&font;
 ng.ng_LeftEdge=12;ng.ng_TopEdge=top;ng.ng_Width=180;ng.ng_Height=16;ng.ng_GadgetID=1;
 shown=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)"",GTTX_Border,TRUE,TAG_DONE);if(!last)goto done;
 for(i=0;i<12;++i){ng.ng_LeftEdge=12+(int)(i%3)*62;ng.ng_TopEdge=top+22+(int)(i/3)*24;ng.ng_Width=56;ng.ng_Height=20;ng.ng_GadgetID=100+i;ng.ng_GadgetText=(STRPTR)ah_tx(keys[i]);ng.ng_Flags=PLACETEXT_IN;last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);if(!last)goto done;}
 w=OpenWindowTags(NULL,WA_Title,(ULONG)ah_tx(title),WA_PubScreen,(ULONG)screen,WA_Left,(screen->Width-204)/2,WA_Top,(screen->Height-(top+130))/2,WA_Width,204,WA_Height,top+128,
  WA_Flags,WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE|WFLG_SMART_REFRESH,WA_IDCMP,BUTTONIDCMP|IDCMP_CLOSEWINDOW|IDCMP_VANILLAKEY,WA_Gadgets,(ULONG)list,TAG_DONE);
 if(!w)goto done;GT_RefreshWindow(w,NULL);
 while(!done){WaitPort(w->UserPort);
  while((m=GT_GetIMsg(w->UserPort))!=NULL){cls=m->Class;key=m->Code;code_id=cls==IDCMP_GADGETUP&&m->IAddress?((struct Gadget *)m->IAddress)->GadgetID:0;GT_ReplyIMsg(m);
   if(cls==IDCMP_VANILLAKEY){if(key>='0'&&key<='9')code_id=key=='0'?110:(UWORD)(99+(key-'0'));else if(key==13)code_id=111;else if(key==8)code_id=109;else if(key==27){done=1;break;}}
   if(cls==IDCMP_CLOSEWINDOW){done=1;break;}
   if(code_id==111){result=n>0;done=1;break;}
   if(code_id==109){memset(code,0,cap);n=0;}
   else if(code_id>=100&&code_id<=110&&n+1<cap){code[n++]=(char)(code_id==110?'0':'1'+(code_id-100));}
   for(i=0;i<n&&i<sizeof(stars)-1;++i)stars[i]='*';stars[n<sizeof(stars)-1?n:sizeof(stars)-1]=0;GT_SetGadgetAttrs(shown,w,NULL,GTTX_Text,(ULONG)stars,TAG_DONE);}}
done:
 if(!result){memset(code,0,cap);}
 if(w)CloseWindow(w);if(list)FreeGadgets(list);if(visual)FreeVisualInfo(visual);if(screen)UnlockPubScreen(NULL,screen);
 if(GadToolsBase)CloseLibrary(GadToolsBase);GadToolsBase=NULL;if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);IntuitionBase=NULL;return result;
}
