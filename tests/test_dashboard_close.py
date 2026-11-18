"""Exercise real Dashboard Save/Discard/Keep Editing and save-failure paths."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static int save_layout(');b=s.index('static int choice_check(',a)
source=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
#include "nd_panel.h"
typedef long BPTR;
struct Dash {struct AHDashboardLayout layout;struct AHDashboardBook book;int dirty,undo_dirty;unsigned long status_due;char status[160];};struct NDPanel {int unused;};static struct NDPanel panel;
static int events[3],next,saves,fail_first,opens,prompts,errors,panel_fail,closes;
static BPTR CreateDir(const char *path){assert(!strcmp(path,"ENVARC:AmiHASS"));return 1;}static void UnLock(BPTR lock){assert(lock==1);}
int ah_dashboard_book_save(const struct AHDashboardBook *layout,const char *path){(void)layout;assert(!strcmp(path,AH_DASH_FILE));++saves;return !(fail_first&&saves==1);}
static void update(struct Dash *d){(void)d;}static void ui_close(struct Dash *d){(void)d;++closes;}static void pause_client(struct Dash *d){(void)d;}static int ui_open(struct Dash *d){(void)d;++opens;return 1;}static void Delay(int n){(void)n;}
struct NDPanel *nd_panel_open(const char *t,const struct NDPanelField *f,unsigned int count,const char **buttons,unsigned int n){(void)t;(void)f;assert(count==1&&n==3&&!strcmp(buttons[0],"Save")&&!strcmp(buttons[1],"Discard")&&!strcmp(buttons[2],"Keep Editing"));++prompts;return panel_fail?NULL:&panel;}
int nd_panel_event(struct NDPanel *p){(void)p;assert(next<3);return events[next++];}void nd_panel_close(struct NDPanel *p){(void)p;}void nd_panel_title(struct NDPanel *p,const char *t){(void)p;assert(strstr(t,"Could not save"));++errors;}
'''+s[a:b]+r'''
int main(void){struct Dash d;int mode,result;for(mode=0;mode<8;++mode){memset(&d,0,sizeof(d));d.dirty=mode!=0;next=saves=opens=prompts=errors=closes=0;panel_fail=mode==7;fail_first=mode>=5;events[0]=mode==2 ? 101:mode==3 ? 102:mode==4 ? -1:100;events[1]=mode==5 ? 102:100;events[2]=102;result=confirm_close(&d);if(mode==0)assert(result&&!prompts&&!saves);if(mode==1)assert(result&&!d.dirty&&saves==1);if(mode==2)assert(result&&d.dirty&&!saves);if(mode==3||mode==4)assert(!result&&d.dirty&&!saves&&opens==0);if(mode==5)assert(!result&&d.dirty&&saves==1&&errors==1&&opens==0);if(mode==6)assert(result&&!d.dirty&&saves==2&&errors==1);if(mode==7)assert(!result&&d.dirty&&!saves);assert(!closes);}puts("Dashboard close: clean, Save, Discard, Keep Editing, Escape, failed save and retry passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(source);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
