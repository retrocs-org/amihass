/* Preferences keeps a draft until Save; Close never commits settings. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "preferences.h"
#include "amihass_config.h"
#include "ah_log.h"
#include "connectivity.h"
#include "nd_panel.h"
#include "nd_interaction.h"
static int path_exists(const char *path)
{BPTR lock=Lock((STRPTR)path,ACCESS_READ);if(!lock)return 0;UnLock(lock);return 1;}
static int boot_enabled(void)
{struct AmiHASSConfig config;int enabled;amihass_config_load(&config);enabled=config.boot;amihass_erase(&config,sizeof(config));return enabled;}
static int boot_apply(int enabled)
{
 char directory[520],helper[540],program[540],qh[1100],qp[1100],command[2300];BPTR input,output;LONG result;
 {struct AmiHASSConfig config;if(!amihass_config_load(&config))return 0;strcpy(directory,config.service_dir);amihass_erase(&config,sizeof(config));}
 if(!directory[0] && (!GetProgramDir() || !NameFromLock(GetProgramDir(),directory,sizeof(directory))))return 0;
 strcpy(helper,directory);strcpy(program,directory);
 if(!AddPart(helper,"SetupStartup",sizeof(helper)) || !AddPart(program,"AmiHASSService",sizeof(program)) || !path_exists(helper) || !path_exists(program))return 0;
 if(!nd_quote_path(qh,sizeof(qh),helper) || !nd_quote_path(qp,sizeof(qp),program))return 0;
 sprintf(command,"%s %s %s",qh,qp,enabled ? "ON":"OFF");
 input=Open("NIL:",MODE_OLDFILE);output=Open("NIL:",MODE_NEWFILE);
 if(!input || !output){if(input)Close(input);if(output)Close(output);return 0;}
 result=SystemTags(command,SYS_Input,input,SYS_Output,output,NP_StackSize,65536UL,TAG_DONE);
 Close(input);Close(output);return result==0;
}
/* Server Settings Save commits only server fields; other draft edits remain. */
int amihass_server_save_options(struct AmiHASSPreferences *prefs,const struct AmiHASSConnection *connection,const char *token,int forget,int reconnect,unsigned int retry_interval)
{
 struct AmiHASSConfig config;struct AmiHASSClient client;unsigned int stored_poll;int stored_low_memory,stored_sample;
 if(!amihass_config_load(&config))return 0;stored_poll=config.connection.poll;stored_low_memory=config.low_memory;stored_sample=config.sample_mode;
 if(forget){config.connection.url[0]=0;amihass_erase(config.token,sizeof(config.token));}
 else{if(!amihass_url(connection->url,connection->allow_http) || !amihass_token(token,config.token)){amihass_erase(&config,sizeof(config));return 0;}strcpy(config.connection.url,connection->url);config.connection.allow_http=connection->allow_http;}
 config.reconnect=reconnect;config.retry_interval=retry_interval;
 if(!amihass_config_save(&config)){amihass_erase(&config,sizeof(config));return 0;}amihass_erase(&config,sizeof(config));
 if(forget || strcmp(prefs->connection.url,connection->url) || strcmp(prefs->token,token))prefs->connection_result=AH_TEST_PENDING;
 ah_log_event(forget ? AH_LOG_SERVER_REMOVED:AH_LOG_SERVER_SAVED,0);
 prefs->connection=*connection;if(forget)amihass_erase(prefs->token,sizeof(prefs->token));else strcpy(prefs->token,token);prefs->forget=forget;
 prefs->reconnect=reconnect;prefs->retry_interval=retry_interval;prefs->dirty=prefs->connection.poll!=stored_poll || prefs->low_memory!=stored_low_memory || prefs->sample_mode!=stored_sample;
 memset(&client,0,sizeof(client));amihass_send(&client,AMIHASS_RELOAD,"AmiHASS.Prefs",NULL,NULL,0,0,NULL,NULL);amihass_client_close(&client);return 1;
}
int amihass_server_save(struct AmiHASSPreferences *prefs,const struct AmiHASSConnection *connection,const char *token,int forget)
{struct AmiHASSConfig config;int reconnect;unsigned int interval;if(!amihass_config_load(&config))return 0;reconnect=config.reconnect;interval=config.retry_interval;amihass_erase(&config,sizeof(config));return amihass_server_save_options(prefs,connection,token,forget,reconnect,interval);}
static int save_preferences(struct AmiHASSPreferences *prefs)
{
 if(prefs->dirty){
  struct AmiHASSConfig config;if(!amihass_config_load(&config))return 0;
  config.connection.poll=prefs->connection.poll;config.low_memory=prefs->low_memory;config.sample_mode=prefs->sample_mode;
  if(prefs->forget){config.connection.url[0]=0;amihass_erase(config.token,sizeof(config.token));}
  else if(prefs->token[0]){config.connection=prefs->connection;if(!amihass_token(prefs->token,config.token)){amihass_erase(&config,sizeof(config));return 0;}}
  if(!amihass_config_save(&config)){amihass_erase(&config,sizeof(config));return 0;}amihass_erase(&config,sizeof(config));
  prefs->dirty=0;
 }
 if(prefs->boot!=prefs->saved_boot){
  if(!boot_apply(prefs->boot))return 0;
  prefs->saved_boot=prefs->boot;
 }
 return 1;
}
static int commit_preferences(struct AmiHASSPreferences *prefs,struct AmiHASSClient *client)
{
 int reload=prefs->dirty,saved=save_preferences(prefs);
 if(!saved)ah_log_event(AH_LOG_SAVE_FAILED,0);
 if(reload && !prefs->dirty){amihass_client_close(client);amihass_send(client,AMIHASS_RELOAD,"AmiHASS.Prefs",NULL,NULL,0,0,NULL,NULL);}
 return saved;
}
void amihass_preferences_ui(int connection_page)
{
 struct AmiHASSPreferences prefs;struct AmiHASSClient client;struct AmiHASSMessage *m;
 struct NDPanel *panel=NULL;struct NDPanelField fields[4];struct MsgPort *guard;
 char state[160],activity[160],remote_server[64]="Not Connected";int owner=0,action,n=0,done=0,network,previous_network=-1,configured,ticks=0,live,previous_live=-1;
 guard=CreateMsgPort();if(!guard)return;
 Forbid();if(!FindPort("AmiHASS.Preferences")){guard->mp_Node.ln_Name="AmiHASS.Preferences";AddPort(guard);owner=1;}Permit();
 if(!owner){DeleteMsgPort(guard);return;}
 memset(&prefs,0,sizeof(prefs));memset(&client,0,sizeof(client));
 if(!amihass_connection_load(&prefs.connection,prefs.token)){strcpy(prefs.connection.url,"https://homeassistant.local:8123");if(prefs.connection.poll<10 || prefs.connection.poll>3600)prefs.connection.poll=30;}
 prefs.boot=prefs.saved_boot=boot_enabled();
 {struct AmiHASSConfig config;amihass_config_load(&config);prefs.sample_mode=config.sample_mode;prefs.low_memory=config.low_memory;prefs.reconnect=config.reconnect;prefs.retry_interval=config.retry_interval;amihass_erase(&config,sizeof(config));}
 if(connection_page)amihass_connection_edit(&prefs);
 strcpy(state,"Checking service...");strcpy(activity,"Waiting for service status");
 fields[0].label="Service";fields[0].value=state;fields[0].limit=0;
 fields[1].label="Network";fields[1].value="Checking TCP/IP stack...";fields[1].limit=0;
 fields[2].label="Server";fields[2].value="Not Configured";fields[2].limit=0;
 fields[3].label="Activity";fields[3].value=activity;fields[3].limit=0;
 while(!done){
  ++ticks;Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();
  if(live!=previous_live){strcpy(remote_server,"Not Connected");amihass_client_close(&client);n=0;previous_live=live;}
  if(!panel){panel=nd_panel_preferences(fields,prefs.boot);if(!panel)break;nd_panel_set_source(panel,prefs.sample_mode);if(prefs.dirty || prefs.boot!=prefs.saved_boot)nd_panel_title(panel,"AmiHASS Preferences - unsaved changes");n=0;}
  network=previous_network<0 || !(ticks%10) ? amihass_network_running():previous_network;configured=!prefs.forget && prefs.token[0] && amihass_url(prefs.connection.url,prefs.connection.allow_http);
  if(network!=previous_network){ah_log_event(network ? AH_LOG_NETWORK_READY:AH_LOG_NETWORK_DOWN,0);previous_network=network;}
  nd_panel_set(panel,1,network ? "TCP/IP stack running":"TCP/IP stack not running");
  nd_panel_set(panel,2,live && !strcmp(remote_server,"Sample data (simulated)") ? remote_server:!configured ? "Not Configured":!network ? "Not Connected":live ? remote_server:"Not Connected");
  nd_panel_enable(panel,104,1);
  if(!(n++%10) && !client.pending)amihass_send(&client,AMIHASS_STATUS,"AmiHASS.Prefs",NULL,NULL,0,0,NULL,NULL);
  m=amihass_reply(&client);
  if(m){
   if(m->kind==AMIHASS_STATUS){
    strcpy(remote_server,!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE) ? "Sample data (simulated)":m->status.connected ? "Connected":!m->status.server[0] ? "Not Configured":!strcmp(m->item.state,AMIHASS_SERVER_CONNECTING) ? "Connecting":!strcmp(m->item.state,AMIHASS_SERVER_RETRY_WAIT) ? "Not Connected (retry)":!strcmp(m->item.state,AMIHASS_SERVER_DISCONNECTED) ? "Connection Failure":"Not Connected");
    nd_panel_set(panel,2,(!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE) || network) ? remote_server:"Not Connected");strcpy(state,"Running");sprintf(activity,"%ld entities; %ld queued",(long)m->status.subscriptions,(long)m->status.queued);nd_panel_set(panel,0,state);nd_panel_set(panel,3,activity);}FreeVec(m);
  }
  Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();
  nd_panel_service_state(panel,live,0);
  if(!live){strcpy(state,"Stopped");strcpy(activity,"No service running");nd_panel_set(panel,0,state);nd_panel_set(panel,3,activity);}
  else if(strcmp(state,"Running")){strcpy(state,"Running");strcpy(activity,"Waiting for service status");nd_panel_set(panel,0,state);nd_panel_set(panel,3,activity);}
  action=nd_panel_event(panel);prefs.boot=nd_panel_checked(panel);
  if(action==113 && prefs.sample_mode!=nd_panel_source(panel)){prefs.sample_mode=nd_panel_source(panel);prefs.dirty=1;nd_panel_title(panel,"AmiHASS Preferences - unsaved changes");}
  if(action==-1 || action==105)done=1;
  else if(action==100 && !live){if(!amihass_start(NULL)){nd_panel_title(panel,"Could not start service; check installation");nd_panel_beep(panel);}}
  else if(action==101 && live){amihass_client_close(&client);if(!amihass_send(&client,AMIHASS_STOP,"AmiHASS.Prefs",NULL,NULL,0,0,NULL,NULL))nd_panel_set(panel,0,"Service is stopped");}
  else if(action==110){if(!amihass_launch("AmiHASSBrowser",NULL)){nd_panel_title(panel,"Could not open AmiHASS Browser; check installation");nd_panel_beep(panel);}}
  else if(action==102 || action==103 || action==107 || action==108 || action==109){
amihass_client_close(&client);nd_panel_close(panel);panel=NULL;
   if(action==102)amihass_connection_edit(&prefs);else if(action==103)amihass_general_edit(&prefs);else if(action==107)amihass_logs_ui();else if(action==108)amihass_about_ui();else amihass_diagnostics_ui();
  }
  else if(action==104){

   if(commit_preferences(&prefs,&client)){done=1;continue;}
   nd_panel_title(panel,"Save failed; check settings and startup files");nd_panel_beep(panel);
  }
  else if(action==106)nd_panel_title(panel,prefs.dirty || prefs.boot!=prefs.saved_boot ? "AmiHASS Preferences - unsaved changes":"AmiHASS Preferences");
  Delay(5);
 }
 if(panel)nd_panel_close(panel);amihass_client_close(&client);amihass_erase(&prefs,sizeof(prefs));
 Forbid();RemPort(guard);Permit();DeleteMsgPort(guard);
}
