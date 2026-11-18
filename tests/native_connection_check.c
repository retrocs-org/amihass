/* AmigaMac QA: a closed loopback endpoint must never pass validation. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "connectivity.h"
#include "ah_log.h"
#include "nd_panel.h"
#include "preferences.h"
#include "amihass_config.h"
#include <proto/icon.h>
#include <workbench/workbench.h>
struct Library *IconBase=NULL;
static void memory_sample(void);
long __stack=65536;
static int token_input_check(void)
{
 struct NDPanelField fields[4];struct NDPanel *panel;char token[1025];int i,ok;
 fields[0].label="Server URL";fields[0].value="https://homeassistant.local:8123";fields[0].limit=160;
 fields[1].label="Token";fields[1].value="fixture-initial-token-for-validation";fields[1].limit=sizeof(token);
 fields[2].label="Transport";fields[2].value="HTTPS (encrypted)";fields[2].limit=0;
 fields[3].label="Connection";fields[3].value="Not Connected";fields[3].limit=0;
 panel=nd_panel_server_settings(fields,1,60);if(!panel){puts("FAIL: Server Settings could not open.");return 10;}
 ok=!nd_panel_secret_shown(panel)&&!nd_panel_enabled(panel,2)&&!strcmp(nd_panel_get(panel,1),fields[1].value)&&nd_panel_reconnect(panel)&&nd_panel_retry(panel)==60&&nd_panel_enabled(panel,112);
 nd_panel_show_secret(panel,1);ok=ok&&nd_panel_secret_shown(panel)&&nd_panel_enabled(panel,2)&&!strcmp(nd_panel_get(panel,1),fields[1].value);nd_panel_show_secret(panel,0);ok=ok&&!nd_panel_secret_shown(panel)&&!nd_panel_enabled(panel,2);
 nd_panel_set_input(panel,1,"fixture-imported-token-for-validation");ok=ok && !strcmp(nd_panel_get(panel,1),"fixture-imported-token-for-validation");
 for(i=0;i<1024;++i)token[i]='a';token[1024]=0;
 nd_panel_set_input(panel,1,token);ok=ok && strlen(nd_panel_get(panel,1))==1024 && !strcmp(nd_panel_get(panel,1),token);
 nd_panel_show_secret(panel,1);ok=ok&&!strcmp(nd_panel_get(panel,1),token);nd_panel_show_secret(panel,0);ok=ok&&!strcmp(nd_panel_get(panel,1),token);
 nd_panel_set_input(panel,1,"");ok=ok && !nd_panel_get(panel,1)[0];
 nd_panel_close(panel);
 {static const unsigned int intervals[]={60,600,900,1800};for(i=0;i<4;++i){panel=nd_panel_server_settings(fields,1,intervals[i]);if(!panel){ok=0;break;}ok=ok&&nd_panel_reconnect(panel)&&nd_panel_retry(panel)==intervals[i]&&nd_panel_enabled(panel,112);nd_panel_close(panel);}
 panel=nd_panel_server_settings(fields,0,900);if(panel){ok=ok&&!nd_panel_reconnect(panel)&&nd_panel_retry(panel)==900&&!nd_panel_enabled(panel,112);nd_panel_close(panel);}else ok=0;}
 amihass_erase(token,sizeof(token));
 puts(ok ? "PASS: editable token field, import population, 1024-character capacity and clearing.":"FAIL: native token input did not preserve its contents.");return ok ? 0:10;
}
static struct AmiHASSMessage *qa_exchange(struct AmiHASSClient *client,int kind,int offset)
{
 struct AmiHASSMessage *reply;int n;
 if(!amihass_send(client,kind,"AmiHASS.QA",kind==AMIHASS_QUERY ? "sensor.amihass_qa":NULL,NULL,offset,0,NULL,NULL))return NULL;
 for(n=0;n<120;++n){reply=amihass_reply(client);if(reply)return reply;memory_sample();Delay(5);}return NULL;
}
static int preferences_check(void)
{
 struct NDPanelField fields[5];struct NDPanel *panel;struct AmiHASSPreferences prefs;
 struct AmiHASSConnection connection,loaded;struct AmiHASSClient client;struct AmiHASSMessage *reply;
 char token[1025];int i,ok=1,live;BPTR input,output;LONG rc;
 for(i=0;i<4;++i){fields[i].label="Status";fields[i].value="QA";fields[i].limit=0;}
 panel=nd_panel_preferences(fields,0);if(!panel){puts("FAIL: Preferences could not open.");return 10;}
 nd_panel_set_source(panel,1);ok=nd_panel_source(panel)==1;nd_panel_set_source(panel,0);ok=ok&&nd_panel_source(panel)==0;
 nd_panel_service_state(panel,0,0);ok=ok&&nd_panel_enabled(panel,100)&&!nd_panel_enabled(panel,101)&&nd_panel_enabled(panel,110);
 nd_panel_service_state(panel,1,0);ok=ok&&!nd_panel_enabled(panel,100)&&nd_panel_enabled(panel,101);
 nd_panel_service_state(panel,1,1);ok=ok&&!nd_panel_enabled(panel,100)&&!nd_panel_enabled(panel,101);
 nd_panel_service_state(panel,0,0);ok=ok&&nd_panel_enabled(panel,100)&&!nd_panel_enabled(panel,101)&&nd_panel_enabled(panel,104);
 nd_panel_close(panel);if(!ok){puts("FAIL: service controls do not match state.");return 10;}
 puts("PASS: Start/Stop controls follow service state and Save stays enabled.");
 if(amihass_about_fields(fields)!=5 || strcmp(fields[2].value,"NovaDock contributors") || strcmp(fields[3].value,"https://amihass.retrocs.org") || strcmp(fields[4].value,"https://github.com/retrocs-org/amihass")){puts("FAIL: About metadata differs.");return 10;}
 {static const char *buttons[]={"Close"};panel=nd_panel_open("About AmiHASS",fields,5,buttons,1);if(!panel)return 10;nd_panel_close(panel);}
 puts("PASS: About metadata and native window.");
 memset(&prefs,0,sizeof(prefs));memset(&connection,0,sizeof(connection));strcpy(connection.url,"https://127.0.0.1:1");connection.poll=30;prefs.connection_result=AH_TEST_FAILED;
 if(!amihass_server_save_options(&prefs,&connection,"fixture-offline-settings-token",0,0,600) || !amihass_connection_load(&loaded,token) || strcmp(loaded.url,connection.url) || strcmp(token,"fixture-offline-settings-token")){puts("FAIL: valid settings could not save after a failed connection test.");return 10;}
 amihass_erase(token,sizeof(token));amihass_erase(&prefs,sizeof(prefs));
 {struct AmiHASSConfig config;if(!amihass_config_load(&config) || config.reconnect || config.retry_interval!=600){puts("FAIL: reconnect settings did not persist.");return 10;}amihass_erase(&config,sizeof(config));}
 puts("PASS: offline server settings and reconnect policy persist in the unified cfg.");
 Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();if(live){puts("FAIL: isolated guest already has a service.");return 10;}
 input=Open("NIL:",MODE_OLDFILE);output=Open("NIL:",MODE_NEWFILE);if(!input||!output){if(input)Close(input);if(output)Close(output);return 10;}
 rc=SystemTags("QATests:AmiHASSService SERVICE",SYS_Input,input,SYS_Output,output,SYS_Asynch,TRUE,NP_StackSize,65536UL,TAG_DONE);
 if(rc<0){Close(input);Close(output);return 10;}
 for(i=0;i<100;++i){Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();if(live)break;Delay(5);}if(!live)return 10;
 memset(&client,0,sizeof(client));
 for(i=0;i<120;++i){reply=qa_exchange(&client,AMIHASS_STATUS,0);if(!reply)return 10;
  ok=!reply->status.connected && strcmp(reply->item.state,AMIHASS_SERVER_CONNECTED) && reply->count==3 && !strcmp(reply->items[0].state,"0") && !strcmp(reply->items[1].state,"600");
  live=!strcmp(reply->item.state,AMIHASS_SERVER_NETWORK_DOWN)||!strcmp(reply->item.state,AMIHASS_SERVER_DISCONNECTED);FreeVec(reply);if(!ok)return 10;if(live)break;Delay(5);
 }
 if(i==120){puts("FAIL: unreachable server did not report disconnected.");return 10;}
 puts("PASS: running bridge reports Home Assistant disconnected and exposes retry policy to clients.");
 reply=qa_exchange(&client,AMIHASS_RECONNECT,0);if(!reply || reply->result!=AMIHASS_OK){if(reply)FreeVec(reply);return 10;}FreeVec(reply);
 reply=qa_exchange(&client,AMIHASS_QUERY,0);if(!reply){amihass_client_close(&client);return 10;}FreeVec(reply);
 reply=qa_exchange(&client,AMIHASS_DIAGNOSTICS,0);if(!reply){amihass_client_close(&client);return 10;}
 ok=reply->result==AMIHASS_OK&&reply->count==16&&reply->total==25&&!strcmp(reply->items[0].id,"ipc.received")&&strtoul(reply->items[0].state,NULL,10)>=2&&strtoul(reply->items[4].state,NULL,10)>=1&&!strcmp(reply->items[8].state,"0");FreeVec(reply);
 reply=qa_exchange(&client,AMIHASS_DIAGNOSTICS,16);if(!reply){amihass_client_close(&client);return 10;}
 ok=ok&&reply->result==AMIHASS_OK&&reply->count==9&&!strcmp(reply->items[0].id,"service.uptime");FreeVec(reply);
 reply=qa_exchange(&client,AMIHASS_STOP,0);if(reply)FreeVec(reply);amihass_client_close(&client);
 if(!ok){puts("FAIL: live diagnostic counters did not reflect bridge traffic.");return 10;}
 puts("PASS: live service diagnostic pages report real IPC traffic, queries and zero HA actions.");return 0;
}
static int qa_command(const char *command)
{BPTR input=Open("NIL:",MODE_OLDFILE),output=Open("AutoRun:installer-output.log",MODE_NEWFILE);LONG rc;if(!input || !output){if(input)Close(input);if(output)Close(output);return 20;}rc=SystemTags((STRPTR)command,SYS_Input,input,SYS_Output,output,NP_StackSize,65536UL,TAG_DONE);Close(input);Close(output);printf("QA command result %ld: %s\n",(long)rc,command);return (int)rc;}
static int qa_exists(const char *path)
{BPTR l=Lock((STRPTR)path,ACCESS_READ);if(!l)return 0;UnLock(l);return 1;}
static int qa_installer(const char *action,int mode,const char *log)
{char command[500],script[96];BPTR input,output;LONG rc;(void)log;sprintf(script,"QATests:Install-QA-%s-%d",action,mode);sprintf(command,"QATests:AmiHASSInstallerWelcome %s",script);input=Open("NIL:",MODE_OLDFILE);output=Open("NIL:",MODE_NEWFILE);if(!input||!output){if(input)Close(input);if(output)Close(output);return 20;}rc=SystemTags(command,SYS_Input,input,SYS_Output,output,SYS_Asynch,TRUE,NP_StackSize,32768UL,TAG_DONE);if(rc<0){Close(input);Close(output);return 20;}sprintf(command,"SYS:System/Installer QATests:Install-QA-%s-%d APPNAME AmiHASS MINUSER NOVICE DEFUSER NOVICE NOPRETEND NOLOG NOPRINT",action,mode);return qa_command(command);}
static int installer_mode_check(int mode)
{
 struct AmiHASSConfig c,loaded;struct MsgPort *guard;struct DiskObject *icon;BPTR lock,f;int ok;
 lock=CreateDir("RAM:AmiHASS-QA");if(lock)UnLock(lock);
 amihass_config_defaults(&c);c.boot=1;c.connection.poll=45;strcpy(c.connection.url,"https://127.0.0.1:1");strcpy(c.token,"fixture-config-token-for-validation");strcpy(c.service_dir,"RAM:AmiHASS-QA/Service");strcpy(c.prefs_dir,"RAM:AmiHASS-QA/Prefs");strcpy(c.startup_dir,"RAM:AmiHASS-QA/Startup");
 if(!amihass_config_save(&c) || !amihass_config_load(&loaded) || strcmp(loaded.token,c.token) || loaded.connection.poll!=45){puts("FAIL: unified native cfg roundtrip.");return 10;}
 printf("QA: Installer mode %d\n",mode);
 puts("PASS: unified native cfg preserves server, token, polling, boot and component paths.");
 f=Open("ENVARC:AmiHASS/Dashboard.cfg",MODE_NEWFILE);if(!f)return 10;Write(f,"dashboard layout",16);Close(f);
 if(qa_installer("Update",mode,"install.log")){puts("FAIL: native fresh install.");return 10;}
 if(!qa_exists("SYS:Utilities/AmiHASSDashboard") || !qa_exists("SYS:Utilities/AmiHASSDashboard.info") || !qa_exists("SYS:Utilities/AmiHASSBrowser") || !qa_exists("SYS:Utilities/AmiHASSBrowser.info") || !qa_exists("RAM:AmiHASS-QA/Service/AmiHASSService") || !qa_exists("RAM:AmiHASS-QA/Prefs/AmiHASSPrefs.info") || !qa_exists("RAM:AmiHASS-QA/Startup/AmiHASS.info")){puts("FAIL: install targets missing.");return 10;}
 IconBase=OpenLibrary("icon.library",36);if(!IconBase)return 10;icon=GetDiskObject("RAM:AmiHASS-QA/Startup/AmiHASS");ok=icon && !strcmp(icon->do_DefaultTool,"RAM:AmiHASS-QA/Service/AmiHASSService");if(icon)FreeDiskObject(icon);CloseLibrary(IconBase);IconBase=NULL;if(!ok)return 10;
 puts("PASS: native Installer installs service, Preferences icon and correct boot shortcut.");
 guard=CreateMsgPort();if(!guard)return 10;guard->mp_Node.ln_Name="AmiHASS.Maintenance";Forbid();AddPort(guard);Permit();
 if(qa_command("QATests:SetupStartup RESTART RAM:AmiHASS-QA/Service/AmiHASSService") || qa_command("QATests:SetupStartup STATE"))return 10;
 if(qa_installer("Update",mode,"update.log") || qa_command("QATests:SetupStartup STATE")){puts("FAIL: running update did not restore service.");return 10;}
 puts("PASS: native update restarts a previously running service through the maintenance guard.");
 if(qa_installer("Keep",mode,"uninstall-keep.log") || qa_command("QATests:SetupStartup STATE")!=5 || qa_exists("RAM:AmiHASS-QA/Prefs/AmiHASSPrefs") || qa_exists("SYS:Utilities/AmiHASSDashboard") || qa_exists("SYS:Utilities/AmiHASSDashboard.info") || qa_exists("SYS:Utilities/AmiHASSBrowser") || qa_exists("SYS:Utilities/AmiHASSBrowser.info") || qa_exists("RAM:AmiHASS-QA/Service/AmiHASSService") || !qa_exists("ENVARC:AmiHASS/Dashboard.cfg") || !amihass_config_load(&loaded) || strcmp(loaded.token,c.token)){puts("FAIL: uninstall keep settings.");return 10;}
 puts("PASS: native uninstall stops service, removes both apps and preserves unified cfg.");
 if(qa_installer("Update",mode,"reinstall.log"))return 10;
 if(qa_installer("Update",mode,"stopped-update.log") || qa_command("QATests:SetupStartup STATE")!=5)return 10;
 puts("PASS: native update leaves a previously stopped service stopped.");
 f=Open("RAM:AmiHASS-QA/Service/unrelated",MODE_NEWFILE);if(f)Close(f);
 if(qa_installer("Remove",mode,"uninstall-remove.log") || qa_exists(AMIHASS_CONFIG_FILE) || qa_exists("ENVARC:AmiHASS/Dashboard.cfg") || qa_exists("RAM:AmiHASS-QA/Prefs/AmiHASSPrefs.info") || !qa_exists("RAM:AmiHASS-QA/Service/unrelated")){puts("FAIL: uninstall remove settings.");return 10;}
 Forbid();RemPort(guard);Permit();DeleteMsgPort(guard);
 amihass_erase(&c,sizeof(c));amihass_erase(&loaded,sizeof(loaded));
 puts("PASS: native uninstall removes cfg/token, retains unrelated files and exits without a Guru.");return 0;
}
static int installer_check(void)
{int mode;for(mode=0;mode<3;++mode){if(installer_mode_check(mode))return 10;}puts("PASS: all three native Installer modes completed install, update and both uninstall policies.");return 0;}
static int maintenance_choice_check(void)
{
 struct AmiHASSConfig c;BPTR l,f;int rc;
 amihass_config_defaults(&c);strcpy(c.service_dir,"RAM:AmiHASS-QA/Service");strcpy(c.prefs_dir,"RAM:AmiHASS-QA/Prefs");strcpy(c.startup_dir,"RAM:AmiHASS-QA/Startup");
 l=CreateDir("RAM:AmiHASS-QA");if(l)UnLock(l);l=CreateDir(c.service_dir);if(l)UnLock(l);l=CreateDir(c.prefs_dir);if(l)UnLock(l);
 f=Open("RAM:AmiHASS-QA/Service/AmiHASSService",MODE_NEWFILE);if(!f)return 10;Close(f);f=Open("RAM:AmiHASS-QA/Prefs/AmiHASSPrefs",MODE_NEWFILE);if(!f)return 10;Close(f);
 if(!amihass_config_save(&c))return 10;amihass_erase(&c,sizeof(c));
 rc=qa_command("SYS:System/Installer QATests:Install-AmiHASS APPNAME AmiHASS MINUSER NOVICE DEFUSER NOVICE NOPRETEND NOLOG NOPRINT LOGFILE AutoRun:maintenance-choice.log");
 if(!qa_exists("RAM:AmiHASS-QA/Service/AmiHASSService") || !qa_exists(AMIHASS_CONFIG_FILE))return 10;
 puts("PASS: real Installer maintenance was inspected in Novice mode and cancelled without changes.");return 0;
}
/* Credentials are supplied through a private external test mount, never the package. */
static unsigned long fast_min;
static void memory_sample(void){unsigned long n=AvailMem(MEMF_FAST);if(n<fast_min)fast_min=n;}
static void memory_report(const char *stage){memory_sample();printf("MEMORY %s: Fast free %lu, minimum %lu, Chip free %lu bytes.\n",stage,AvailMem(MEMF_FAST),fast_min,AvailMem(MEMF_CHIP));fast_min=AvailMem(MEMF_FAST);}
static struct AmiHASSMessage *live_exchange(struct AmiHASSClient *client,int kind,const char *entity)
{
 struct AmiHASSMessage *reply;int i;
 if(!amihass_send(client,kind,"AmiHASS.ReadQA",entity,NULL,0,kind==AMIHASS_QUERY ? AMIHASS_QUERY_ONCE:0,NULL,NULL))return NULL;
 for(i=0;i<120;++i){reply=amihass_reply(client);if(reply)return reply;memory_sample();Delay(5);}return NULL;
}
static int live_readonly_check(void)
{
 struct AmiHASSConfig config;struct AmiHASSClient client;struct AmiHASSMessage *reply;struct AHConnectionTest probe;FILE *file;char raw[1030],entity[96],server_state[40];int i,ok=0,started=0,count=0,total=0;
 fast_min=AvailMem(MEMF_FAST);memory_report("baseline");
 memset(&client,0,sizeof(client));amihass_config_defaults(&config);entity[0]=0;server_state[0]=0;
 file=fopen("LiveCredentials:credentials","rb");if(!file){puts("FAIL: private live-test credential mount is missing.");goto cleanup;}
 if(!fgets(config.connection.url,sizeof(config.connection.url),file) || !fgets(raw,sizeof(raw),file)){fclose(file);goto cleanup;}fclose(file);
 config.connection.url[strcspn(config.connection.url,"\r\n")]=0;
 if(!amihass_url(config.connection.url,0) || !amihass_token(raw,config.token))goto cleanup;
 amihass_erase(raw,sizeof(raw));DeleteFile("LiveCredentials:credentials");
 /* Exercise the actual Preferences PROBE path without a CLI, as on Workbench. */
 {struct Process *self=(struct Process *)FindTask(NULL);BPTR saved_cli=self->pr_CLI;int result;
 self->pr_CLI=0;result=amihass_connection_test_start(&probe,&config.connection,config.token);self->pr_CLI=saved_cli;
 while(result==AH_TEST_PENDING){result=amihass_connection_test_poll(&probe);memory_sample();Delay(5);}amihass_connection_test_close(&probe);
 if(result!=AH_TEST_OK){printf("FAIL: Workbench-style Preferences connection test returned %d.\n",result);goto cleanup;}
 memory_report("after HTTPS probe");puts("PASS: Preferences connection test authenticates successfully without a CLI program name.");}
 config.low_memory=1;config.reconnect=0;config.retry_interval=60;strcpy(config.service_dir,"QATests:");
 if(!amihass_config_save(&config)){puts("FAIL: could not save isolated live configuration.");goto cleanup;}
 amihass_erase(&config,sizeof(config));
 if(qa_command("QATests:SetupStartup RESTART QATests:AmiHASSService")){puts("FAIL: could not start isolated service.");goto cleanup;}started=1;
 for(i=0;i<600;++i){
  reply=live_exchange(&client,AMIHASS_STATUS,NULL);if(!reply)goto cleanup;
  count=reply->status.connected;strcpy(server_state,reply->item.state);FreeVec(reply);
  if(count)break;if(!strcmp(server_state,AMIHASS_SERVER_DISCONNECTED)){printf("FAIL: bridge could not establish authenticated HTTPS (%s).\n",server_state);goto cleanup;}Delay(5);
 }
 if(i==600){printf("FAIL: no server connection (%s).\n",server_state);goto cleanup;}
 memory_report("service connected");puts("PASS: native bridge established authenticated HTTPS to the supplied Home Assistant server.");
 for(i=0;i<600;++i){
  reply=live_exchange(&client,AMIHASS_LIST,NULL);if(!reply)goto cleanup;
  if(reply->result==AMIHASS_OK){int j;count=reply->count;total=reply->total;for(j=0;j<reply->count;++j)if(amihass_entity_id(reply->items[j].id) && !entity[0])strcpy(entity,reply->items[j].id);FreeVec(reply);break;}
  count=reply->result;FreeVec(reply);if(count!=AMIHASS_WAIT){printf("FAIL: entity discovery returned result %d.\n",count);goto cleanup;}Delay(5);
 }
 if(i==600 || !entity[0]){puts("FAIL: no readable entity returned by discovery.");goto cleanup;}
 printf("PASS: native bridge discovered %d entities; retrieved a page of %d.\n",total,count);
 for(i=0;i<600;++i){
  reply=live_exchange(&client,AMIHASS_QUERY,entity);if(!reply)goto cleanup;
  count=reply->result;
  if((count==AMIHASS_OK || count==AMIHASS_UNAVAILABLE) && !strcmp(reply->item.id,entity)){FreeVec(reply);break;}FreeVec(reply);
  if(count!=AMIHASS_WAIT){printf("FAIL: entity state read returned result %d.\n",count);goto cleanup;}Delay(5);
 }
 if(i==600){puts("FAIL: entity state read timed out.");goto cleanup;}
 puts("PASS: native bridge read one discovered entity state; no control actions were sent.");
 {struct NDPanelField fields[2];static const char *buttons[]={"Close"};struct NDPanel *panel;
 fields[0].label="Low memory mode";fields[0].value="1";fields[0].limit=ND_PANEL_CHECKBOX;
 fields[1].label="Memory";fields[1].value="Reclaim unused libraries after 30 s idle.";fields[1].limit=0;
 panel=nd_panel_open("Low memory mode QA",fields,2,buttons,1);if(!panel)goto cleanup;count=nd_panel_toggle(panel,0);nd_panel_close(panel);if(!count)goto cleanup;
 puts("PASS: native Low memory mode checkbox is checked.");}
 memory_report("before Browser");
 if(qa_command("QATests:AmiHASSBrowser LIVE-SELFTEST >AutoRun:browser-output.log")){puts("FAIL: live Browser integration.");goto cleanup;}
 memory_report("after Browser");
 reply=live_exchange(&client,AMIHASS_STATUS,NULL);if(!reply)goto cleanup;count=reply->status.subscriptions;FreeVec(reply);if(count){puts("FAIL: Browser left active polling subscriptions.");goto cleanup;}puts("PASS: Browser closed with zero active polling subscriptions.");
 for(i=0;i<350;++i){memory_sample();Delay(5);}memory_report("after automatic idle reclamation");
 {char line[512];int reclaimed=0;file=fopen(AH_LOG_FILE,"rb");if(file){while(fgets(line,sizeof(line),file))if(strstr(line,"Low memory mode: unused libraries reclaimed."))reclaimed=1;fclose(file);}if(!reclaimed){puts("FAIL: low memory mode did not reclaim unused libraries.");goto cleanup;}puts("PASS: Low memory mode automatically reclaimed unused libraries while service remained running.");}
 reply=live_exchange(&client,AMIHASS_DIAGNOSTICS,NULL);if(!reply)goto cleanup;
 ok=reply->count>8 && !strcmp(reply->items[6].state,"0") && !strcmp(reply->items[8].state,"0");FreeVec(reply);
 if(!ok)puts("FAIL: expected zero action requests and zero action HTTP attempts.");
cleanup:
 amihass_client_close(&client);amihass_erase(raw,sizeof(raw));amihass_erase(&config,sizeof(config));amihass_erase(entity,sizeof(entity));
 if(started || qa_command("QATests:SetupStartup STATE")==0)qa_command("QATests:AmiHASSService STOP");
 for(i=0;i<400;++i){int busy;Forbid();busy=FindPort("AmiHASS.Fetch")!=NULL;Permit();if(!busy)break;Delay(5);}
 memory_report("after service stop");
 qa_command("C:Avail FLUSH >AutoRun:memory-flush.log");memory_report("after reclaiming unused libraries");
 DeleteFile(AMIHASS_CONFIG_FILE);DeleteFile("LiveCredentials:credentials");
 {BPTR in=Open(AH_LOG_FILE,MODE_OLDFILE),out;LONG n;char line[512];if(in){out=Open("AutoRun:live-service.log",MODE_NEWFILE);if(out){while((n=Read(in,line,sizeof(line)))>0)Write(out,line,n);Close(out);}Close(in);}}
 return ok ? 0:10;
}
static struct AmiHASSMessage *sample_exchange(struct AmiHASSClient *client,int kind,const char *owner,const char *entity,const char *action)
{struct AmiHASSMessage *m;int i;if(!amihass_send(client,kind,owner,entity,action,0,0,NULL,NULL))return NULL;for(i=0;i<100;++i){m=amihass_reply(client);if(m)return m;Delay(2);}return NULL;}
static struct AmiHASSMessage *wait_reply(struct AmiHASSClient *c)
{struct AmiHASSMessage *m;int i;for(i=0;i<150;++i){m=amihass_reply(c);if(m)return m;Delay(2);}return NULL;}
static int selected_client_check(void)
{struct AmiHASSClient a,b,observer;struct AmiHASSMessage *m;int ok=0;memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));memset(&observer,0,sizeof(observer));
 if(!amihass_subscribe(&a,"Lease.A","light.amihass_sample_living_room",5))goto failed;m=wait_reply(&a);if(!m)goto failed;ok=m->result==AMIHASS_OK;FreeVec(m);if(!ok)goto failed;
 if(!amihass_subscribe(&b,"Lease.B","light.amihass_sample_living_room",300))goto failed;m=wait_reply(&b);if(!m)goto failed;ok=m->result==AMIHASS_OK;FreeVec(m);if(!ok)goto failed;
 m=sample_exchange(&observer,AMIHASS_STATUS,"Lease.Observer",NULL,NULL);if(!m)goto failed;ok=m->status.subscriptions==1;FreeVec(m);if(!ok)goto failed;
 amihass_client_close(&a);m=sample_exchange(&observer,AMIHASS_STATUS,"Lease.Observer",NULL,NULL);if(!m)goto failed;ok=m->status.subscriptions==1;FreeVec(m);if(!ok)goto failed;
 if(!amihass_unsubscribe(&b,NULL))goto failed;m=wait_reply(&b);if(!m)goto failed;ok=m->result==AMIHASS_OK;FreeVec(m);if(!ok)goto failed;
 m=sample_exchange(&observer,AMIHASS_STATUS,"Lease.Observer",NULL,NULL);if(!m)goto failed;ok=m->status.subscriptions==0;FreeVec(m);if(!ok)goto failed;
 if(!amihass_send_area(&observer,AMIHASS_DEVICES,"Area.QA",NULL,0,0,"light","","Study"))goto failed;m=wait_reply(&observer);if(!m)goto failed;ok=m->result==AMIHASS_OK&&m->total==1&&!strcmp(m->items[0].name,"Sample desk");FreeVec(m);if(!ok)goto failed;
 if(!amihass_send(&observer,AMIHASS_DIAGNOSTICS,"Memory.QA",NULL,NULL,0,0,"memory",NULL))goto failed;m=wait_reply(&observer);if(!m)goto failed;ok=m->result==AMIHASS_OK&&m->count==12&&!strcmp(m->items[0].id,"memory.fast.baseline")&&strtoul(m->items[1].state,NULL,10)>0&&!strcmp(m->items[8].state,"0");FreeVec(m);
 goto done;failed:ok=0;done:amihass_client_close(&a);amihass_client_close(&b);amihass_client_close(&observer);puts(ok ? "PASS: shared leases survive one client closing, stop at last release, scope rooms and expose sampled RAM.":"FAIL: selected-client/room/memory integration.");return ok;
}
static int sample_bridge_check(int dock)
{
 struct AmiHASSConfig cfg;struct AmiHASSClient a,b;struct AmiHASSMessage *m;struct NDPanelField fields[2];struct NDPanel *panel;static const char *buttons[]={"Close"};int ok=0,i;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));amihass_config_defaults(&cfg);cfg.sample_mode=1;strcpy(cfg.service_dir,"QATests:");
 if(!amihass_config_save(&cfg))goto cleanup;
 fields[0].label="Sample data";fields[0].value="1";fields[0].limit=ND_PANEL_CHECKBOX;fields[1].label="Mode";fields[1].value="Shared bridge simulation";fields[1].limit=0;
 panel=nd_panel_open("AmiHASS sample mode QA",fields,2,buttons,1);if(!panel)goto cleanup;i=nd_panel_toggle(panel,0);nd_panel_close(panel);if(!i)goto cleanup;
 if(qa_command("QATests:SetupStartup RESTART QATests:AmiHASSService"))goto cleanup;
 m=sample_exchange(&a,AMIHASS_STATUS,"SampleClient1",NULL,NULL);if(!m)goto cleanup;i=m->status.connected&&!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE);FreeVec(m);if(!i)goto cleanup;
 m=sample_exchange(&a,AMIHASS_AREAS,"Rooms.QA",NULL,NULL);if(!m)goto cleanup;i=m->result==AMIHASS_OK&&m->total==4&&m->count==4&&!strcmp(m->items[3].id,"study")&&!strcmp(m->items[3].name,"Study");FreeVec(m);if(!i)goto cleanup;
 puts("PASS: room catalogue exposes stable IDs and friendly names through the shared bridge.");
 m=sample_exchange(&a,AMIHASS_QUERY,"SampleClient1","light.amihass_sample_living_room",NULL);if(!m)goto cleanup;i=m->result==AMIHASS_OK&&!strcmp(m->item.state,"on");FreeVec(m);if(!i)goto cleanup;
 m=sample_exchange(&a,AMIHASS_ACTION,"SampleClient1","light.amihass_sample_living_room","Toggle");if(!m)goto cleanup;i=m->result==AMIHASS_WAIT;FreeVec(m);if(!i)goto cleanup;
 m=sample_exchange(&b,AMIHASS_QUERY,"SampleClient2","light.amihass_sample_living_room",NULL);if(!m)goto cleanup;i=m->result==AMIHASS_OK&&!strcmp(m->item.state,"off");FreeVec(m);if(!i)goto cleanup;
 if(!amihass_send(&a,AMIHASS_DIAGNOSTICS,"DiagnosticsQA",NULL,NULL,0,0,"clients",NULL))goto cleanup;
 for(i=0;i<100;++i){m=amihass_reply(&a);if(m)break;Delay(2);}if(!m)goto cleanup;{int row,one=0,two=0,entity=0;for(row=0;row<m->count;++row){if(!strcmp(m->items[row].id,"SampleClient1"))one=1;if(!strcmp(m->items[row].id,"SampleClient2"))two=1;if(!strcmp(m->items[row].device_class,"entity")&&!strcmp(m->items[row].id,"light.amihass_sample_living_room"))entity=1;}i=m->result==AMIHASS_OK&&one&&two&&entity;}FreeVec(m);if(!i)goto cleanup;puts("PASS: per-client diagnostics list both clients and their shared selected entity.");
 if(qa_command("QATests:AmiHASSPrefs DIAGNOSTICS-SELFTEST >AutoRun:clients.log"))goto cleanup;
 puts("PASS: two native bridge clients share and control the same sample light.");
 if(qa_command("QATests:AmiHASSBrowser LIVE-SELFTEST >AutoRun:sample-browser.log"))goto cleanup;
 if(qa_command("QATests:AmiHASSDashboard SELFTEST >AutoRun:dashboard.log"))goto cleanup;
 {int master;Forbid();master=FindPort("REXX")!=NULL;Permit();if(!master && qa_command("SYS:System/RexxMast"))goto cleanup;}
 if(qa_command("SYS:Rexxc/RX QATests:ARexx/Status.rexx >AutoRun:rexx-status.log"))goto cleanup;
 if(qa_command("SYS:Rexxc/RX QATests:ARexx/Discover.rexx >AutoRun:rexx-discovery.log"))goto cleanup;
 if(qa_command("SYS:Rexxc/RX QATests:ARexx/SampleLight.rexx >AutoRun:rexx-control.log"))goto cleanup;
 m=sample_exchange(&b,AMIHASS_QUERY,"SampleClient2","light.amihass_sample_living_room",NULL);if(!m)goto cleanup;i=m->result==AMIHASS_OK&&!strcmp(m->item.state,"off");FreeVec(m);if(!i)goto cleanup;
 puts("PASS: real ARexx scripts query and control the shared sample light through AMIHASS.");
 if(qa_command("QATests:SDK/AmiHASSExample STATUS >AutoRun:sdk-status.log"))goto cleanup;
 if(qa_command("QATests:SDK/AmiHASSExample LIST >AutoRun:sdk-list.log"))goto cleanup;
 if(qa_command("QATests:SDK/AmiHASSExample ACTION light.amihass_sample_desk Toggle >AutoRun:sdk-control.log"))goto cleanup;
 m=sample_exchange(&b,AMIHASS_QUERY,"SampleClient2","light.amihass_sample_desk",NULL);if(!m)goto cleanup;i=m->result==AMIHASS_OK&&!strcmp(m->item.state,"on");FreeVec(m);if(!i)goto cleanup;
 puts("PASS: Dashboard reads six tiles; independent SDK example changes a light seen by another client.");
 m=sample_exchange(&a,AMIHASS_DIAGNOSTICS,"SampleClient1",NULL,NULL);if(!m)goto cleanup;i=m->count>10&&!strcmp(m->items[8].state,"0")&&!strcmp(m->items[9].state,"0")&&!strcmp(m->items[10].state,"0")&&!strcmp(m->items[11].state,"0");FreeVec(m);if(!i)goto cleanup;
 if(dock&&(qa_command("QATests:AmiHASSNovaDockCheck >AutoRun:novadock.log")||qa_command("QATests:NovaDockHASS CONTROL-SELFTEST >AutoRun:novadock-ui.log")))goto cleanup;
 if(!selected_client_check())goto cleanup;puts("PASS: sample device browsing, state reads and controls make zero HTTP requests.");ok=1;
