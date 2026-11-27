"""Run production local dependency checks with filesystem/library fixtures; no network."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/ui.c').read_text();a=s.index('static int dependency_path(');b=s.index('static int health_panel(',a)
code=r''' 
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "nd_panel.h"
typedef int BPTR;
typedef char *STRPTR;
#define ACCESS_READ 0
struct Library {int unused;};struct DateStamp {long ds_Days;};struct NDPanel {int unused;};
static struct Library library;static struct NDPanel panel;static int mode,checks,closes,events;
static BPTR Lock(STRPTR path,int access){(void)access;if(!strcmp(path,"C:curl"))return mode!=0;if(!strcmp(path,"AmiSSL:Certs"))return mode==1;if(!strcmp(path,"fixture-ca"))return mode==2;return 0;}
static void UnLock(BPTR lock){assert(lock);}
static struct Library *OpenLibrary(const char *name,int version){assert(!strcmp(name,"amisslmaster.library")&&!version);return mode==1 ? &library:NULL;}
static void CloseLibrary(struct Library *lib){assert(lib==&library);++closes;}
static int GetVar(const char *name,char *out,int size,int flags){(void)size;(void)flags;if(mode==2&&!strcmp(name,"CURL_CA_BUNDLE")){strcpy(out,"fixture-ca");return 10;}return -1;}
static void DateStamp(struct DateStamp *stamp){stamp->ds_Days=mode==0 ? 0:18000;}
static int health_panel(int selftest){assert(selftest);return 1;}
static void Delay(int ticks){(void)ticks;}
/* Production loops wait on signals; in tests this is one scripted Delay step. */
void nd_panel_wait(struct NDPanel *p,unsigned long e,unsigned long t){(void)p;(void)e;(void)t;Delay(5);}

struct NDPanel *nd_panel_open(const char *title,const struct NDPanelField *f,unsigned int n,const char **buttons,unsigned int count){assert(strstr(title,"dependencies")&&n==6&&count==2&&!strcmp(buttons[0],"Check again"));++checks;if(!mode){assert(strstr(f[0].value,"Missing")&&strstr(f[1].value,"unavailable")&&strstr(f[2].value,"No CA")&&strstr(f[3].value,"old"));}else if(mode==1){assert(strstr(f[0].value,"found")&&strstr(f[1].value,"opens")&&strstr(f[2].value,"AmiSSL:Certs found"));}else assert(strstr(f[2].value,"Configured CA path found"));assert(strstr(f[5].value,"Availability only"));return &panel;}
int nd_panel_event(struct NDPanel *p){assert(p==&panel);return events++==0 ? 100:101;}
void nd_panel_close(struct NDPanel *p){assert(p==&panel);}
'''+s[a:b]+s[s.index('static void dependencies_ui(void)'):s.index('void amihass_recovery_notice(')]+r'''
int main(void){for(mode=0;mode<3;++mode){events=checks=closes=0;dependencies_ui();assert(checks==2&&closes==(mode==1 ? 2:0));}puts("Dependencies: missing/present curl, loadable AmiSSL, CA path/directory, clock and recheck; no network.");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
