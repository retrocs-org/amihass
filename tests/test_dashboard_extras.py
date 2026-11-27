"""Room layouts, safe refresh policy, distinct availability and credential-free sharing."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "dashboard_extras.h"
int main(void){struct AHDashboardLayout l;struct AmiHASSEntity items[30];struct AHDashboardBook b,c;FILE *f;char data[20000];unsigned int i,n;memset(items,0,sizeof(items));for(i=0;i<30;++i){sprintf(items[i].id,"sensor.room_%u",i);sprintf(items[i].name,"Room sensor %u",i);}assert(ah_dashboard_room_layout(&l,"Living room",items,30)==24&&l.columns==4&&l.rows==6);for(i=0;i<24;++i)assert(!l.tiles[i].action[0]);strcpy(items[1].id,items[0].id);strcpy(items[2].id,"invalid entity");strcpy(items[0].name,"Clean\nlabel");assert(ah_dashboard_room_layout(&l,"Room\nname",items,3)==1&&!strcmp(l.tiles[0].label,"Clean label")&&!strcmp(l.name,"Room name"));assert(!ah_dashboard_room_layout(&l,"Empty",items,0)&&l.rows==1);
 assert(!strcmp(ah_dashboard_availability(1,0,1,1,0,30),"Offline"));assert(!strcmp(ah_dashboard_availability(8,1,1,1,0,30),"Unavailable"));assert(!strcmp(ah_dashboard_availability(1,1,1,0,0,30),"Unavailable"));assert(!strcmp(ah_dashboard_availability(3,1,1,1,2,30),"Cached"));assert(!strcmp(ah_dashboard_availability(1,1,1,1,500,30),"Cached"));assert(!strcmp(ah_dashboard_availability(1,1,1,1,1,30),"Available"));assert(!strcmp(ah_dashboard_availability(2,1,0,1,0,30),"Updating"));
 assert(!ah_camera_policy_load("policy"));assert(!ah_camera_policy_save("policy",5));assert(ah_camera_policy_save("policy",60)&&ah_camera_policy_load("policy")==60);mkdir("policy.new",0700);assert(!ah_camera_policy_save("policy",300)&&ah_camera_policy_load("policy")==60);rmdir("policy.new");assert(ah_camera_policy_save("policy",0)&&!ah_camera_policy_load("policy"));f=fopen("policy","wb");fputs("AMIHASSCAMERA1\n60 garbage\n",f);fclose(f);assert(!ah_camera_policy_load("policy"));
 assert(!ah_camera_auto_due(0,1000,0,1,1,1,0));assert(!ah_camera_auto_due(5,1000,0,1,1,1,0));assert(ah_camera_auto_due(60,1000,0,1,1,1,0));assert(!ah_camera_auto_due(60,1000,990,1,1,1,0));assert(!ah_camera_auto_due(60,1000,0,0,1,1,0));assert(!ah_camera_auto_due(60,1000,0,1,0,1,0));assert(!ah_camera_auto_due(60,1000,0,1,1,0,0));assert(!ah_camera_auto_due(60,1000,0,1,1,1,1));assert(!ah_camera_auto_due(60,1000,1100,1,1,1,0));assert(ah_camera_auto_due(60,1060,1000,1,1,1,0));
 ah_dashboard_book_defaults(&b);ah_dashboard_sample(&b.pages[0]);assert(ah_dashboard_export(&b,"share.ahdash")&&ah_dashboard_import(&c,"share.ahdash")&&!memcmp(&b,&c,sizeof(b)));f=fopen("share.ahdash","rb");n=fread(data,1,sizeof(data)-1,f);data[n]=0;fclose(f);assert(!strstr(data,"Bearer")&&!strstr(data,"token")&&!strstr(data,"https://"));puts("Dashboard extras: bounded read-only room layouts, unique IDs, clean names, availability distinctions, manual-default camera policy, failure-safe saves, refresh gating and credential-free sharing passed.");return 0;}
'''
source=(root/'src/dashboard.c').read_text();scheduler=source[source.index('static void camera_auto_step('):source.index('static int backup_confirm(')]
stubs=r'''
#include <time.h>
#define AMIHASS_UNAVAILABLE 8
static time_t fixture_time=1000;
#define time(p) fixture_time
struct Dash {void *thumbs;struct AHDashboardLayout layout;struct AmiHASSEntity values[24];time_t received[24];int results[24],ready,sample,inactive,editing,in_controls,first;unsigned int cells;unsigned long camera_attempt[24];};
static unsigned int camera_interval;static int busy,requested,accept=1;
static int cell_index(struct Dash *d,unsigned int cell){return d->first+(int)cell;}
static int ah_thumbs_busy(void *p){(void)p;return busy;}
static int ah_thumbs_request(void *p,int tile,const char *id){(void)p;assert(tile>=0&&tile<3&&strstr(id,"amihass_sample"));++requested;if(accept)busy=1;return accept;}
'''
checks=r'''
{struct Dash d;memset(&d,0,sizeof(d));d.thumbs=&d;d.ready=d.sample=1;d.cells=2;strcpy(d.layout.tiles[0].entity,"camera.amihass_sample_a");strcpy(d.layout.tiles[1].entity,"camera.amihass_sample_b");strcpy(d.layout.tiles[2].entity,"camera.amihass_sample_hidden");camera_auto_step(&d);assert(!requested);camera_interval=60;camera_auto_step(&d);assert(requested==1&&d.camera_attempt[0]==1000&&!d.camera_attempt[1]&&!d.camera_attempt[2]);camera_auto_step(&d);assert(requested==1);busy=0;camera_auto_step(&d);assert(requested==2&&d.camera_attempt[1]==1000&&!d.camera_attempt[2]);busy=0;camera_auto_step(&d);assert(requested==2);fixture_time=1060;d.inactive=1;camera_auto_step(&d);d.inactive=0;d.editing=1;camera_auto_step(&d);d.editing=0;d.ready=0;camera_auto_step(&d);d.ready=1;d.sample=0;camera_auto_step(&d);assert(requested==2);d.sample=1;d.cells=1;d.results[0]=8;camera_auto_step(&d);assert(requested==2);d.results[0]=1;d.received[0]=10;strcpy(d.values[0].state,"unavailable");camera_auto_step(&d);assert(requested==2);d.received[0]=0;accept=0;camera_auto_step(&d);assert(requested==3&&d.camera_attempt[0]==1060);camera_auto_step(&d);assert(requested==3);}
'''
code=code.replace('int main(void){',stubs+scheduler+'int main(void){'+checks,1)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(root/'include'),'-I'+str(root/'SDK'),str(p/'test.c'),str(root/'src/core.c'),str(root/'src/dashboard_layout.c'),'-o',str(p/'test')]
 if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],cwd=p,check=True)