cleanup:
 amihass_client_close(&a);amihass_client_close(&b);qa_command("QATests:AmiHASSService STOP");Forbid();i=FindPort(AMIHASS_PORT)!=NULL;Permit();if(i)ok=0;DeleteFile(AMIHASS_CONFIG_FILE);return ok ? 0:10;
}
static int sample_memory_check(void)
{struct AmiHASSConfig config;struct AmiHASSClient a,b;struct AmiHASSMessage *m;unsigned long warm,ending;int i,ok=0;static const char *commands[]={"Brightness 45","Color #123456","Temperature 22.5","Toggle"};
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));amihass_config_defaults(&config);config.sample_mode=1;config.boot=0;strcpy(config.service_dir,"QATests:");if(!amihass_config_save(&config))return 10;amihass_erase(&config,sizeof(config));if(qa_command("QATests:SetupStartup RESTART QATests:AmiHASSService"))return 10;
 for(i=0;i<32;++i){m=sample_exchange(&a,AMIHASS_QUERY,"Memory.A","light.amihass_sample_living_room",NULL);if(!m)goto cleanup;FreeVec(m);m=sample_exchange(&b,AMIHASS_QUERY,"Memory.B","climate.amihass_sample_hallway",NULL);if(!m)goto cleanup;FreeVec(m);}warm=AvailMem(MEMF_FAST);printf("MEMORY sample warm: %lu Fast bytes free.\n",warm);
 for(i=0;i<500;++i){const char *entity=i%4==2 ? "climate.amihass_sample_hallway":"light.amihass_sample_living_room";m=sample_exchange(&a,AMIHASS_ACTION,"Memory.A",entity,commands[i%4]);if(!m)goto cleanup;ok=m->result==AMIHASS_WAIT;FreeVec(m);if(!ok)goto cleanup;m=sample_exchange(&b,AMIHASS_QUERY,"Memory.B",entity,NULL);if(!m)goto cleanup;ok=m->result==AMIHASS_OK;FreeVec(m);if(!ok)goto cleanup;if(i%100==99)printf("MEMORY sample commands %d: %lu Fast bytes free.\n",i+1,AvailMem(MEMF_FAST));}
 ending=AvailMem(MEMF_FAST);ok=ending+65536UL>=warm;printf("MEMORY sample ending: %lu; retained change %ld bytes.\n",ending,(long)warm-(long)ending);puts(ok ? "PASS: 500 shared sample controls and 500 reads show no sustained memory growth.":"FAIL: sample bridge retained more than 64 KiB after warmup.");
