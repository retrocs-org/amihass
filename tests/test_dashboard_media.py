"""Run the production media panel against shared sample data with mocked gadgets."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static int media_controls(');b=s.index('static void control_dialog(',a)
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
#include "sample.h"
#include "action_progress.h"
#include "control_draft.h"
#include "nd_panel.h"
struct Dash {struct AHDashboardBook book;struct AHDashboardLayout layout;struct AHControls controls[1];struct AmiHASSEntity values[1];struct AHActionProgress progress[1];struct {int pending;} client;int selected,ready,results[1],in_controls,fresh,force;unsigned long ticks,next,status_due;};
struct NDPanel {int volume;};static struct NDPanel panel;static struct AHSample sample;static int mode,sends,events,details,enabled_buttons[4];
static int read_only_details(struct Dash *d,int check){(void)d;(void)check;++details;return 1;}
static void ui_close(struct Dash *d){(void)d;}static void pause_client(struct Dash *d){(void)d;}static void ui_open(struct Dash *d){(void)d;}static void Delay(int ticks){(void)ticks;}
/* Production loops wait on signals; in tests this is one scripted Delay step. */
void nd_panel_wait(struct NDPanel *p,unsigned long e,unsigned long t){(void)p;(void)e;(void)t;Delay(5);}
unsigned long amihass_client_signal(const struct AmiHASSClient *c){(void)c;return 0;}

static void step(struct Dash *d){++d->ticks;if(mode==2)d->ready=0;}
static int send_action(struct Dash *d,const char *action){struct AmiHASSMessage m;assert(d->ready&&amihass_control_allowed(&d->controls[0],action));memset(&m,0,sizeof(m));m.kind=AMIHASS_ACTION;strcpy(m.owner,"Panel.QA");strcpy(m.entity,"media_player.amihass_sample_living_room");strcpy(m.action,action);ah_sample_request(&sample,&m,d->ticks);assert(m.result==AMIHASS_WAIT);m.kind=AMIHASS_QUERY;ah_sample_request(&sample,&m,d->ticks);d->values[0]=m.item;amihass_controls_items(m.items,m.count,&d->controls[0]);d->progress[0].stage=AH_ACTION_CONFIRMED;++sends;return 1;}
struct NDPanel *nd_panel_open(const char *title,const struct NDPanelField *f,unsigned int n,const char **buttons,unsigned int count){assert(strstr(title,"media controls")&&n==5&&count==5&&!strcmp(buttons[4],"Close"));panel.volume=35;assert(strstr(f[0].value,"Amiga"));return &panel;}
void nd_panel_set(struct NDPanel *p,unsigned int n,const char *s){(void)p;(void)n;(void)s;}
void nd_panel_enable(struct NDPanel *p,unsigned int n,int on){(void)p;if(n>=100&&n<=103)enabled_buttons[n-100]=on;if(mode==2&&n==3)assert(!on);}
int nd_panel_event(struct NDPanel *p){(void)p;++events;return mode==0?0:104;}
unsigned int nd_panel_level(struct NDPanel *p,unsigned int n){assert(n==2);return p->volume;}
void nd_panel_set_level(struct NDPanel *p,unsigned int n,unsigned int level){assert(n==2&&level<=100);p->volume=level;}
void nd_panel_close(struct NDPanel *p){assert(p==&panel);}
'''+s[a:b]+r'''
int main(void){struct Dash d;struct AmiHASSMessage m;for(mode=0;mode<4;++mode){memset(&d,0,sizeof(d));ah_sample_init(&sample);memset(&m,0,sizeof(m));m.kind=AMIHASS_QUERY;strcpy(m.owner,"Panel.QA");strcpy(m.entity,"media_player.amihass_sample_living_room");ah_sample_request(&sample,&m,1);d.values[0]=m.item;amihass_controls_items(m.items,m.count,&d.controls[0]);d.ready=1;d.results[0]=AMIHASS_OK;if(mode==1)d.controls[0].media_next=0;if(mode==3)d.book.read_only=1;sends=events=details=0;assert(media_controls(&d,mode==0));if(mode==0)assert(sends==3&&d.controls[0].volume==37&&!strcmp(d.values[0].state,"playing"));if(mode==1)assert(!sends&&!enabled_buttons[3]);if(mode==2)assert(!sends&&!enabled_buttons[0]);if(mode==3)assert(details==1&&!sends);}puts("Dashboard media: sample Pause/Volume/Play, unsupported buttons, offline blocking and read-only mode passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'test.c').write_text(code);cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),str(root/'src/core.c'),str(root/'src/sample.c'),str(root/'src/browser_data.c'),str(root/'src/dashboard_layout.c'),'-o',str(p/'test')];subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
