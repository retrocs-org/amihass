/* Isolated sample-only reliability checks. Never uses real credentials/devices. */
#include <exec/types.h>
#include <exec/execbase.h>
extern struct ExecBase *SysBase;
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amihass.h"
#include "amihass_config.h"
#include "camera_thumbs.h"
#include "catalogue_picker.h"
#include "camera.h"
#include "preferences.h"
struct IntuitionBase *IntuitionBase=NULL;
long __stack=65536;
static struct AmiHASSMessage *wait_reply(struct AmiHASSClient *c)
{int i;struct AmiHASSMessage *m;for(i=0;i<300;++i){m=amihass_reply(c);if(m)return m;Delay(1);}return NULL;}
static struct AmiHASSMessage *exchange(struct AmiHASSClient *c,int kind,const char *entity,const char *action,const char *domain)
{if(!amihass_send(c,kind,"Reliability.QA",entity,action,0,0,domain,NULL))return NULL;return wait_reply(c);}
static unsigned long idle_window(void)
{unsigned long before=SysBase->IdleCount;Delay(250);return SysBase->IdleCount-before;}
int main(int argc,char **argv)
{struct AmiHASSConfig cfg;struct AmiHASSClient a,b,observer;struct AmiHASSMessage *m=NULL;struct AHControls controls;struct AHCameraThumbs *thumb=NULL;struct Screen *screen=NULL;struct Window *window=NULL;unsigned long ticket,warm=0,ending=0,idle_baseline,idle_service,fast_baseline;int i,j,ok=0,stage=0;char id[96],name[64];memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));memset(&observer,0,sizeof(observer));amihass_config_defaults(&cfg);cfg.sample_mode=1;cfg.low_memory=1;cfg.boot=0;strcpy(cfg.service_dir,"QATests:");idle_baseline=idle_window();fast_baseline=AvailMem(MEMF_FAST);if(!amihass_config_save(&cfg)||!amihass_start("QATests:AmiHASSService"))goto done;for(i=0;i<200;++i){int live;Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();if(live)break;Delay(2);}if(i==200)goto done;
 idle_service=idle_window();printf("PERF idle calibration=%lu service=%lu; estimated system busy=%lu%% (emulator idle counter, not process CPU). Fast service cost=%ld bytes.\n",idle_baseline,idle_service,idle_baseline&&idle_service<idle_baseline ? (idle_baseline-idle_service)/(idle_baseline/100UL+1UL):0UL,(long)fast_baseline-(long)AvailMem(MEMF_FAST));
 if(argc==2&&!strcmp(argv[1],"HEALTH"))goto health;
 stage=1;m=exchange(&a,AMIHASS_ACTION,"button.amihass_sample_doorbell_test","Press",NULL);if(!m||m->result!=AMIHASS_WAIT)goto done;ticket=m->age;FreeVec(m);m=NULL;m=exchange(&b,AMIHASS_ACTION,"button.amihass_sample_doorbell_test","Press",NULL);if(!m||m->result!=AMIHASS_WAIT||m->age==ticket)goto done;FreeVec(m);m=NULL;m=exchange(&a,AMIHASS_QUERY,"button.amihass_sample_doorbell_test",NULL,NULL);if(!m)goto done;amihass_controls_items(m->items,m->count,&controls);if(m->result!=AMIHASS_OK||!controls.press||controls.delivery!=ticket||strcmp(m->item.state,"unknown"))goto done;FreeVec(m);m=NULL;puts("PASS: native stateless Press and per-client delivery receipts.");amihass_client_close(&a);amihass_client_close(&b);
 stage=2;if(!ah_catalogue_choose(AMIHASS_LABELS,"",id,name,1)||strcmp(id,"lighting")||strcmp(name,"Lighting"))goto done;puts("PASS: native paged label selector returns stable ID and friendly name.");
 stage=3;IntuitionBase=(struct IntuitionBase*)OpenLibrary("intuition.library",39);if(!IntuitionBase)goto done;screen=LockPubScreen(NULL);if(!screen)goto done;window=OpenWindowTags(NULL,WA_Title,(ULONG)"AmiHASS thumbnail reliability",WA_PubScreen,(ULONG)screen,WA_Width,260,WA_Height,110,WA_Left,12,WA_Top,screen->BarHeight+10,WA_Flags,WFLG_DRAGBAR|WFLG_DEPTHGADGET,TAG_DONE);if(!window)goto done;
 for(i=0;i<100;++i){thumb=ah_thumbs_new(screen);if(!thumb)goto done;ah_thumbs_limit(thumb,1);if(!ah_thumbs_request(thumb,0,"camera.amihass_sample_driveway"))goto done;for(j=0;j<500;++j){if(ah_thumbs_step(thumb))break;Delay(1);}if(j==500||!ah_thumbs_draw(thumb,0,window->RPort,12,30)){printf("FAIL preview: %s\n",ah_thumbs_status(thumb));goto done;}ah_thumbs_free(thumb);thumb=NULL;if(i==9)warm=AvailMem(MEMF_FAST);if(i%25==24)printf("SOAK %d thumbnail cycles; Fast free %lu\n",i+1,AvailMem(MEMF_FAST));}
 ending=AvailMem(MEMF_FAST);if(!warm||ending+262144UL<warm){printf("FAIL sustained memory growth: %ld\n",(long)warm-(long)ending);goto done;}printf("PASS: 100 native thumbnail decode/draw/close cycles; observed retained Fast bytes %ld.\n",(long)warm-(long)ending);CloseWindow(window);window=NULL;UnlockPubScreen(NULL,screen);screen=NULL;CloseLibrary((struct Library*)IntuitionBase);IntuitionBase=NULL;
 stage=4;for(i=0;i<10;++i)if(!ah_camera_view("camera.amihass_sample_driveway",1))goto done;puts("PASS: repeated snapshot viewer open/close cycles.");
 stage=5;for(i=0;i<30;++i){cfg.sample_mode=i%2;if(!amihass_config_save(&cfg))goto done;m=exchange(&observer,AMIHASS_RELOAD,NULL,NULL,NULL);if(!m||m->result!=AMIHASS_OK)goto done;FreeVec(m);m=NULL;if(!amihass_snapshot(&a,"Sources.QA","camera.amihass_sample_driveway",0,NULL,1))goto done;m=wait_reply(&a);if(!m||(cfg.sample_mode?m->result!=AMIHASS_OK:m->result!=AMIHASS_UNCONFIGURED))goto done;FreeVec(m);m=NULL;amihass_client_close(&a);}
 m=exchange(&observer,AMIHASS_DIAGNOSTICS,NULL,NULL,"camera");if(!m||m->count!=7||strcmp(m->items[3].state,"0")||strcmp(m->items[4].state,"0"))goto done;FreeVec(m);m=NULL;m=exchange(&observer,AMIHASS_DIAGNOSTICS,NULL,NULL,"transport");if(!m||m->count!=7||strcmp(m->items[0].state,"Sample"))goto done;FreeVec(m);m=NULL;puts("PASS: 30 provider changes, immediate low-memory cache release and transport/camera diagnostics.");
health:stage=6;if(!amihass_diagnostics_check()||!amihass_dependencies_check())goto done;puts("PASS: live Diagnostics, report export and local installation health windows.");ok=1;
done:if(m)FreeVec(m);if(thumb)ah_thumbs_free(thumb);if(window)CloseWindow(window);if(screen)UnlockPubScreen(NULL,screen);if(IntuitionBase)CloseLibrary((struct Library*)IntuitionBase);amihass_client_close(&a);amihass_client_close(&b);m=exchange(&observer,AMIHASS_STOP,NULL,NULL,NULL);if(m)FreeVec(m);amihass_client_close(&observer);if(!ok)printf("FAIL native six-feature reliability stage %d\n",stage);return ok?0:10;}