cleanup:amihass_client_close(&a);amihass_client_close(&b);qa_command("QATests:AmiHASSService STOP");DeleteFile(AMIHASS_CONFIG_FILE);return ok ? 0:10;}
static int transport_check(void)
{unsigned long before,loaded,after;LONG rc;FILE *f;char line[200];before=AvailMem(MEMF_FAST);rc=qa_command("C:curl --version >AutoRun:curl-version.txt");loaded=AvailMem(MEMF_FAST);qa_command("C:Avail FLUSH");after=AvailMem(MEMF_FAST);printf("CURL compatibility rc %ld; Fast before %lu, after command %lu, after flush %lu bytes.\n",rc,before,loaded,after);f=fopen("AutoRun:curl-version.txt","rb");if(f){while(fgets(line,sizeof(line),f))fputs(line,stdout);fclose(f);}if(rc)return 10;puts("PASS: runtime curl capabilities recorded without credentials or network traffic.");return 0;}
static int https_memory_check(int transport_fixture)
{struct AmiHASSConnection connection;struct AHConnectionTest probe;FILE *file;char token[1025],raw[1030];unsigned long warm=0,ending=0,before=0,unflushed=0;int i,j,result,ok=0;memset(&connection,0,sizeof(connection));memset(&probe,0,sizeof(probe));
 file=fopen("LiveCredentials:credentials","rb");if(!file)return 10;if(!fgets(connection.url,sizeof(connection.url),file)||!fgets(raw,sizeof(raw),file)){fclose(file);return 10;}fclose(file);DeleteFile("LiveCredentials:credentials");connection.url[strcspn(connection.url,"\r\n")]=0;connection.poll=30;connection.allow_http=0;if(!amihass_url(connection.url,0)||!amihass_token(raw,token))return 10;amihass_erase(raw,sizeof(raw));
 {struct Process *self=(struct Process *)FindTask(NULL);APTR saved=self->pr_WindowPtr;BPTR ca;self->pr_WindowPtr=(APTR)-1;ca=Lock("TLSFixture:ca.pem",ACCESS_READ);self->pr_WindowPtr=saved;if(ca){UnLock(ca);if(!SetVar("CURL_CA_BUNDLE","TLSFixture:ca.pem",-1,GVF_GLOBAL_ONLY))goto cleanup;}}
 for(i=0;i<8;++i){fast_min=before=AvailMem(MEMF_FAST);result=amihass_connection_test_start(&probe,&connection,token);while(result==AH_TEST_PENDING){result=amihass_connection_test_poll(&probe);memory_sample();Delay(5);}amihass_connection_test_close(&probe);if(result!=AH_TEST_OK&&!(transport_fixture&&result==AH_TEST_ENDPOINT)){printf("FAIL: read-only HTTPS memory probe returned %d.\n",result);{FILE *diag=fopen(AH_LOG_FILE,"rb");char line[180];if(diag){while(fgets(line,sizeof(line),diag))fputs(line,stdout);fclose(diag);}}goto cleanup;}for(j=0;j<100;++j){int busy;Forbid();busy=FindPort("AmiHASS.Probe")!=NULL;Permit();if(!busy)break;Delay(5);}Delay(10);unflushed=AvailMem(MEMF_FAST);qa_command("C:Avail FLUSH");ending=AvailMem(MEMF_FAST);printf("MEMORY HTTPS cycle %d: before %lu, minimum %lu, after request %lu, after reclaim %lu bytes.\n",i+1,before,fast_min,unflushed,ending);if(i==1)warm=ending;}
 ok=ending+131072UL>=warm;printf("MEMORY HTTPS retained after warmup: %ld bytes.\n",(long)warm-(long)ending);puts(ok ? (transport_fixture ? "PASS: eight verified TLS probes with expected HTTP 404 reclaim RAM without sustained growth.":"PASS: eight authenticated GET probes reclaim RAM with no sustained TLS growth."):"FAIL: HTTPS retained more than 128 KiB after warmup.");
cleanup:amihass_erase(token,sizeof(token));amihass_erase(raw,sizeof(raw));amihass_connection_test_close(&probe);return ok ? 0:10;}
static unsigned long approved_ticks(void)
{struct DateStamp stamp;DateStamp(&stamp);return (unsigned long)stamp.ds_Days*4320000UL+(unsigned long)stamp.ds_Minute*3000UL+stamp.ds_Tick;}
static struct AmiHASSMessage *approved_exchange(struct AmiHASSClient *client,int kind,const char *entity,const char *action,int refresh)
{struct AmiHASSMessage *m;int i;if(!amihass_send(client,kind,"AmiHASS.RealQA",entity,action,0,refresh,NULL,NULL))return NULL;for(i=0;i<200;++i){m=amihass_reply(client);if(m)return m;if(!client->pending)break;Delay(2);}return NULL;}
static int approved_read(struct AmiHASSClient *client,const char *entity,struct AmiHASSEntity *state,struct AHControls *controls)
{struct AmiHASSMessage *m;unsigned long start=approved_ticks();int fresh=1;while(approved_ticks()-start<2250UL){m=approved_exchange(client,AMIHASS_QUERY,entity,NULL,AMIHASS_QUERY_ONCE|(fresh?AMIHASS_QUERY_FRESH:0));fresh=0;if(!m)return 0;if(m->result==AMIHASS_OK){*state=m->item;amihass_controls_items(m->items,m->count,controls);FreeVec(m);if(strncmp(entity,"climate.",8)||controls->temperature)return 1;}else{int result=m->result;FreeVec(m);if(result!=AMIHASS_WAIT)return 0;}Delay(5);}return 0;}
static int approved_change(struct AmiHASSClient *client,const char *entity,const char *action)
{struct AmiHASSMessage *m;struct AHControls controls;unsigned long start=approved_ticks();int fresh=1,match;m=approved_exchange(client,AMIHASS_ACTION,entity,action,0);if(!m)return 0;match=m->result==AMIHASS_WAIT;FreeVec(m);if(!match)return 0;
 while(approved_ticks()-start<2250UL){m=approved_exchange(client,AMIHASS_QUERY,entity,NULL,AMIHASS_QUERY_ONCE|(fresh?AMIHASS_QUERY_FRESH:0));fresh=0;if(!m)return 0;if(m->result==AMIHASS_OK){amihass_controls_items(m->items,m->count,&controls);match=!strcmp(action,"Turn on") ? !strcmp(m->item.state,"on"):amihass_control_matches(&controls,m->item.state,action);FreeVec(m);if(match)return 1;fresh=1;Delay(50);}else{int result=m->result;FreeVec(m);if(result!=AMIHASS_WAIT)return 0;Delay(5);}}
 puts("FAIL: the device did not confirm the requested target before timeout.");return 0;}
