/* Disposable fixture only. This test cannot send ACTION requests. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "amihass.h"
#include "amihass_config.h"
long __stack=65536;
static unsigned long free_min=~0UL;
static void memory_sample(void){unsigned long n=AvailMem(MEMF_FAST);if(n<free_min)free_min=n;}
static struct AmiHASSMessage *request(struct AmiHASSClient *c,int kind,const char *entity)
{int n;struct AmiHASSMessage *m;if(!amihass_send(c,kind,"Push.QA",entity,NULL,0,0,NULL,NULL))return NULL;for(n=0;n<200;++n){m=amihass_reply(c);if(m)return m;memory_sample();Delay(1);}return NULL;}
int main(int argc,char **argv)
{struct AmiHASSConfig cfg;struct AmiHASSClient client,observer;struct AmiHASSMessage *m;struct AHControls controls;FILE *f;unsigned long baseline=AvailMem(MEMF_FAST);int i,ok=0,pushed=0,changed=0,fallback=0,idle=0,rejoined=0,reconnect_test=argc==2&&!strcmp(argv[1],"RECONNECT");char line[1100];memset(&client,0,sizeof(client));memset(&observer,0,sizeof(observer));amihass_config_defaults(&cfg);f=fopen("TLSFixture:credentials","rb");if(!f){puts("FAIL fixture input");return 10;}if(!fgets(line,sizeof(line),f)){fclose(f);return 10;}line[strcspn(line,"\r\n")]=0;strcpy(cfg.connection.url,line);if(!fgets(line,sizeof(line),f)){fclose(f);return 10;}fclose(f);line[strcspn(line,"\r\n")]=0;strcpy(cfg.token,line);cfg.connection.allow_http=0;cfg.push_updates=1;cfg.reconnect=1;cfg.boot=0;cfg.connection.poll=10;strcpy(cfg.service_dir,"QATests:");if(!SetVar("CURL_CA_BUNDLE","TLSFixture:ca.pem",-1,GVF_GLOBAL_ONLY)||!amihass_config_save(&cfg)){puts("FAIL fixture settings");return 10;}amihass_erase(&cfg,sizeof(cfg));if(!amihass_start("QATests:AmiHASSService")){puts("FAIL service startup");return 10;}
 for(i=0;i<900;++i){m=request(&client,AMIHASS_QUERY,"light.fixture");if(m){amihass_controls_items(m->items,m->count,&controls);if(m->result==AMIHASS_OK&&controls.level_known&&controls.level==50)pushed=1;if(pushed&&m->result==AMIHASS_OK&&controls.level_known&&controls.level==100)changed=1;if(changed&&m->result==AMIHASS_OK&&!strcmp(m->item.state,"off"))fallback=1;FreeVec(m);}m=request(&observer,AMIHASS_STATUS,NULL);if(m){if(i%50==0)printf("TRACE server=%s transport=%s pushed=%s errors=%s\n",m->item.state,m->items[7].state,m->items[8].state,m->items[9].state);FreeVec(m);}if(fallback)break;Delay(5);}
 if(!pushed||!changed||!fallback){printf("FAIL push full=%d delta=%d REST fallback=%d\n",pushed,changed,fallback);goto done;}puts("PASS native verified TLS, selected initial state, delta and REST fallback.");
 if(reconnect_test){for(i=0;i<1400;++i){m=request(&client,AMIHASS_QUERY,"light.fixture");if(m){amihass_controls_items(m->items,m->count,&controls);if(m->result==AMIHASS_OK&&controls.level_known&&controls.level==50)rejoined=1;FreeVec(m);}m=request(&observer,AMIHASS_STATUS,NULL);if(m){if(rejoined&&!strcmp(m->items[7].state,"WebSocket")){FreeVec(m);break;}FreeVec(m);}Delay(5);}if(!rejoined||i==1400){puts("FAIL automatic selected reconnect");goto done;}printf("PERF reconnect Fast baseline=%lu minimum=%lu current=%lu bytes\n",baseline,free_min,AvailMem(MEMF_FAST));puts("PASS automatic reconnect after fixture disconnect; original selection retained.");}
 amihass_client_close(&client);for(i=0;i<150;++i){Forbid();idle=FindPort("AmiHASS.PushWorker")==NULL;Permit();if(idle)break;Delay(5);}if(!idle){puts("FAIL idle worker cleanup");goto done;}puts("PASS native no-selection worker cleanup; no device actions sent.");ok=1;
done:amihass_client_close(&client);m=request(&observer,AMIHASS_STOP,NULL);if(m)FreeVec(m);amihass_client_close(&observer);return ok?0:10;}
