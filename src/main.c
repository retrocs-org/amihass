#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "service.h"
#include "nd_panel.h"
#include "preferences.h"
#include "ah_log.h"
#include "connectivity.h"
#ifdef AMIHASS_SERVICE
const char version[]="$VER: AmiHASSService 0.2 (20.11.2026)";
#else
const char version[]="$VER: AmiHASSPrefs 0.2 (20.11.2026)";
#endif
long __stack=65536;
static int exchange(struct AmiHASSClient *c,int kind,const char *entity,const char *action,struct AmiHASSMessage **m)
{int n;if(!amihass_send(c,kind,"AmiHASS.Prefs",entity,action,0,0,NULL,NULL))return 0;for(n=0;n<110;++n){*m=amihass_reply(c);if(*m)return 1;Delay(5);}return 0;}
static int service_present(void)
{int live;Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();return live;}
static int stop_service(struct AmiHASSClient *client)
{
 struct AmiHASSMessage *reply=NULL;int n,accepted;
 if(!exchange(client,AMIHASS_STOP,NULL,NULL,&reply)){
  amihass_client_close(client);
  return service_present() ? 20:0;
 }
 accepted=reply->result==AMIHASS_OK;FreeVec(reply);amihass_client_close(client);
 if(!accepted)return 20;
 for(n=0;n<100;++n){if(!service_present())return 0;Delay(5);}
 return 20;
}
int main(int argc,char **argv)
{
 char self[520];struct AmiHASSClient client;struct AmiHASSMessage *m=NULL;int mode=0,n;
 memset(&client,0,sizeof(client));self[0]=0;if(GetProgramDir() && NameFromLock(GetProgramDir(),self,sizeof(self)))AddPart(self,"AmiHASSService",sizeof(self));
 if(argc>1 && !strcmp(argv[1],"PROBE"))return argc==3 ? amihass_probe_worker(argv[2]):20;
 if(argc>1 && !strcmp(argv[1],"FETCH"))return argc==3 ? amihass_worker(argv[2]):20;
#ifdef AMIHASS_SERVICE
 if(argc<=1 || (argc==2 && !strcmp(argv[1],"SERVICE")))return amihass_backend(self);
#endif
 if(argc==2 && !strcmp(argv[1],"RESTORE"))return amihass_backend_restore(self);
 if(argc>1){
  if(!strcmp(argv[1],"ABOUT")){amihass_about_ui();return 0;}
  if(!strcmp(argv[1],"DIAGNOSTICS-SELFTEST"))return amihass_diagnostics_check() ? 0:10;
  if(!strcmp(argv[1],"DIAGNOSTICS")){amihass_diagnostics_ui();return 0;}
  if(!strcmp(argv[1],"LOGS")){amihass_logs_ui();return 0;}
  if(!strcmp(argv[1],"CONFIG")){amihass_preferences_ui(1);return 0;}
  if(!strcmp(argv[1],"STATUS"))mode=AMIHASS_STATUS;
  else if(!strcmp(argv[1],"RECONNECT"))mode=AMIHASS_RECONNECT;
  else if(!strcmp(argv[1],"STOP"))mode=AMIHASS_STOP;
  else if(!strcmp(argv[1],"QUERY") && argc==3)mode=AMIHASS_QUERY;
  else if(!strcmp(argv[1],"ACTION") && argc==4)mode=AMIHASS_ACTION;
  else if(!strcmp(argv[1],"SERVICE"))return amihass_backend(self);
  else {puts("AmiHASSService [SERVICE|STATUS|RECONNECT|STOP|QUERY entity|ACTION entity Toggle]");return 20;}
  if(mode==AMIHASS_STOP)return stop_service(&client);
  if(!exchange(&client,mode,argc>2 ? argv[2]:NULL,argc>3 ? argv[3]:NULL,&m)){puts("AmiHASS service is not running.");amihass_client_close(&client);return 20;}
  if(mode==AMIHASS_STATUS)printf("AmiHASS 0.2 ABI 1; server %s; subscriptions %ld; uptime %lu seconds; connected %s\n",m->status.server,m->status.subscriptions,m->status.uptime,m->status.connected ? "yes":"no");
  else if(mode==AMIHASS_RECONNECT)puts("Home Assistant reconnect requested.");
  else printf("%s: %s %s (%s)\n",m->item.id,m->item.state,m->item.unit,amihass_problem(m->result));n=m->result==AMIHASS_OK || m->result==AMIHASS_WAIT ? 0:10;FreeVec(m);amihass_client_close(&client);return n;
 }
 amihass_preferences_ui(0);return 0;
}