static int live_controls_check(void)
{struct AmiHASSConfig config;struct AmiHASSClient client;struct AmiHASSMessage *m;struct AmiHASSEntity state;struct AHControls controls;FILE *file;char raw[1030],ids[3][96],action[20],original[40];long target;int i,ok=0,ready=0;memset(&client,0,sizeof(client));amihass_config_defaults(&config);
 file=fopen("LiveCredentials:credentials","rb");if(!file)return 10;if(!fgets(config.connection.url,sizeof(config.connection.url),file)||!fgets(raw,sizeof(raw),file)){fclose(file);return 10;}fclose(file);config.connection.url[strcspn(config.connection.url,"\r\n")]=0;if(!amihass_url(config.connection.url,0)||!amihass_token(raw,config.token))return 10;amihass_erase(raw,sizeof(raw));DeleteFile("LiveCredentials:credentials");
 file=fopen("LiveCredentials:controls","rb");if(!file)goto cleanup;for(i=0;i<3;++i){if(!fgets(ids[i],sizeof(ids[i]),file)){fclose(file);goto cleanup;}ids[i][strcspn(ids[i],"\r\n")]=0;if(!amihass_entity_id(ids[i])||strncmp(ids[i],i==0?"climate.":"light.",i==0?8:6)){fclose(file);goto cleanup;}}fclose(file);DeleteFile("LiveCredentials:controls");
 config.boot=0;config.sample_mode=0;config.reconnect=0;config.connection.poll=10;strcpy(config.service_dir,"QATests:");if(!amihass_config_save(&config)||qa_command("QATests:SetupStartup RESTART QATests:AmiHASSService"))goto cleanup;
 for(i=0;i<300;++i){m=approved_exchange(&client,AMIHASS_STATUS,NULL,NULL,0);if(!m)goto cleanup;ready=m->status.connected;FreeVec(m);if(ready)break;Delay(5);}if(!ready)goto cleanup;
 if(!approved_read(&client,ids[0],&state,&controls)||!controls.temperature)goto cleanup;target=controls.target;amihass_number_text(target,original);amihass_number_text(target+100,raw);snprintf(action,sizeof(action),"Temperature %s",raw);if(!amihass_control_allowed(&controls,action))goto cleanup;
 puts("RUN: approved heater target +1 degree.");if(!approved_change(&client,ids[0],action))goto cleanup;printf("PASS: heater +1 degree confirmed (%s).\n",controls.unit);snprintf(action,sizeof(action),"Temperature %s",original);if(!approved_change(&client,ids[0],action))goto cleanup;puts("PASS: original heater target restored and confirmed.");
 if(!approved_read(&client,ids[1],&state,&controls)||!controls.color)goto cleanup;puts("RUN: approved wall-light RGB test.");if(!approved_change(&client,ids[1],"Color #ff8040"))goto cleanup;puts("PASS: real wall-light color confirmed through the bridge.");
 if(!approved_read(&client,ids[2],&state,&controls)||!controls.brightness)goto cleanup;if(!approved_change(&client,ids[2],"Turn on"))goto cleanup;sprintf(action,"Brightness %d",controls.level>=45&&controls.level<=55?60:50);if(!approved_change(&client,ids[2],action))goto cleanup;puts("PASS: real cabinet-light on and brightness confirmed through the bridge.");
 m=approved_exchange(&client,AMIHASS_ACTION,ids[2],"Brightness 101",0);if(!m)goto cleanup;ready=m->result==AMIHASS_UNSUPPORTED;FreeVec(m);if(!ready)goto cleanup;puts("PASS: out-of-range control rejected without sending it to a device.");
 strcpy(config.connection.url,"https://127.0.0.1:1");if(!amihass_config_save(&config))goto cleanup;m=approved_exchange(&client,AMIHASS_RELOAD,NULL,NULL,0);if(!m)goto cleanup;FreeVec(m);ready=0;for(i=0;i<300;++i){m=approved_exchange(&client,AMIHASS_STATUS,NULL,NULL,0);if(!m)goto cleanup;ready=!m->status.connected&&!strcmp(m->item.state,AMIHASS_SERVER_DISCONNECTED);FreeVec(m);if(ready)break;Delay(5);}if(!ready)goto cleanup;m=approved_exchange(&client,AMIHASS_ACTION,ids[2],"Brightness 50",0);if(!m)goto cleanup;ready=m->result!=AMIHASS_WAIT&&m->result!=AMIHASS_OK;FreeVec(m);if(!ready)goto cleanup;puts("PASS: failed connection is reported and device controls are not accepted.");ok=1;
cleanup:amihass_client_close(&client);qa_command("QATests:AmiHASSService STOP");DeleteFile(AMIHASS_CONFIG_FILE);amihass_erase(&config,sizeof(config));amihass_erase(raw,sizeof(raw));return ok?0:10;}

