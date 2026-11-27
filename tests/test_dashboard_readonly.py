"""Exercise the production action boundary, not just disabled UI widgets."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static int send_action(');b=s.index('static void step(',a)
code=r'''
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "dashboard.h"
#include "action_progress.h"
#define AMIHASS_ACTION 3
#define AMIHASS_OK 1
#define AMIHASS_WAIT 2
#ifndef AMIHASS_BUSY
#define AMIHASS_BUSY 6
#endif
#define AMIHASS_STATE_SELECTION_FULL "selection_full"
struct AmiHASSClient {void *pending;};
static unsigned int page_poll;
#define TILE_REFRESH(d,i) ((d)->layout.tiles[(i)].refresh ? (d)->layout.tiles[(i)].refresh:page_poll)
static int ah_keypad(const char *t,char *c,unsigned int n){(void)t;(void)c;(void)n;return 0;}
void amihass_erase(void *p,unsigned long n){memset(p,0,n);}
struct Dash {time_t received[24];int inactive;char status[160];struct AHActionProgress progress[24];int in_controls;struct AHDashboardBook book;struct AHDashboardLayout layout;struct AmiHASSClient client;struct AmiHASSEntity values[1];struct AHControls controls[1];unsigned long ticks,read_serial[24];int ready,selected,results[1],sample,action_failed,sent,index;};
static int checks,recheck_ok=1;static int revalidate_device_action(struct Dash *d,const char *action){(void)d;(void)action;++checks;return recheck_ok;}
static int confirmations,consent;static int confirm_device_action(struct Dash *d,const char *action){(void)d;(void)action;++confirmations;return consent;}
static int sends;static void update(struct Dash *d){(void)d;}
static int amihass_send(struct AmiHASSClient *c,int kind,const char *owner,const char *id,const char *action,int offset,int refresh,const char *domain,const char *filter){(void)c;(void)owner;(void)id;(void)action;(void)offset;(void)refresh;(void)domain;(void)filter;assert(kind==AMIHASS_ACTION);++sends;return 1;}
'''+s[a:b]+r'''
int main(void){struct Dash d;int i;const char *actions[]={"Toggle","Turn on","Turn off","Brightness 42","Color #123456","Temperature 22.5"};memset(&d,0,sizeof(d));d.ready=d.sample=1;strcpy(d.layout.tiles[0].entity,"light.amihass_sample_living_room");strcpy(d.values[0].state,"on");d.controls[0].brightness=d.controls[0].color=1;d.book.read_only=1;for(i=0;i<6;++i){d.results[0]=AMIHASS_OK;if(i==5){strcpy(d.layout.tiles[0].entity,"climate.amihass_sample_hallway");d.controls[0].temperature=1;d.controls[0].minimum=700;d.controls[0].maximum=3500;d.controls[0].step=50;strcpy(d.controls[0].unit,"C");}strcpy(d.values[0].id,d.layout.tiles[0].entity);assert(!send_action(&d,actions[i]));}assert(!sends);strcpy(d.layout.tiles[0].entity,"light.amihass_sample_living_room");strcpy(d.values[0].id,d.layout.tiles[0].entity);d.book.read_only=0;assert(send_action(&d,"Brightness 42")&&sends==1);d.book.read_only=1;d.results[0]=AMIHASS_OK;assert(!send_action(&d,"Toggle")&&sends==1);d.book.read_only=0;d.sample=0;d.results[0]=AMIHASS_OK;memset(d.progress,0,sizeof(d.progress));strcpy(d.layout.tiles[0].entity,"lock.test");strcpy(d.values[0].id,"lock.test");strcpy(d.values[0].state,"locked");d.controls[0].unlock=1;assert(!send_action(&d,"Unlock")&&sends==1&&confirmations==1);consent=1;recheck_ok=0;assert(!send_action(&d,"Unlock")&&sends==1&&checks==1&&strstr(d.status,"cancelled"));recheck_ok=1;assert(send_action(&d,"Unlock")&&sends==2&&confirmations==3&&checks==2);d.results[0]=AMIHASS_OK;memset(d.progress,0,sizeof(d.progress));d.sample=1;strcpy(d.layout.tiles[0].entity,"lock.amihass_sample_test");strcpy(d.values[0].id,d.layout.tiles[0].entity);assert(send_action(&d,"Unlock")&&sends==3&&confirmations==3&&checks==2);puts("Read-only Dashboard: all action paths blocked at send boundary; explicit editable mode restores supported controls.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),str(root/'src/core.c'),str(root/'src/dashboard_layout.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
