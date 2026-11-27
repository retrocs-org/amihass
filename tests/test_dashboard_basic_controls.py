"""Exercise the production capability-filtered device action panel with fake I/O."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static void basic_controls(');b=s.index('static int media_controls(',a)
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
#include "control_summary.h"
#include "nd_panel.h"
#include "action_progress.h"
#define AMIHASS_OK 1
struct NDPanel {int unused;};static struct NDPanel panel;
struct Dash {struct AHDashboardLayout layout;struct AHControls controls[1];int selected,results[1],in_controls,ready;struct {int pending;} client;struct AHActionProgress progress[1];unsigned long status_due;};
static int events,sends,details,mode;static unsigned int count;
static int read_only_details(struct Dash *d,int check){(void)d;(void)check;++details;return 1;}
static void ui_close(struct Dash *d){(void)d;}static void ui_open(struct Dash *d){(void)d;}static void pause_client(struct Dash *d){(void)d;}static void Delay(int n){(void)n;}
/* Production loops wait on signals; in tests this is one scripted Delay step. */
void nd_panel_wait(struct NDPanel *p,unsigned long e,unsigned long t){(void)p;(void)e;(void)t;Delay(5);}
unsigned long amihass_client_signal(const struct AmiHASSClient *c){(void)c;return 0;}

static void step(struct Dash *d){if(mode==2&&events==0){memset(&d->controls[0],0,sizeof(d->controls[0]));}}
static int send_action(struct Dash *d,const char *action){assert(amihass_control_allowed(&d->controls[0],action));assert(!strcmp(action,mode==1 ? "Lock":"Toggle"));++sends;return 1;}
struct NDPanel *nd_panel_open(const char *t,const struct NDPanelField *f,unsigned int n,const char **b,unsigned int bc){assert(strstr(t,"device controls")&&n==3&&bc==2&&strstr(b[0],"Run"));assert(f[0].value[0]);return &panel;}
void nd_panel_choices(struct NDPanel *p,unsigned int field,const char **labels,unsigned int active){assert(p==&panel&&field==1);count=0;while(labels[count])++count;assert(count<=4);if(mode==1)assert(!strcmp(labels[1],"Lock")&&!strcmp(labels[2],"Unlock"));if(mode==2&&active==0)assert(count==1);}
unsigned int nd_panel_choice(struct NDPanel *p,unsigned int field){(void)p;(void)field;return 1;}
int nd_panel_event(struct NDPanel *p){(void)p;return events++==0 ? 100:101;}
void nd_panel_enable(struct NDPanel *p,unsigned int id,int on){(void)p;(void)id;if(mode==2)assert(!on);}
void nd_panel_set(struct NDPanel *p,unsigned int i,const char *v){(void)p;(void)i;(void)v;}
void nd_panel_close(struct NDPanel *p){(void)p;}
'''+s[a:b]+r'''
int main(void){struct Dash d;for(mode=0;mode<4;++mode){memset(&d,0,sizeof(d));d.ready=1;d.results[0]=AMIHASS_OK;strcpy(d.layout.tiles[0].entity,mode==0 ? "switch.fixture":mode==3 ? "sensor.fixture":"lock.fixture");if(mode==1||mode==2)d.controls[0].lock=d.controls[0].unlock=1;events=sends=details=0;basic_controls(&d);if(mode<2)assert(sends==1&&!details);else if(mode==2)assert(!sends);else assert(details==1&&!sends);}puts("Device controls: supported switch/lock actions, removed capabilities and read-only sensor details; mocked actions only.");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(code);cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),str(root/'src/core.c'),str(root/'src/dashboard_layout.c'),'-o',str(p/'test')];subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