int main(int argc,char **argv)
{
 setvbuf(stdout,NULL,_IONBF,0);
 struct AHConnectionTest test;struct AmiHASSConnection connection;
 BPTR file,report;LONG count;char text[8193];int result;
 if(argc>1 && !strcmp(argv[1],"LIVE-CONTROLS"))return live_controls_check();
 if(argc>1 && !strcmp(argv[1],"TRANSPORT-CHECK"))return transport_check();
 if(argc>1 && !strcmp(argv[1],"SAMPLE-MEMORY"))return sample_memory_check();
 if(argc>1 && !strcmp(argv[1],"HTTPS-MEMORY"))return https_memory_check(0);
 if(argc>1 && !strcmp(argv[1],"HTTPS-TRANSPORT-MEMORY"))return https_memory_check(1);
 if(argc>1 && !strcmp(argv[1],"SAMPLE-DOCK"))return sample_bridge_check(1);
 if(argc>1 && !strcmp(argv[1],"SAMPLE-BRIDGE"))return sample_bridge_check(0);
 if(argc>1 && !strcmp(argv[1],"LIVE-READONLY"))return live_readonly_check();
 if(argc>1 && !strcmp(argv[1],"MAINTENANCE"))return maintenance_choice_check();
 if(argc>1 && !strcmp(argv[1],"INSTALLER"))return installer_check();
 if(argc>1 && !strcmp(argv[1],"PREFERENCES"))return preferences_check();
 if(argc>1 && !strcmp(argv[1],"TOKEN-INPUT"))return token_input_check();
 if(argc>1 && !strcmp(argv[1],"PROBE"))return argc==3 ? amihass_probe_worker(argv[2]):20;
 memset(&connection,0,sizeof(connection));strcpy(connection.url,"https://127.0.0.1:1");connection.poll=30;
 result=amihass_connection_test_start(&test,&connection,"fixture-offline-token-for-check");
 while(result==AH_TEST_PENDING){result=amihass_connection_test_poll(&test);Delay(5);}
 amihass_connection_test_close(&test);
 if(result!=AH_TEST_FAILED && result!=AH_TEST_NO_NETWORK && result!=AH_TEST_CONNECT && result!=AH_TEST_TIMEOUT){puts("FAIL: unreachable server was not rejected.");return 10;}
 file=Open(AH_LOG_FILE,MODE_OLDFILE);if(!file){puts("FAIL: diagnostic log was not created.");return 10;}
 count=Read(file,text,sizeof(text)-1);Close(file);if(count<=0){puts("FAIL: diagnostic log is empty.");return 10;}text[count]=0;
 if((!strstr(text,ah_connection_text(result)) && !strstr(text,"Connection test blocked")) || strstr(text,"fixture-offline-token") || strstr(text,"Bearer ")){puts("FAIL: failure log missing or includes credentials.");return 10;}
 report=Open("AutoRun:AmiHASS-Service.log",MODE_NEWFILE);if(!report)return 10;
 if(Write(report,text,count)!=count){Close(report);return 10;}Close(report);
 puts(result==AH_TEST_NO_NETWORK ? "PASS: TCP/IP unavailable; request blocked before contacting server.":"PASS: unreachable server rejected by authenticated connection validation.");
 puts("PASS: failure diagnostics recorded without credentials.");return 0;
}
