/* Preferences keeps a draft until Save; Close never commits settings. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ah_locale.h"
#include "preferences.h"
#include "server_profiles.h"
#include "wait.h"
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
 struct AmiHASSConfig config;struct AmiHASSClient client;unsigned int stored_poll,stored_budget;int stored_low_memory,stored_sample,stored_push,stored_remote;
 if(!amihass_config_load(&config))return 0;stored_push=config.push_updates;stored_remote=config.remote_security;stored_poll=config.connection.poll;stored_budget=config.background_budget;stored_low_memory=config.low_memory;stored_sample=config.sample_mode;
 if(forget){config.connection.url[0]=0;amihass_erase(config.token,sizeof(config.token));}
 else{if(!amihass_url(connection->url,connection->allow_http) || !amihass_token(token,config.token)){amihass_erase(&config,sizeof(config));return 0;}strcpy(config.connection.url,connection->url);config.connection.allow_http=connection->allow_http;}
 config.reconnect=reconnect;config.retry_interval=retry_interval;
 if(!amihass_config_save(&config)){amihass_erase(&config,sizeof(config));return 0;}amihass_erase(&config,sizeof(config));
 if(forget || strcmp(prefs->connection.url,connection->url) || strcmp(prefs->token,token))prefs->connection_result=AH_TEST_PENDING;
 ah_log_event(forget ? AH_LOG_SERVER_REMOVED:AH_LOG_SERVER_SAVED,0);
 prefs->connection=*connection;if(forget)amihass_erase(prefs->token,sizeof(prefs->token));else strcpy(prefs->token,token);prefs->forget=forget;
 prefs->reconnect=reconnect;prefs->retry_interval=retry_interval;prefs->dirty=prefs->push_updates!=stored_push || prefs->background_budget!=stored_budget || prefs->connection.poll!=stored_poll || prefs->low_memory!=stored_low_memory || prefs->sample_mode!=stored_sample || prefs->remote_security!=stored_remote;
 memset(&client,0,sizeof(client));amihass_send(&client,AMIHASS_RELOAD,"AmiHASS.Prefs",NULL,NULL,0,0,NULL,NULL);amihass_client_close(&client);return 1;
}
int amihass_server_save(struct AmiHASSPreferences *prefs,const struct AmiHASSConnection *connection,const char *token,int forget)
{struct AmiHASSConfig config;int reconnect;unsigned int interval;if(!amihass_config_load(&config))return 0;reconnect=config.reconnect;interval=config.retry_interval;amihass_erase(&config,sizeof(config));return amihass_server_save_options(prefs,connection,token,forget,reconnect,interval);}
static int save_preferences(struct AmiHASSPreferences *prefs)
{
 if(prefs->dirty){
  struct AmiHASSConfig config;if(!amihass_config_load(&config))return 0;
  config.push_updates=prefs->push_updates;config.remote_security=prefs->remote_security;config.background_budget=prefs->background_budget;config.connection.poll=prefs->connection.poll;config.low_memory=prefs->low_memory;config.sample_mode=prefs->sample_mode;
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
 struct NDPanel *panel=NULL;struct NDPanelField fields[6];struct MsgPort *guard;
 char contact[160]="No successful contact",resources[160]="Waiting for service",server_name[33]="Server",server_url[160]="";int overview_due=0;
 char state[160],activity[160],remote_server[64]="Not Connected";int owner=0,action,done=0,j;unsigned long retry_remaining=0;unsigned long ticks=0,next_status=0,next_network=0,tick_clock=0;int network,previous_network=-1,configured,live,connected=0,connection_pending=0,previous_live=-1;
 guard=CreateMsgPort();if(!guard)return;
 Forbid();if(!FindPort("AmiHASS.Preferences")){guard->mp_Node.ln_Name="AmiHASS.Preferences";AddPort(guard);owner=1;}Permit();
 if(!owner){DeleteMsgPort(guard);return;}
 memset(&prefs,0,sizeof(prefs));memset(&client,0,sizeof(client));
 if(!amihass_connection_load(&prefs.connection,prefs.token)){strcpy(prefs.connection.url,"https://homeassistant.local:8123");if(prefs.connection.poll<10 || prefs.connection.poll>3600)prefs.connection.poll=30;}
 prefs.boot=prefs.saved_boot=boot_enabled();
 {struct AmiHASSConfig config;amihass_config_load(&config);if(config.recovered){prefs.dirty=1;amihass_recovery_notice("Bridge settings");}prefs.push_updates=config.push_updates;prefs.remote_security=config.remote_security;prefs.background_budget=config.background_budget;prefs.sample_mode=config.sample_mode;prefs.low_memory=config.low_memory;prefs.reconnect=config.reconnect;prefs.retry_interval=config.retry_interval;amihass_erase(&config,sizeof(config));}
 if(connection_page)amihass_connection_edit(&prefs);
 strcpy(state,"Checking service...");strcpy(activity,"Waiting for service status");
 fields[0].label="Service";fields[0].value=state;fields[0].limit=0;
 fields[1].label="Network";fields[1].value="Checking TCP/IP stack...";fields[1].limit=0;
 fields[2].label="Server";fields[2].value="Not Configured";fields[2].limit=0;
 fields[3].label="Activity";fields[3].value=activity;fields[3].limit=0;
 fields[4].label="Contact";fields[4].value=contact;fields[4].limit=0;fields[5].label="Resources";fields[5].value=resources;fields[5].limit=0;
 while(!done){
  ticks+=ah_elapsed100(&tick_clock);Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();
  if(live!=previous_live){connected=connection_pending=0;strcpy(remote_server,"Not Connected");amihass_client_close(&client);next_status=0;previous_live=live;}
  if(!panel){panel=nd_panel_preferences(fields,prefs.boot);if(!panel)break;nd_panel_set_source(panel,prefs.sample_mode);if(prefs.dirty || prefs.boot!=prefs.saved_boot)nd_panel_title(panel,"AmiHASS Preferences - unsaved changes");next_status=0;}
  if(previous_network<0 || ticks>=next_network){network=amihass_network_running();next_network=ticks+10;}else network=previous_network;configured=!prefs.forget && prefs.token[0] && amihass_url(prefs.connection.url,prefs.connection.allow_http);
  if(network!=previous_network){ah_log_event(network ? AH_LOG_NETWORK_READY:AH_LOG_NETWORK_DOWN,0);previous_network=network;}
  nd_panel_set(panel,1,network ? "TCP/IP stack running":"TCP/IP stack not running");
  nd_panel_set(panel,2,live && !strcmp(remote_server,"Sample data (simulated)") ? remote_server:!configured ? "Not Configured":!network ? "Not Connected":live ? remote_server:"Not Connected");
  nd_panel_enable(panel,104,1);
  if(overview_due && !client.pending){amihass_send(&client,AMIHASS_DIAGNOSTICS,"AmiHASS.Prefs",NULL,NULL,0,0,"overview",NULL);overview_due=0;}
  if(ticks>=next_status && !client.pending){amihass_send(&client,AMIHASS_STATUS,"AmiHASS.Prefs",NULL,NULL,0,0,NULL,NULL);next_status=ticks+10;}
  m=amihass_reply(&client);
  if(m){
   if(m->kind==AMIHASS_RECONNECT||m->kind==AMIHASS_DISCONNECT){connection_pending=0;next_status=0;if(m->result!=AMIHASS_OK)nd_panel_title(panel,"Connection request failed; check service version");}
   if(m->kind==AMIHASS_DIAGNOSTICS && m->result==AMIHASS_OK && m->count==6){
    snprintf(activity,sizeof(activity),"%.24s",m->items[0].state);if(!strcmp(m->items[0].state,"Discovery failed"))strcpy(activity,"Discovery failed; use Refresh");
    snprintf(resources,sizeof(resources),ah_txf("%s cached; %s req; %lukB%s"),m->items[1].state,m->items[4].state,strtoul(m->items[3].state,NULL,10)/1024UL,!strcmp(m->items[5].state,"1") ? ah_tx(" Low"):"");nd_panel_set(panel,3,activity);nd_panel_set(panel,5,resources);
   }
   if(m->kind==AMIHASS_STATUS){
    overview_due=1;
    if(strcmp(server_url,m->status.server)){struct AHServerProfiles *servers=AllocVec(sizeof(*servers),MEMF_PUBLIC|MEMF_CLEAR);unsigned int k;strncpy(server_url,m->status.server,sizeof(server_url)-1);strcpy(server_name,"Server");if(servers){if(ah_servers_load(servers,AH_SERVERS_FILE,AH_SERVERS_TOKENS))for(k=0;k<servers->count;++k)if(!strcmp(servers->items[k].url,server_url)){strcpy(server_name,servers->items[k].name);break;}ah_servers_erase(servers);FreeVec(servers);}}
    if(!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE))strcpy(contact,"Sample data - local");else if(m->status.last_contact)snprintf(contact,sizeof(contact),ah_txf("%.14s: %lus ago"),server_name,(unsigned long)m->age);else snprintf(contact,sizeof(contact),ah_txf("%.14s: never"),server_name);nd_panel_set(panel,4,contact);

    connected=m->status.connected!=0;
    strcpy(remote_server,!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE) ? "Sample data (simulated)":m->status.connected ? "Connected":!m->status.server[0] ? "Not Configured":!strcmp(m->item.state,AMIHASS_SERVER_PAUSED) ? "Disconnected":!strcmp(m->item.state,AMIHASS_SERVER_AUTH_REJECTED) ? "Token rejected - check Server Settings":!strcmp(m->item.state,AMIHASS_SERVER_CONNECTING) ? "Connecting":!strcmp(m->item.state,AMIHASS_SERVER_RETRY_WAIT) ? "Not Connected (retry)":!strcmp(m->item.state,AMIHASS_SERVER_DISCONNECTED) ? "Connection Failure":"Not Connected");
    retry_remaining=0;for(j=0;j<m->count;++j)if(!strcmp(m->items[j].id,"retry.remaining"))sscanf(m->items[j].state,"%lu",&retry_remaining);
    if(!strcmp(m->item.state,AMIHASS_SERVER_RETRY_WAIT)&&retry_remaining)snprintf(remote_server,sizeof(remote_server),ah_txf("Retry in %lum %lus"),retry_remaining/60,retry_remaining%60);
    nd_panel_set(panel,2,(!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE) || network) ? remote_server:"Not Connected");strcpy(state,"Running");nd_panel_set(panel,0,state);}FreeVec(m);
  }
  if(!client.pending)connection_pending=0;
  Forbid();live=FindPort(AMIHASS_PORT)!=NULL;Permit();
  nd_panel_service_state(panel,live,0);
  if(!live){nd_panel_set(panel,4,"No service contact");nd_panel_set(panel,5,"Service stopped");strcpy(state,"Stopped");strcpy(activity,"No service running");nd_panel_set(panel,0,state);nd_panel_set(panel,3,activity);}
  else if(strcmp(state,"Running")){strcpy(state,"Running");strcpy(activity,"Waiting for service status");nd_panel_set(panel,0,state);nd_panel_set(panel,3,activity);}
  nd_panel_connection_state(panel,connected,live && configured && !prefs.sample_mode && !connection_pending && (connected||network));
  action=nd_panel_event(panel);prefs.boot=nd_panel_checked(panel);
  if(action==113 && prefs.sample_mode!=nd_panel_source(panel)){prefs.sample_mode=nd_panel_source(panel);prefs.dirty=1;nd_panel_title(panel,"AmiHASS Preferences - unsaved changes");}
  if(action==-1 || action==105)done=1;
  else if(action==100 && !live){if(!amihass_start(NULL)){nd_panel_title(panel,"Could not start service; check installation");nd_panel_beep(panel);}}
  else if(action==101 && live){amihass_client_close(&client);if(!amihass_send(&client,AMIHASS_STOP,"AmiHASS.Prefs",NULL,NULL,0,0,NULL,NULL))nd_panel_set(panel,0,"Service is stopped");}
  else if(action==115 && !connected && live && configured && !prefs.sample_mode && !connection_pending && (connected||network)){int command=connected ? AMIHASS_DISCONNECT:AMIHASS_RECONNECT;amihass_client_close(&client);if(amihass_send(&client,command,"AmiHASS.Prefs",NULL,NULL,0,0,NULL,NULL)){connected=0;connection_pending=1;strcpy(remote_server,command==AMIHASS_DISCONNECT ? "Disconnected":"Connecting");next_status=0;}else nd_panel_title(panel,"Could not change connection state");}
  else if(action==116){amihass_client_close(&client);nd_panel_close(panel);panel=NULL;amihass_health_ui();}
  else if(action==110){if(!amihass_launch("AmiHASSBrowser",NULL)){nd_panel_title(panel,"Could not open AmiHASS Browser; check installation");nd_panel_beep(panel);}}
  else if(action==102 || action==103 || action==107 || action==108 || action==109){
amihass_client_close(&client);nd_panel_close(panel);panel=NULL;
   if(action==102){amihass_connection_edit(&prefs);server_url[0]=0;}else if(action==103)amihass_general_edit(&prefs);else if(action==107)amihass_logs_ui();else if(action==108)amihass_about_ui();else amihass_diagnostics_ui();
  }
  else if(action==104){

   if(commit_preferences(&prefs,&client)){done=1;continue;}
   nd_panel_title(panel,"Save failed; check settings and startup files");nd_panel_beep(panel);
  }
  else if(action==106)nd_panel_title(panel,prefs.dirty || prefs.boot!=prefs.saved_boot ? "AmiHASS Preferences - unsaved changes":"AmiHASS Preferences");
  /* Status and network are polled once a second; between those, sleep
   * until input or the status reply instead of waking every 100 ms. */
  if(panel)nd_panel_wait(panel,amihass_client_signal(&client),client.pending ? 50:(next_status>ticks ? (next_status-ticks)*5:1));
 }
 if(panel)nd_panel_close(panel);amihass_client_close(&client);amihass_erase(&prefs,sizeof(prefs));
 Forbid();RemPort(guard);Permit();DeleteMsgPort(guard);
}
