"""Exercise the real Preferences controller and editors with scripted gadgets."""
from pathlib import Path
import os, subprocess, tempfile
ROOT = Path(__file__).resolve().parents[1]
ui = (ROOT / 'src/ui.c').read_text()
ui=ui[:ui.index('/* Local availability checks')]+ui[ui.index('int amihass_general_edit('):]
editors = ui[ui.index('/* Connection drafts'):ui.index('void amihass_connection_ui(')]
controller = (ROOT / 'src/preferences.c').read_text()
controller = controller[controller.index('int amihass_server_save_options('):]
panel_source=(ROOT/'src/panel.c').read_text()
at=panel_source.index('void nd_panel_service_state(');end=panel_source.index('}',at)+1
service_controls=panel_source[at:end]
prelude = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preferences.h"
#include "amihass_config.h"
#include "nd_panel.h"
#include "server_profiles.h"
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
typedef unsigned long BPTR;
static void *AllocVec(unsigned long n,unsigned long f){(void)f;return calloc(1,n);}
static BPTR CreateDir(const char *p){(void)p;return 1;}
static void UnLock(BPTR l){(void)l;}
static struct AHServerProfiles saved_profiles;
int ah_servers_load(struct AHServerProfiles *p,const char *f,const char *t){(void)f;(void)t;*p=saved_profiles;return 1;}
int ah_servers_save(const struct AHServerProfiles *p,const char *f,const char *t){(void)f;(void)t;saved_profiles=*p;return 1;}
#include "ah_log.h"
static void dependencies_ui(void){}
void amihass_recovery_notice(const char *what);
void ah_log_event(enum AHLogEvent event,long code){(void)event;(void)code;}
#include "connectivity.h"
#define TRUE 1
struct NDPanel {unsigned int choice;char values[12][1025];int boot,kind,reconnect,source;unsigned int retry_interval;};
static struct NDPanel panel;
static struct MsgPort guard;
struct Event {int action,boot;const char *url,*http,*poll;};
static struct Event events[30];static int at,count,writes,deletes,boots,starts,reloads,beeps,fail_write,fail_boot,disk_config,persisted_boot,network,probe_result,probe_calls,probe_complete,log_views,live,stops,start_enabled,stop_enabled,save_enabled,reconnect_enabled;
static int notices,stored_recovered;static unsigned int stored_budget;static int stored_sample,sample_toggle_override=-1,stored_low_memory,toggle_override=-1;static int stored_reconnect=1;static unsigned int stored_retry=60;
static struct AmiHASSConnection stored;
static char stored_token[1025],tested_token[1025];
static int overview_fixture,contact_fixture;static const char *reply_state;static unsigned long retry_seconds;static int reconnects,disconnects;
int amihass_launch(const char *name,const char *argument){assert(!strcmp(name,"AmiHASSBrowser")&&!argument);++log_views;return 1;}
void amihass_logs_ui(void){++log_views;}
void amihass_about_ui(void){++log_views;}
void amihass_diagnostics_ui(void){++log_views;}
void amihass_health_ui(void){++log_views;}
static char displayed_server[160],displayed_network[160],connection_status[160],displayed_contact[160],displayed_resources[160];
int amihass_network_running(void){return network;}
int amihass_connection_test_start(struct AHConnectionTest *t,const struct AmiHASSConnection *c,const char *token){(void)c;(void)token;++probe_calls;memset(t,0,sizeof(*t));if(!network)return AH_TEST_NO_NETWORK;t->port=(struct MsgPort *)1;return AH_TEST_PENDING;}
int amihass_connection_test_poll(struct AHConnectionTest *t){t->port=NULL;++probe_complete;return network ? probe_result:AH_TEST_NO_NETWORK;}
void amihass_connection_test_close(struct AHConnectionTest *t){memset(t,0,sizeof(*t));}
int amihass_connection_test_dialog(const struct AmiHASSConnection *c,const char *token,struct NDPanel *p,int cancel){(void)c;(void)p;(void)cancel;strcpy(tested_token,token);++probe_calls;return network ? probe_result:AH_TEST_NO_NETWORK;}
const char *amihass_server_status(int configured,int net,int result){if(!configured)return "Not Configured";if(!net)return "Not Connected";if(result==AH_TEST_OK)return "Connected";return result==AH_TEST_FAILED ? "Connection Failure":"Not Connected";}
void nd_panel_connection_state(struct NDPanel *p,int connected,int enabled);
void nd_panel_enable(struct NDPanel *p,unsigned int id,int enabled){(void)p;if(id==100)start_enabled=enabled;if(id==101)stop_enabled=enabled;if(id==104)save_enabled=enabled;if(id==115)reconnect_enabled=enabled;}
void nd_panel_connection_state(struct NDPanel *p,int connected,int enabled){nd_panel_enable(p,115,enabled && !connected);}


