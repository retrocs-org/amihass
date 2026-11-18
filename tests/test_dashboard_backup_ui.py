"""Drive the actual backup dialogs; confirmations never nest native panels."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static int backup_confirm(');b=s.index('static int tile_tools(',a)
code=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dashboard.h"
#include "nd_panel.h"
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
struct Dash {struct AHDashboardBook book;struct AHDashboardLayout layout;int selected,dirty;unsigned int row_offset;};
struct NDPanel {int value;};static struct NDPanel panel;
static int events[6],at,alive,opens;static const char *path;
static void *AllocVec(unsigned long n,int flags){(void)flags;return calloc(1,n);}static void FreeVec(void *p){free(p);}static void Delay(int n){(void)n;}
struct NDPanel *nd_panel_open(const char *title,const struct NDPanelField *f,unsigned int n,const char **buttons,unsigned int count){(void)title;(void)f;(void)n;(void)buttons;(void)count;assert(!alive);alive=1;++opens;return &panel;}
void nd_panel_close(struct NDPanel *p){assert(p==&panel&&alive);alive=0;}
int nd_panel_event(struct NDPanel *p){assert(p==&panel&&alive&&at<6);return events[at++];}
const char *nd_panel_get(struct NDPanel *p,unsigned int field){assert(p==&panel&&alive&&field==0);return path;}
'''+s[a:b]+r'''
int main(void){struct Dash d;struct AHDashboardBook loaded;FILE *f;int mode;for(mode=0;mode<6;++mode){memset(&d,0,sizeof(d));ah_dashboard_book_defaults(&d.book);strcpy(d.book.pages[0].name,"Before");d.layout=d.book.pages[0];at=alive=opens=0;path=mode==3 ? "bad.ahdash":"backup.ahdash";if(mode==0){events[0]=100;events[1]=102;}else if(mode==1){events[0]=101;events[1]=100;events[2]=102;}else if(mode==2){events[0]=101;events[1]=101;events[2]=102;}else if(mode==3){f=fopen(path,"wb");fputs("bad",f);fclose(f);events[0]=101;events[1]=102;}else{strcpy(d.layout.name,"Changed");events[0]=100;events[1]=mode==4 ? 101:100;events[2]=102;}backup_dialog(&d);assert(!alive);if(mode==0)assert(!d.dirty&&ah_dashboard_import(&loaded,path)&&!strcmp(loaded.pages[0].name,"Before"));if(mode==1)assert(d.dirty&&!strcmp(d.layout.name,"Before"));if(mode==2||mode==3)assert(!d.dirty&&!strcmp(d.layout.name,"Before"));if(mode==4)assert(!d.dirty&&ah_dashboard_import(&loaded,path)&&!strcmp(loaded.pages[0].name,"Before"));if(mode==5)assert(!d.dirty&&ah_dashboard_import(&loaded,path)&&!strcmp(loaded.pages[0].name,"Changed"));}puts("Backup UI: export/import, invalid input, replacement confirmation, cancellation and sequential native panels passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),'-I'+str(root/'SDK'),str(p/'test.c'),str(root/'src/dashboard_layout.c'),str(root/'src/core.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],cwd=p,check=True)
