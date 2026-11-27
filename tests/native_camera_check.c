/* Fixture-only camera image transfer; no real server or device actions. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "camera.h"
#include "camera_thumbs.h"
struct IntuitionBase *IntuitionBase=NULL;
#include "amihass_config.h"
long __stack=65536;
static unsigned long free_min=~0UL;
static void memory_sample(void){unsigned long n=AvailMem(MEMF_FAST);if(n<free_min)free_min=n;}
static struct AmiHASSMessage *answer(struct AmiHASSClient *c)
{int i;struct AmiHASSMessage *m;for(i=0;i<400;++i){m=amihass_reply(c);if(m)return m;memory_sample();Delay(1);}return NULL;}
int main(int argc,char **argv)
{struct AmiHASSConfig cfg;struct AmiHASSClient a,b;struct AmiHASSMessage *m=NULL;unsigned char image[8192];unsigned long offset=0,size=0,n,baseline,after;char version[20],format[8],line[1100];unsigned int width,height;int flushed=0,stage=0,i,ok=0,fresh=1,fixture=argc==2&&!strcmp(argv[1],"FIXTURE");const char *entity=fixture?"camera.fixture":"camera.amihass_sample_driveway";FILE *f;memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));amihass_config_defaults(&cfg);cfg.sample_mode=!fixture;cfg.low_memory=1;cfg.boot=0;strcpy(cfg.service_dir,"QATests:");
 if(fixture){f=fopen("TLSFixture:credentials","rb");if(!f)return 10;if(!fgets(line,sizeof(line),f)){fclose(f);return 10;}line[strcspn(line,"\r\n")]=0;if(strncmp(line,"https://",8)){fclose(f);return 10;}strcpy(cfg.connection.url,line);if(!fgets(line,sizeof(line),f)){fclose(f);return 10;}fclose(f);line[strcspn(line,"\r\n")]=0;if(strncmp(line,"amihass_fixture_",16))return 10;strcpy(cfg.token,line);if(!SetVar("CURL_CA_BUNDLE","TLSFixture:ca.pem",-1,GVF_GLOBAL_ONLY))return 10;}
 baseline=AvailMem(MEMF_FAST);memory_sample();if(!amihass_config_save(&cfg)||!amihass_start("QATests:AmiHASSService"))goto done;stage=1;for(i=0;i<200;++i){int present;Forbid();present=FindPort(AMIHASS_PORT)!=NULL;Permit();if(present)break;Delay(2);}if(i==200)goto done;
 for(i=0;i<700&&offset<sizeof(image);++i){if(!amihass_snapshot(&a,"Camera.QA.A",entity,(long)offset,offset?version:NULL,fresh))goto done;fresh=0;m=answer(&a);if(!m)goto done;if(m->result==AMIHASS_WAIT||m->result==AMIHASS_BUSY){FreeVec(m);m=NULL;Delay(5);continue;}if(!amihass_snapshot_data(m,&n)||m->total>sizeof(image)){printf("TRACE snapshot result=%ld total=%ld offset=%ld bytes=%ld version=%s\n",m->result,m->total,m->offset,m->refresh,m->item.updated);goto done;}stage=2;if(!offset){size=m->total;strncpy(version,m->item.updated,19);version[19]=0;}if(offset+n>size)goto done;memcpy(image+offset,m->items,n);offset+=n;FreeVec(m);m=NULL;if(offset==size)break;}
 stage=3;if(!size||offset!=size||!ah_camera_probe(image,size,&width,&height,format)||width!=160||height!=96)goto done;
 stage=4;if(!amihass_snapshot(&b,"Camera.QA.B",entity,0,NULL,0))goto done;m=answer(&b);if(!m||!amihass_snapshot_data(m,&n)||strcmp(m->item.updated,version)||memcmp(m->items,image,n))goto done;FreeVec(m);m=NULL;puts("PASS: native versioned image chunks shared by two clients.");
 if(!amihass_snapshot(&b,"Camera.QA.B",entity,2048,"invalid-version",0))goto done;m=answer(&b);if(!m||m->result!=AMIHASS_STALE)goto done;FreeVec(m);m=NULL;
 stage=5;if(!ah_camera_view(entity,1))goto done;puts("PASS: native camera viewer renders a bounded snapshot and closes cleanly.");{struct Screen *screen;struct Window *window;struct AHCameraThumbs *thumb;int pass=0,j;IntuitionBase=(struct IntuitionBase*)OpenLibrary("intuition.library",39);if(!IntuitionBase)goto done;screen=LockPubScreen(NULL);if(!screen)goto done;window=OpenWindowTags(NULL,WA_Title,(ULONG)"Camera fixture preview",WA_PubScreen,(ULONG)screen,WA_Width,240,WA_Height,100,WA_Top,screen->BarHeight+10,WA_Flags,WFLG_DRAGBAR|WFLG_DEPTHGADGET,TAG_DONE);thumb=window?ah_thumbs_new(screen):NULL;if(thumb&&ah_thumbs_request(thumb,0,entity)){for(j=0;j<500;++j){if(ah_thumbs_step(thumb))break;Delay(1);}pass=ah_thumbs_draw(thumb,0,window->RPort,12,30);if(!pass)printf("FAIL fixture thumbnail: %s\n",ah_thumbs_status(thumb));}if(thumb)ah_thumbs_free(thumb);if(window)CloseWindow(window);UnlockPubScreen(NULL,screen);CloseLibrary((struct Library*)IntuitionBase);IntuitionBase=NULL;if(!pass)goto done;puts("PASS native decoded camera thumbnail drawn to Workbench.");}amihass_client_close(&a);amihass_client_close(&b);
 if(fixture){int cycle,j;for(cycle=0;cycle<3;++cycle){for(j=0;j<700;++j){if(!amihass_snapshot_size(&a,"Camera.Soak",entity,0,NULL,j==0,cycle==0))goto done;m=answer(&a);if(!m)goto done;if(m->result==AMIHASS_WAIT||m->result==AMIHASS_BUSY){FreeVec(m);m=NULL;memory_sample();Delay(5);continue;}if(!amihass_snapshot_data(m,&n))goto done;FreeVec(m);m=NULL;break;}if(j==700)goto done;amihass_client_close(&a);printf("PERF HTTPS cycle %d Fast free=%lu bytes\n",cycle+1,AvailMem(MEMF_FAST));}puts("PASS repeated authenticated full-size and thumbnail camera fetches.");stage=6;Delay(1750);}
 if(amihass_send(&a,AMIHASS_DIAGNOSTICS,"Camera.Perf",NULL,NULL,0,0,"memory",NULL)){m=answer(&a);if(m&&m->result==AMIHASS_OK){for(i=0;i<m->count;++i){printf("PERF %s=%s\n",m->items[i].id,m->items[i].state);if(!strcmp(m->items[i].id,"memory.flushes")&&sscanf(m->items[i].state,"%lu",&n)==1&&n>0)flushed=1;}}if(m)FreeVec(m);m=NULL;}if(fixture&&!flushed){puts("FAIL automatic low-memory idle reclamation was not observed");goto done;}if(fixture)puts("PASS automatic low-memory idle reclamation observed.");ok=1;
done:if(m)FreeVec(m);amihass_client_close(&a);amihass_client_close(&b);if(amihass_send(&a,AMIHASS_STOP,"Camera.QA",NULL,NULL,0,0,NULL,NULL)){m=answer(&a);if(m)FreeVec(m);}amihass_client_close(&a);for(i=0;i<150;++i){int live;Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();if(!live)break;Delay(2);}after=AvailMem(MEMF_FAST);printf("PERF camera baseline=%lu minimum=%lu after=%lu observed drop=%lu retained=%ld bytes\n",baseline,free_min,after,baseline>free_min ? baseline-free_min:0UL,(long)baseline-(long)after);amihass_erase(&cfg,sizeof(cfg));if(!ok)printf("FAIL stage %d\n",stage);puts(ok?"PASS: camera snapshot QA; fixture/sample images only, no actions.":"FAIL: camera snapshot QA.");return ok?0:10;}
