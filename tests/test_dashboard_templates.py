"""Drive production template preview and replacement confirmation."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static int template_dialog(');b=s.index('static void edit_layout(',a)
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
#include "nd_panel.h"
struct Dash {struct AHDashboardLayout layout;int sample,dirty,selected;unsigned int row_offset;};struct NDPanel {int unused;};static struct NDPanel panel;static int events[8],at,alive,confirm,confirmed,mode,preview_lines;static unsigned int choice;
struct NDPanel *nd_panel_open(const char *title,const struct NDPanelField *f,unsigned int count,const char **buttons,unsigned int n){(void)title;(void)f;assert(!alive&&count==8&&n==3&&!strcmp(buttons[0],"Preview"));alive=1;return &panel;}
void nd_panel_close(struct NDPanel *p){assert(p==&panel&&alive);alive=0;}
void nd_panel_choices(struct NDPanel *p,unsigned int field,const char **labels,unsigned int active){(void)p;(void)labels;assert(field==0);choice=active;}
unsigned int nd_panel_choice(struct NDPanel *p,unsigned int field){(void)p;assert(field==0);return choice;}
void nd_panel_enable(struct NDPanel *p,unsigned int id,int enable){(void)p;assert(id==101);(void)enable;}
void nd_panel_set(struct NDPanel *p,unsigned int field,const char *text){(void)p;(void)text;assert(field>=2&&field<8);++preview_lines;}
int nd_panel_event(struct NDPanel *p){int event;(void)p;assert(at<8);event=events[at++];if(event==100&&mode==5)choice=1;if(event==1)choice=1;return event;}
static int backup_confirm(const char *title,const char *message,const char *button){(void)title;(void)message;assert(!alive&&!strcmp(button,"Replace"));++confirmed;return confirm;}
static void Delay(int n){(void)n;}
'''+s[a:b]+r'''
int main(void){struct Dash d;struct AHDashboardLayout before;for(mode=0;mode<6;++mode){memset(&d,0,sizeof(d));ah_dashboard_sample(&d.layout);before=d.layout;d.sample=mode!=3;at=alive=confirmed=preview_lines=0;confirm=mode!=2;if(mode==0){events[0]=101;events[1]=102;}else if(mode==4){events[0]=100;events[1]=1;events[2]=101;events[3]=102;}else{events[0]=100;events[1]=101;events[2]=102;}template_dialog(&d,0);assert(!alive);if(mode==1){assert(d.dirty&&confirmed==1&&!strcmp(d.layout.name,"Lighting")&&preview_lines==6);}else if(mode==5){assert(d.dirty&&confirmed==1&&!strcmp(d.layout.name,"Climate")&&d.layout.tiles[0].refresh==300);}else{assert(!d.dirty&&!memcmp(&d.layout,&before,sizeof(before)));if(mode!=2)assert(!confirmed);}}puts("Templates UI: mandatory preview, replacement confirmation, cancellation, changed selection and server-data preview-only mode passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),'-I'+str(root/'SDK'),str(p/'test.c'),str(root/'src/dashboard_layout.c'),str(root/'src/core.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
