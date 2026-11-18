"""Drive the actual native control dialog through delayed/unknown readback."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static void control_dialog(');b=s.index('static int save_layout(',a)
source=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
#include "action_progress.h"
#include "nd_panel.h"
#define AMIHASS_OK 1
#define AMIHASS_WAIT 2
struct Dash {struct AHActionProgress progress[24];int in_controls;struct AHDashboardBook book;struct AHDashboardLayout layout;struct {void *pending;} client;int dirty;struct AHControls controls[1];int editing,results[1],selected,ready,action_failed,fresh,force;unsigned long ticks,next,status_due,read_serial[1];struct AmiHASSEntity values[1];char status[160];};
struct NDPanel {int unused;};static struct NDPanel panel;
static int mode,issued,finished,confirmed,timed_out;static unsigned long confirmed_at;
static void ui_close(struct Dash *d){(void)d;}static void pause_client(struct Dash *d){(void)d;}static int ui_open(struct Dash *d){(void)d;return 1;}static void update(struct Dash *d){(void)d;}
static int read_only_details(struct Dash *d,int selftest){(void)d;(void)selftest;return 1;}
static void Delay(int n){(void)n;}
static void step(struct Dash *d){++d->ticks;if(!issued)d->results[0]=AMIHASS_OK;if(issued){d->results[0]=AMIHASS_OK;++d->read_serial[0];d->controls[0].level=mode==0&&d->ticks>=8?50:10;d->controls[0].level_known=mode!=2;}}
static int send_action(struct Dash *d,const char *action){assert(!strcmp(action,"Brightness 50"));issued=1;d->results[0]=AMIHASS_WAIT;return 1;}
static const char *amihass_problem(int n){(void)n;return "Rejected";}
struct NDPanel *nd_panel_open(const char *title,const struct NDPanelField *f,unsigned int count,const char **buttons,unsigned int n){(void)title;(void)f;(void)count;(void)buttons;(void)n;return &panel;}
int nd_panel_event(struct NDPanel *p){(void)p;if(finished)return 101;if(!issued)return 1;return 0;}
void nd_panel_choices(struct NDPanel *p,unsigned int i,const char **labels,unsigned int active){(void)p;(void)i;(void)labels;(void)active;}
unsigned int nd_panel_choice(struct NDPanel *p,unsigned int i){(void)p;(void)i;return 0;}
void nd_panel_enable(struct NDPanel *p,unsigned int id,int enabled){(void)p;(void)id;(void)enabled;}
void nd_panel_set(struct NDPanel *p,unsigned int i,const char *text){(void)p;(void)i;if(strstr(text,"Confirmed state")){confirmed=1;finished=1;}if(strstr(text,"Timed out")){timed_out=1;finished=1;}}
unsigned int nd_panel_level(struct NDPanel *p,unsigned int field){(void)p;(void)field;return 50;}
void nd_panel_set_level(struct NDPanel *p,unsigned int field,unsigned int value){(void)p;(void)field;assert(value==50||value==10);}
const char *nd_panel_get(struct NDPanel *p,unsigned int field){(void)p;(void)field;return "";}
void nd_panel_set_input(struct NDPanel *p,unsigned int field,const char *text){(void)p;(void)field;(void)text;}
void nd_panel_close(struct NDPanel *p){(void)p;}
'''+s[a:b]+r'''
int main(void){struct Dash d;for(mode=0;mode<3;++mode){memset(&d,0,sizeof(d));d.ready=1;d.results[0]=AMIHASS_OK;d.controls[0].brightness=d.controls[0].level_known=1;d.controls[0].level=10;strcpy(d.values[0].state,"on");issued=finished=confirmed=timed_out=0;control_dialog(&d);assert(finished);if(!mode){assert(confirmed&&!timed_out&&d.ticks>=8);}else assert(timed_out&&!confirmed&&d.ticks>=450);assert(!d.fresh);}puts("Confirmation: delayed target readback, stale values, unknown values and bounded timeout passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(source);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),str(root/'src/core.c'),str(root/'src/dashboard_layout.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
