"""Drive production room preview, cancellation, source checks and confirmation."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();s=s[s.index('static int room_layout_dialog('):s.index('static int template_dialog(')]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dashboard_extras.h"
#include "browser_live.h"
#include "nd_panel.h"
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
struct Dash {struct AHDashboardLayout layout;int selected,dirty;unsigned int row_offset;};struct NDPanel {int n;};static struct NDPanel panel;static int events[12],at,alive,mode,confirmed,requests,steps,source_checks;static unsigned int category,page;
static void *AllocVec(unsigned long n,int f){(void)f;return calloc(1,n);}static void FreeVec(void *p){free(p);}
static int ah_catalogue_choose(int kind,const char *old,char id[96],char name[64],int test){(void)old;(void)test;assert(kind==AMIHASS_AREAS&&!alive);strcpy(id,"living_room");strcpy(name,"Living room");return 1;}
static int backup_confirm(const char *a,const char *b,const char *c){(void)a;(void)b;(void)c;assert(!alive);++confirmed;return mode!=3;}
static int room_source_current(const char *s){assert(!alive&&!strcmp(s,"sample://amihass"));++source_checks;return mode!=4;}
void ah_browser_live_init(struct AHBrowserLive *p){memset(p,0,sizeof(*p));}
void ah_browser_live_close(struct AHBrowserLive *p){p->loading=p->wanted=0;}
int ah_browser_live_page(struct AHBrowserLive *p,const char *domain,const char *filter,int offset,int refresh){assert(!strcmp(p->area,"living_room")&&p->page_size==24&&!filter[0]&&!offset&&refresh);assert(!domain[0]);p->loading=p->wanted=1;++requests;steps=0;return 1;}
void ah_browser_live_step(struct AHBrowserLive *p){unsigned int i;++steps;if(mode==8)return;p->loading=p->wanted=0;p->connected=p->running=1;strcpy(p->server,"sample://amihass");if(mode==6){strcpy(p->error,"Fixture discovery failure");return;}p->count=mode==5 ? 0:mode==9 ? 24:3;p->total=mode==9 ? 40:p->count;for(i=0;i<(unsigned)p->count;++i){sprintf(p->items[i].id,"sensor.room_%u",i);sprintf(p->items[i].name,"Sensor %u",i);}if(mode!=9&&p->count)strcpy(p->items[2].id,p->items[0].id);}
unsigned long amihass_client_signal(const struct AmiHASSClient *p){(void)p;return 0;}
struct NDPanel *nd_panel_open(const char *t,const struct NDPanelField *f,unsigned int n,const char **buttons,unsigned int c){(void)t;(void)buttons;assert(!alive&&n==8&&c==4&&f[2].limit==ND_PANEL_CHOICE);alive=1;return &panel;}
void nd_panel_close(struct NDPanel *p){assert(p==&panel&&alive);alive=0;}
void nd_panel_choices(struct NDPanel *p,unsigned int field,const char **labels,unsigned int active){assert(p==&panel&&labels[0]);if(!field)category=active;else {assert(field==2);page=active;}}
unsigned int nd_panel_choice(struct NDPanel *p,unsigned int field){assert(p==&panel);return field==0 ? category:page;}
void nd_panel_enable(struct NDPanel *p,unsigned int id,int e){(void)p;(void)e;assert(id==101);}
void nd_panel_set(struct NDPanel *p,unsigned int field,const char *text){(void)p;(void)text;assert(field>=1&&field<8);}
void nd_panel_wait(struct NDPanel *p,unsigned long e,unsigned long t){(void)p;(void)e;(void)t;}
int nd_panel_event(struct NDPanel *p){int e;assert(p==&panel&&alive&&at<12);e=events[at++];if(e==1)category=1;if(e==3)page=1;return e;}
'''+s+r'''
int main(void){struct Dash d;struct AHDashboardLayout before;int i;for(mode=0;mode<10;++mode){memset(&d,0,sizeof(d));ah_dashboard_defaults(&d.layout);strcpy(d.layout.name,"Original");before=d.layout;memset(events,0,sizeof(events));at=alive=confirmed=requests=steps=source_checks=0;if(mode==0)events[0]=102;else if(mode==1){events[0]=101;events[1]=102;}else if(mode==7){events[0]=100;events[1]=1;events[2]=101;events[3]=102;}else if(mode==8){events[0]=100;events[1]=102;}else if(mode==9){events[0]=100;events[1]=3;events[2]=101;}else{events[0]=100;events[1]=101;events[2]=102;}room_layout_dialog(&d,0);assert(!alive);if(mode==2||mode==9){assert(d.dirty&&confirmed==1&&source_checks==1&&!strcmp(d.layout.name,"Living room"));for(i=0;i<24;++i)assert(!d.layout.tiles[i].action[0]);if(mode==9)assert(d.layout.columns==4&&d.layout.rows==6);}else assert(!d.dirty&&!memcmp(&before,&d.layout,sizeof(before)));if(mode==0||mode==1)assert(!requests&&!confirmed);if(mode==4)assert(source_checks==1&&confirmed==1);if(mode==5||mode==6||mode==7||mode==8)assert(!confirmed);}puts("Room Dashboard UI: preview required, scoped reads, bounded/paged preview, confirmation, cancellation, empty/error results, changed filters and changed source preserve the current layout passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG; typedef int32_t LONG; struct MsgPort; struct Message {unsigned short mn_Length; struct MsgPort *mn_ReplyPort;};\n');(p/'test.c').write_text(code)
 cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-DAH_HOST_TEST','-include','ah_locale.h','-I'+str(p),'-I'+str(root/'include'),'-I'+str(root/'SDK'),str(p/'test.c'),str(root/'src/core.c'),str(root/'src/dashboard_layout.c'),str(root/'src/ah_locale.c'),'-o',str(p/'test')]
 if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
