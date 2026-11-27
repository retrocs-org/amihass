"""Run the real panel close path with nested windows and masked token buffers."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'src/panel.c').read_text();a=s.index('void nd_panel_close(');b=s.index('static void panel_logo(',a)
code=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct StringInfo{char *Buffer,*UndoBuffer;unsigned int MaxChars;};struct Gadget{void *SpecialInfo;};
struct NDPanel{void *window,*visual,*screen;struct Gadget *gadgets,*fields[12];unsigned int count,limits[12];char secret[1025];};
static unsigned int panel_users;static void *GadToolsBase,*GfxBase,*IntuitionBase;static int closed,freed;
struct Library{int unused;};
static void CloseWindow(void *p){assert(p);}static void FreeGadgets(void *p){assert(p);}static void FreeVisualInfo(void *p){assert(p);}static void UnlockPubScreen(void *n,void *p){(void)n;assert(p);}
static void FreeVec(struct NDPanel *p){unsigned int i;for(i=0;i<sizeof(p->secret);++i)assert(!p->secret[i]);++freed;free(p);}
static void CloseLibrary(void *p){assert(p);++closed;}
'''+s[a:b]+r'''
int main(void){struct NDPanel *parent=calloc(1,sizeof(*parent)),*child=calloc(1,sizeof(*child));struct StringInfo info;struct Gadget gadget;char input[40],undo[40];assert(parent&&child);panel_users=2;GadToolsBase=GfxBase=IntuitionBase=(void*)1;memset(parent->secret,'p',sizeof(parent->secret));memset(child->secret,'c',sizeof(child->secret));memset(input,'x',sizeof(input));memset(undo,'y',sizeof(undo));info.Buffer=input;info.UndoBuffer=undo;info.MaxChars=40;gadget.SpecialInfo=&info;child->gadgets=&gadget;child->fields[0]=&gadget;child->limits[0]=40;child->count=1;nd_panel_close(child);assert(panel_users==1&&freed==1&&!closed&&GadToolsBase&&IntuitionBase);for(unsigned int i=0;i<40;++i)assert(!input[i]&&!undo[i]);nd_panel_close(NULL);assert(panel_users==1&&!closed);nd_panel_close(parent);assert(!panel_users&&freed==2&&closed==3&&!GadToolsBase&&!GfxBase&&!IntuitionBase);puts("Panels: closing nested Profiles retains parent libraries; final close releases libraries and wipes token/undo buffers.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);cmd=[os.environ.get('CC','cc'),'-std=c99',str(p/'test.c'),'-o',str(p/'test')]
 if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