static void Forbid(void){}static void Permit(void){}static void Delay(int n){(void)n;}
/* Production loops wait on signals; in tests this is one scripted Delay step. */
void nd_panel_wait(struct NDPanel *p,unsigned long e,unsigned long t){(void)p;(void)e;(void)t;Delay(5);}
unsigned long ah_elapsed100(unsigned long *stamp){(void)stamp;return 1;} /* one loop step = 1/10 s, as before */
unsigned long amihass_client_signal(const struct AmiHASSClient *c){(void)c;return 0;}

static struct MsgPort *CreateMsgPort(void){return &guard;}
static void DeleteMsgPort(struct MsgPort *p){(void)p;}
static struct MsgPort *FindPort(const char *name){return live&&!strcmp(name,AMIHASS_PORT) ? &guard:NULL;}
static void AddPort(struct MsgPort *p){(void)p;}static void RemPort(struct MsgPort *p){(void)p;}
static void FreeVec(void *p){free(p);}
static int path_exists(const char *p){assert(!strcmp(p,"ENVARC:AmiHASS/Private/connection"));return disk_config;}
static int DeleteFile(const char *p){assert(path_exists(p));++deletes;disk_config=0;return 1;}
static int boot_enabled(void){return persisted_boot;}
static int boot_apply(int enabled){++boots;if(fail_boot)return 0;persisted_boot=enabled;return 1;}
void amihass_erase(void *p,unsigned long n){memset(p,0,n);}
int amihass_connection_load(struct AmiHASSConnection *c,char *token){if(!disk_config)return 0;*c=stored;strcpy(token,stored_token);return 1;}
int amihass_connection_save(const struct AmiHASSConnection *c,const char *token){char normalized[1025];if(fail_write || !amihass_url(c->url,c->allow_http) || !amihass_token(token,normalized) || c->poll<10 || c->poll>3600)return 0;++writes;stored=*c;strcpy(stored_token,normalized);disk_config=1;return 1;}
int amihass_config_load(struct AmiHASSConfig *c){memset(c,0,sizeof(*c));c->recovered=stored_recovered;c->background_budget=stored_budget;c->sample_mode=stored_sample;c->low_memory=stored_low_memory;c->reconnect=stored_reconnect;c->retry_interval=stored_retry;c->connection=stored;c->boot=persisted_boot;if(disk_config)strcpy(c->token,stored_token);else {c->connection.url[0]=0;c->connection.poll=30;}return 1;}
int amihass_config_save(const struct AmiHASSConfig *c){if(fail_write)return 0;if(disk_config&&!c->token[0])++deletes;else ++writes;stored_budget=c->background_budget;stored=c->connection;stored_sample=c->sample_mode;stored_low_memory=c->low_memory;stored_reconnect=c->reconnect;stored_retry=c->retry_interval;strcpy(stored_token,c->token);disk_config=!!c->token[0];return 1;}
int amihass_config_forget(void){if(fail_write)return 0;++deletes;disk_config=0;return 1;}
int amihass_send(struct AmiHASSClient *c,int kind,const char *o,const char *e,const char *a,int offset,int refresh,const char *domain,const char *filter){(void)c;(void)o;(void)e;(void)a;(void)offset;(void)refresh;(void)domain;(void)filter;if(kind==AMIHASS_RELOAD)++reloads;if(kind==AMIHASS_RECONNECT){++reconnects;return 1;}if(kind==AMIHASS_DISCONNECT){++disconnects;reply_state=AMIHASS_SERVER_PAUSED;return 1;}if(kind==AMIHASS_STOP){++stops;live=0;return 1;}return 0;}
struct AmiHASSMessage *amihass_reply(struct AmiHASSClient *c){struct AmiHASSMessage *m;(void)c;if(!reply_state||!live)return NULL;m=calloc(1,sizeof(*m));if(overview_fixture){int k;m->kind=AMIHASS_DIAGNOSTICS;m->result=AMIHASS_OK;m->count=6;for(k=0;k<6;++k)strcpy(m->items[k].state,k==0 ? "Ready":k==1 ? "4":k==3 ? "4096":"0");return m;}m->kind=AMIHASS_STATUS;m->count=1;strcpy(m->items[0].id,"retry.remaining");sprintf(m->items[0].state,"%lu",retry_seconds);strcpy(m->status.server,stored.url);strcpy(m->item.state,reply_state);m->status.connected=!strcmp(reply_state,AMIHASS_SERVER_CONNECTED);if(contact_fixture){m->status.last_contact=100;m->age=5;}return m;}
void amihass_client_close(struct AmiHASSClient *c){memset(c,0,sizeof(*c));}
int amihass_start(const char *p){(void)p;++starts;live=1;return 1;}
static int token_file(char *token){strcpy(token,"fixture-imported-token-for-testing");return 1;}
int nd_panel_toggle(struct NDPanel *p,unsigned int index){return index==4 ? (sample_toggle_override>=0 ? sample_toggle_override:p->values[index][0]=='1'):(toggle_override>=0 ? toggle_override:p->values[index][0]=='1');}
void amihass_recovery_notice(const char *what){assert(!strcmp(what,"Bridge settings"));++notices;}
struct NDPanel *nd_panel_open(const char *title,const struct NDPanelField *fields,unsigned int n,const char **buttons,unsigned int b){unsigned int i;(void)buttons;(void)b;memset(&panel,0,sizeof(panel));panel.kind=strstr(title,"Server") ? 1:2;for(i=0;i<n;++i)strcpy(panel.values[i],fields[i].value);return &panel;}
struct NDPanel *nd_panel_preferences(const struct NDPanelField *fields,int boot){(void)fields;memset(&panel,0,sizeof(panel));panel.boot=boot;return &panel;}
struct NDPanel *nd_panel_server_settings(const struct NDPanelField *fields,int reconnect,unsigned int retry_interval){struct NDPanel *p;assert(fields[1].limit==1025);p=nd_panel_open("Server",fields,4,NULL,6);p->reconnect=reconnect;p->retry_interval=retry_interval;return p;}
struct NDPanel *nd_panel_server_editor(const struct NDPanelField *fields,const char **names,unsigned int n,unsigned int selected,int reconnect,unsigned int retry_interval){struct NDPanel *p;(void)names;assert(n>=1&&n<=8);assert(fields[1].limit==1025&&fields[4].limit==33);p=nd_panel_open("Server",fields,5,NULL,6);p->choice=selected;p->reconnect=reconnect;p->retry_interval=retry_interval;return p;}
void nd_panel_servers(struct NDPanel *p,const char **names,unsigned int n,unsigned int selected){(void)names;assert(n<=8);p->choice=selected;}
unsigned int nd_panel_server_selected(struct NDPanel *p){return p->choice;}
int nd_panel_reconnect(struct NDPanel *p){return p->reconnect;}
unsigned int nd_panel_retry(struct NDPanel *p){return p->retry_interval;}
void nd_panel_set_source(struct NDPanel *p,int sample){p->source=sample;}
int nd_panel_source(struct NDPanel *p){return p->source;}
void nd_panel_close(struct NDPanel *p){(void)p;}
static void servers_dialog(struct NDPanel *p){(void)p;}
static void profile_title(struct NDPanel *p,const char *u,const char *t){(void)p;(void)u;(void)t;}
int nd_panel_event(struct NDPanel *p){struct Event *e;assert(at<count);e=&events[at++];if(e->action==118)p->choice=e->boot;if(e->action==118)p->choice=e->boot;if(e->action==111)p->reconnect=e->boot;if(e->action==112 && p->reconnect)p->retry_interval=e->boot;if(e->action==9001)live=1;if(e->action==9000)live=0;if(e->action==113)p->source=e->boot;else if(e->boot>=0)p->boot=e->boot;if(e->url)strcpy(p->values[0],e->url);if(e->http)strcpy(p->values[p->kind==2 ? 5:1],e->http);if(e->action==5)p->choice=e->boot;if(e->poll)strcpy(p->values[0],e->poll);return e->action;}
void nd_panel_choices(struct NDPanel *p,unsigned int n,const char **labels,unsigned int active){(void)n;(void)labels;p->choice=active;}
unsigned int nd_panel_choice(struct NDPanel *p,unsigned int n){(void)n;return p->choice;}
int nd_panel_checked(struct NDPanel *p){return p->boot;}
const char *nd_panel_get(struct NDPanel *p,unsigned int n){return p->values[n];}
void nd_panel_set(struct NDPanel *p,unsigned int n,const char *v){(void)p;if(p->kind==1&&n==3)strcpy(connection_status,v);if(n==4)strcpy(displayed_contact,v);if(n==5)strcpy(displayed_resources,v);if(n==2)strcpy(displayed_server,v);if(n==1)strcpy(displayed_network,v);}
void nd_panel_set_input(struct NDPanel *p,unsigned int n,const char *v){strcpy(p->values[n],v);}
void nd_panel_title(struct NDPanel *p,const char *v){(void)p;(void)v;}
void nd_panel_beep(struct NDPanel *p){(void)p;++beeps;}
'''
tests = r'''
static void reset(void){overview_fixture=contact_fixture=0;displayed_contact[0]=displayed_resources[0]=0;memset(&saved_profiles,0,sizeof(saved_profiles));reconnect_enabled=0;retry_seconds=0;reconnects=disconnects=0;notices=stored_recovered=0;stored_budget=0;sample_toggle_override=-1;stored_sample=0;toggle_override=-1;stored_low_memory=0;reply_state=NULL;stored_reconnect=1;stored_retry=60;at=count=writes=deletes=boots=starts=reloads=beeps=fail_write=fail_boot=persisted_boot=0;live=stops=start_enabled=stop_enabled=save_enabled=0;network=1;probe_result=AH_TEST_OK;probe_calls=probe_complete=log_views=0;displayed_server[0]=displayed_network[0]=tested_token[0]=0;disk_config=1;memset(&stored,0,sizeof(stored));strcpy(stored.url,"https://original.fixture.local");stored.poll=30;strcpy(stored_token,"fixture-original-token-for-testing");}
static void event(int id,int boot,const char *url,const char *http,const char *poll){events[count++]=(struct Event){id,boot,url,http,poll};}
static void changes(void){event(103,-1,NULL,NULL,NULL);event(100,-1,NULL,NULL,"60");event(106,1,NULL,NULL,NULL);}
static void server_save(const char *url){event(102,-1,NULL,NULL,NULL);event(100,-1,NULL,NULL,NULL);event(103,-1,url,NULL,NULL);}
static void run(void){amihass_preferences_ui(0);assert(at==count);}
int main(void){
 reset();assert(ah_servers_set(&saved_profiles,"Home",stored.url,stored_token));live=1;reply_state=AMIHASS_SERVER_CONNECTED;contact_fixture=1;event(105,-1,NULL,NULL,NULL);run();assert(!strcmp(displayed_contact,"Home: 5s ago"));

 reset();live=1;reply_state=AMIHASS_SERVER_CONNECTED;event(105,-1,NULL,NULL,NULL);run();assert(strstr(displayed_contact,"never"));
 reset();live=1;reply_state=AMIHASS_SERVER_CONNECTED;overview_fixture=1;event(105,-1,NULL,NULL,NULL);run();assert(strstr(displayed_resources,"4 cached")&&strstr(displayed_resources,"4kB"));

 reset();event(102,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&!saved_profiles.count&&!strcmp(stored.url,"https://original.fixture.local"));
 reset();event(102,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);event(103,-1,"https://new.fixture.local","fixture-new-token-for-testing",NULL);event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&saved_profiles.count==1&&!strcmp(stored.url,"https://new.fixture.local"));
 reset();assert(ah_servers_set(&saved_profiles,"Home",stored.url,stored_token));assert(ah_servers_set(&saved_profiles,"Cabin","https://cabin.fixture.local","fixture-cabin-token-for-testing"));event(102,-1,NULL,NULL,NULL);event(118,1,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&!strcmp(stored.url,"https://original.fixture.local")&&saved_profiles.count==2);
 reset();assert(ah_servers_set(&saved_profiles,"Home",stored.url,stored_token));assert(ah_servers_set(&saved_profiles,"Cabin","https://cabin.fixture.local","fixture-cabin-token-for-testing"));event(102,-1,NULL,NULL,NULL);event(1,-1,"https://edited.fixture.local",NULL,NULL);event(118,1,NULL,NULL,NULL);event(118,0,NULL,NULL,NULL);event(103,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&!strcmp(stored.url,"https://edited.fixture.local")&&!strcmp(saved_profiles.items[ah_servers_find(&saved_profiles,"Cabin")].url,"https://cabin.fixture.local"));

 {int selected;for(selected=0;selected<3;++selected){struct AmiHASSPreferences p;reset();memset(&p,0,sizeof(p));p.connection.poll=30;event(5,selected,NULL,NULL,NULL);event(100,-1,NULL,"12",NULL);assert(amihass_general_edit(&p)&&p.connection.poll==(selected==0?60:selected==1?30:10)&&p.background_budget==12&&p.dirty&&!writes);}reset();stored_recovered=1;event(105,-1,NULL,NULL,NULL);run();assert(notices==1&&!writes);reset();stored_recovered=1;event(104,-1,NULL,NULL,NULL);run();assert(notices==1&&writes==1);}

 reset();network=0;live=1;stored_sample=1;reply_state=AMIHASS_SERVER_SAMPLE;event(105,-1,NULL,NULL,NULL);run();assert(!strcmp(displayed_server,"Sample data (simulated)"));
 reset();event(113,1,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);run();assert(stored_sample&&writes==1&&reloads==1);
 reset();event(113,1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!stored_sample&&!writes);

 reset();toggle_override=1;event(103,-1,NULL,NULL,NULL);event(100,-1,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);run();assert(stored_low_memory&&writes==1&&reloads==1);
 reset();toggle_override=1;event(103,-1,NULL,NULL,NULL);event(100,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!stored_low_memory&&!writes);
 reset();toggle_override=1;event(103,-1,NULL,NULL,NULL);event(101,-1,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);run();assert(!stored_low_memory&&!writes);

 reset();live=1;reply_state=AMIHASS_SERVER_CONNECTED;event(105,-1,NULL,NULL,NULL);run();assert(!strcmp(displayed_server,"Connected")&&!probe_calls&&!reconnect_enabled);
 reset();live=1;reply_state=AMIHASS_SERVER_RETRY_WAIT;event(105,-1,NULL,NULL,NULL);run();assert(!strcmp(displayed_server,"Not Connected (retry)")&&!probe_calls);
 reset();live=1;reply_state=AMIHASS_SERVER_DISCONNECTED;event(105,-1,NULL,NULL,NULL);run();assert(!strcmp(displayed_server,"Connection Failure")&&!probe_calls);
 reset();live=1;network=0;reply_state=AMIHASS_SERVER_CONNECTED;event(105,-1,NULL,NULL,NULL);run();assert(!strcmp(displayed_server,"Not Connected")&&!probe_calls);
 reset();live=1;reply_state=AMIHASS_SERVER_CONNECTED;event(115,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!disconnects&&!reconnects&&!writes&&!reloads);
 reset();live=1;reply_state=AMIHASS_SERVER_PAUSED;event(105,-1,NULL,NULL,NULL);run();assert(!strcmp(displayed_server,"Disconnected")&&reconnect_enabled);
 reset();live=1;retry_seconds=125;reply_state=AMIHASS_SERVER_RETRY_WAIT;event(105,-1,NULL,NULL,NULL);run();assert(!strcmp(displayed_server,"Retry in 2m 5s"));
 reset();live=1;event(115,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(reconnects==1&&!writes&&!reloads);
 reset();live=1;network=0;event(115,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!reconnects&&!writes);
 reset();event(116,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(log_views==1&&!writes);
 reset();stored_reconnect=0;event(105,-1,NULL,NULL,NULL);run();assert(!probe_calls&&!strcmp(displayed_server,"Not Connected"));
 reset();event(102,-1,NULL,NULL,NULL);event(112,900,NULL,NULL,NULL);event(111,0,NULL,NULL,NULL);event(103,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&!stored_reconnect&&stored_retry==900);
 reset();event(102,-1,NULL,NULL,NULL);event(112,1800,NULL,NULL,NULL);event(111,0,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&stored_reconnect&&stored_retry==60);
 reset();changes();event(105,-1,NULL,NULL,NULL);run();assert(!writes&&!deletes&&!boots&&!starts&&!reloads&&stored.poll==30);
 reset();changes();event(-1,-1,NULL,NULL,NULL);run();assert(!writes&&!deletes&&!boots);
 reset();changes();event(104,-1,NULL,NULL,NULL);run();assert(writes==1&&boots==1&&persisted_boot&&reloads==1&&stored.poll==60);
 reset();server_save("https://new.fixture.local");event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&!boots&&reloads==1&&stored.poll==30);assert(!strcmp(stored.url,"https://new.fixture.local")&&!strcmp(stored_token,"fixture-imported-token-for-testing"));
 reset();event(102,-1,NULL,NULL,NULL);event(100,-1,NULL,NULL,NULL);event(104,-1,"https://discard.fixture.local",NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&!deletes&&!boots&&!strcmp(stored.url,"https://original.fixture.local"));
 reset();event(102,-1,NULL,NULL,NULL);event(101,-1,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!deletes&&disk_config);
 reset();event(102,-1,NULL,NULL,NULL);event(101,-1,NULL,NULL,NULL);event(103,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(deletes==1&&!disk_config&&reloads==1);
 reset();event(103,-1,NULL,NULL,NULL);event(100,-1,NULL,NULL,"9");event(101,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(beeps==1&&!writes&&stored.poll==30);
 reset();fail_write=1;changes();event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(beeps==1&&!writes&&!boots);
 reset();persisted_boot=1;event(106,0,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(persisted_boot&&!boots);
 reset();persisted_boot=1;event(106,0,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);run();assert(!persisted_boot&&boots==1&&!writes);
 reset();fail_boot=1;changes();event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&reloads==1&&beeps==1&&!persisted_boot);
 reset();network=0;event(104,-1,NULL,NULL,NULL);run();assert(!writes&&!boots&&!probe_complete&&!beeps&&save_enabled);assert(!strcmp(displayed_server,"Not Connected")&&strstr(displayed_network,"not running"));
 reset();probe_result=AH_TEST_FAILED;event(104,-1,NULL,NULL,NULL);run();assert(!writes&&!boots&&!beeps&&save_enabled&&!strcmp(displayed_server,"Not Connected"));
 reset();disk_config=0;event(104,-1,NULL,NULL,NULL);run();assert(!writes&&!boots&&!probe_calls&&!strcmp(displayed_server,"Not Configured"));
 reset();server_save("http://new.fixture.local");event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&stored.allow_http==1);
 reset();server_save("https://new.fixture.local");event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&stored.allow_http==0);
 reset();network=0;event(102,-1,NULL,NULL,NULL);event(102,-1,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&!boots&&strstr(connection_status,"ailed")&&strstr(connection_status,"not running"));
 reset();probe_result=AH_TEST_FAILED;event(102,-1,NULL,NULL,NULL);event(102,-1,NULL,NULL,NULL);event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&!boots&&strstr(connection_status,"ailed"));
 reset();changes();server_save("https://new.fixture.local");event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&!boots&&stored.poll==30);
 reset();changes();server_save("https://new.fixture.local");event(104,-1,NULL,NULL,NULL);run();assert(writes==2&&boots==1&&stored.poll==60);
 reset();event(102,-1,NULL,NULL,NULL);event(102,-1,"https://test-only.fixture.local",NULL,NULL);event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&!strcmp(connection_status,"Connected")&&!strcmp(stored.url,"https://original.fixture.local"));
 reset();changes();event(107,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(log_views==1&&!writes&&!boots&&stored.poll==30);
 reset();event(102,-1,NULL,NULL,NULL);event(103,-1,NULL,"manual-fixture-token-for-testing",NULL);event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&!strcmp(stored_token,"manual-fixture-token-for-testing"));
 reset();event(102,-1,NULL,NULL,NULL);event(104,-1,NULL,"discarded-fixture-token-for-testing",NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&!strcmp(stored_token,"fixture-original-token-for-testing"));
 reset();event(102,-1,NULL,NULL,NULL);event(103,-1,NULL,"short",NULL);event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&strstr(connection_status,"ailed"));
 reset();event(102,-1,NULL,NULL,NULL);event(101,-1,NULL,NULL,NULL);event(103,-1,NULL,"retyped-fixture-token-for-testing",NULL);event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&!deletes&&!strcmp(stored_token,"retyped-fixture-token-for-testing"));
 reset();event(102,-1,NULL,NULL,NULL);event(102,-1,NULL,"manual-test-token-for-validation",NULL);event(104,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!writes&&!strcmp(tested_token,"manual-test-token-for-validation"));
 {char maximum[1025];int i;for(i=0;i<1024;++i)maximum[i]='a';maximum[1024]=0;reset();event(102,-1,NULL,NULL,NULL);event(103,-1,NULL,maximum,NULL);event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&strlen(stored_token)==1024&&!strcmp(stored_token,maximum));}
 reset();event(105,-1,NULL,NULL,NULL);run();assert(start_enabled&&!stop_enabled);
 reset();live=1;event(105,-1,NULL,NULL,NULL);run();assert(!start_enabled&&stop_enabled);
 reset();event(100,-1,NULL,NULL,NULL);event(101,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(starts==1&&stops==1&&start_enabled&&!stop_enabled);
 reset();event(9001,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!start_enabled&&stop_enabled);
 reset();live=1;event(9000,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(start_enabled&&!stop_enabled);
 reset();live=1;event(100,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!starts);
 reset();event(101,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(!stops);
 reset();network=0;changes();event(104,-1,NULL,NULL,NULL);run();assert(writes==1&&boots==1&&stored.poll==60&&!beeps);
 reset();network=0;server_save("https://offline.fixture.local");event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&!strcmp(stored.url,"https://offline.fixture.local")&&!beeps);
 reset();probe_result=AH_TEST_FAILED;event(102,-1,NULL,NULL,NULL);event(102,-1,NULL,NULL,NULL);event(103,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(writes==1&&strstr(connection_status,"ailed"));
 reset();changes();event(108,-1,NULL,NULL,NULL);event(109,-1,NULL,NULL,NULL);event(110,-1,NULL,NULL,NULL);event(105,-1,NULL,NULL,NULL);run();assert(log_views==3&&!writes&&!boots);
 puts("Preferences: Server Save/Close, separate connection tests, inline failure status, HTTP/HTTPS and independent draft settings passed.");return 0;}

'''
with tempfile.TemporaryDirectory() as directory:
    p = Path(directory)
    (p/'exec').mkdir()
    (p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort {struct {char *ln_Name;} mp_Node;};struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n')
    model=(ROOT/'src/server_profiles.c').read_text().split('/* Lines are name',1)[0]
    (p/'test.c').write_text(prelude + model + service_controls + editors + controller + tests)
    command = [os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(p/'test.c'),'-DAH_HOST_TEST','-include','ah_locale.h',str(ROOT/'src/ah_locale.c'),str(ROOT/'src/core.c'),'-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
    subprocess.run(command,check=True)
    subprocess.run([str(p/'test')],check=True)
