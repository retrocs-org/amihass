/* Authenticated API checks use a separate worker; the UI remains responsive. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "ah_locale.h"
#include "connectivity.h"
#include "ah_log.h"
#include "nd_panel.h"
#include "nd_interaction.h"
#define FETCH_GUARD "AmiHASS.Probe"
#define FETCH_PREFIX "T:AmiHASS-Probe-"
#define FETCH_PROGRESS_FILE "request.conf"
#include "fetch_worker.h"
void amihass_connection_test_close(struct AHConnectionTest *test)
{
 if(test->port)fetch_close_port(test->port);
 /* A worker that already owns the request cleans it after finishing. */
 if(!test->transferred && test->directory[0])fetch_clean(test->directory);
 memset(test,0,sizeof(*test));
}
int amihass_connection_test_start(struct AHConnectionTest *test,const struct AmiHASSConnection *connection,const char *token)
{
 struct AmiHASSConnection c;char config[1800],request[200],tool[520],self[520],name[520],qtool[1100],qrequest[430],qself[1100],command[1300];
 BPTR lock,input,output;FILE *file;LONG result;int i,ok,busy;
 memset(test,0,sizeof(*test));
 if(!amihass_network_running()){ah_log_event(AH_LOG_TEST_NO_NETWORK,0);return AH_TEST_NO_NETWORK;}
 Forbid();busy=FindPort(FETCH_GUARD)!=NULL;Permit();if(busy){ah_log_event(AH_LOG_TEST_BUSY,0);return AH_TEST_BUSY;}
 c=*connection;c.allow_http=!strncmp(c.url,"http://",7);
 if(!amihass_request(&c,token,"/api/",NULL,config,sizeof(config))){ah_log_event(AH_LOG_TEST_CONFIG,0);return AH_TEST_FAILED;}
 lock=Lock("C:curl",ACCESS_READ);ok=lock && NameFromLock(lock,tool,sizeof(tool));if(lock)UnLock(lock);
 if(!ok){amihass_erase(config,sizeof(config));ah_log_event(AH_LOG_TEST_NO_CURL,0);return AH_TEST_NO_CURL;}
 for(i=0;i<20;++i){sprintf(test->directory,FETCH_PREFIX"%08lx-%08lx-%d",(ULONG)FindTask(NULL),weather_ticks(),i);lock=CreateDir(test->directory);if(lock){UnLock(lock);break;}}
 if(i==20){test->directory[0]=0;amihass_erase(config,sizeof(config));ah_log_event(AH_LOG_TEST_PREPARE,0);return AH_TEST_PREPARE;}
 sprintf(request,"%s/request.conf",test->directory);file=fopen(request,"wb");ok=0;
 if(file){ok=fputs(config,file)>=0;if(fclose(file))ok=0;}
 amihass_erase(config,sizeof(config));if(!ok)goto failed;
 if(!nd_quote_path(qtool,sizeof(qtool),tool) || !nd_quote_path(qrequest,sizeof(qrequest),request))goto failed;
 sprintf(test->command,ah_txf("%s --disable --config %s --silent --fail --connect-timeout 10 --max-time 30 --max-filesize 1024 --proto %s --write-out \"%%{http_code}\" --output "),qtool,qrequest,c.allow_http ? "=http":"=https");
 sprintf(request,"%s/response",test->directory);if(!nd_quote_path(qrequest,sizeof(qrequest),request) || strlen(test->command)+strlen(qrequest)>=sizeof(test->command))goto failed;strcat(test->command,qrequest);
 test->port=CreateMsgPort();if(!test->port)goto failed;
 sprintf(test->name,"AmiHASS.Test.%s",test->directory+sizeof(FETCH_PREFIX)-1);
 test->port->mp_Node.ln_Name=test->name;AddPort(test->port);
 /* Workbench tools have no CLI program name. The installed Prefs tool also
  * implements PROBE, so use its fixed sibling name in that case. */
 name[0]=0;if(!GetProgramName(name,sizeof(name)) || !name[0])strcpy(name,"AmiHASSPrefs");
 if(!GetProgramDir() || !NameFromLock(GetProgramDir(),self,sizeof(self)) || !AddPart(self,FilePart(name),sizeof(self)) || !nd_quote_path(qself,sizeof(qself),self))goto published_failed;
 sprintf(command,"%s PROBE %s",qself,test->name);input=Open("NIL:",MODE_OLDFILE);output=Open("NIL:",MODE_NEWFILE);
 if(!input || !output){if(input)Close(input);if(output)Close(output);goto published_failed;}
 result=SystemTags(command,SYS_Input,input,SYS_Output,output,SYS_Asynch,TRUE,NP_StackSize,65536UL,TAG_DONE);
 if(result<0){Close(input);Close(output);goto published_failed;}
 ah_log_event(c.allow_http ? AH_LOG_TEST_HTTP:AH_LOG_TEST_HTTPS,0);
 test->started=weather_ticks();return AH_TEST_PENDING;
published_failed:
 ah_log_event(AH_LOG_TEST_PREPARE,0);
 amihass_connection_test_close(test);return AH_TEST_PREPARE;
failed:
 ah_log_event(AH_LOG_TEST_PREPARE,0);
 if(test->port){DeleteMsgPort(test->port);test->port=NULL;}
 amihass_connection_test_close(test);return AH_TEST_PREPARE;
}
static void close_named_test(struct AHConnectionTest *test,int completed)
{
 if(completed)test->transferred=0;
 amihass_connection_test_close(test);
}
static int response_status(struct AHConnectionTest *test)
{char path[200];FILE *f;int code=0;sprintf(path,"%s/fetch.log",test->directory);f=fopen(path,"rb");if(f){if(fscanf(f,"%d",&code)!=1||fgetc(f)!=EOF)code=0;fclose(f);}return code;}
static int valid_response(struct AHConnectionTest *test)
{
 char path[200],body[1025];FILE *file;size_t count;int code,ok=0;
 sprintf(path,"%s/fetch.log",test->directory);file=fopen(path,"rb");if(!file)return 0;
 ok=fscanf(file,"%d",&code)==1 && code==200 && fgetc(file)==EOF;fclose(file);if(!ok)return 0;
 sprintf(path,"%s/response",test->directory);file=fopen(path,"rb");if(!file)return 0;
 count=fread(body,1,sizeof(body)-1,file);body[count]=0;
 ok=!ferror(file) && fgetc(file)==EOF && !memchr(body,0,count) && amihass_api_response(body);fclose(file);return ok;
}
int amihass_connection_test_poll(struct AHConnectionTest *test)
{
 struct FetchMessage *message;int result;
 if(!test->port)return AH_TEST_FAILED;
 if(!amihass_network_running()){ah_log_event(AH_LOG_TEST_NO_NETWORK,0);close_named_test(test,0);return AH_TEST_NO_NETWORK;}
 while((message=(struct FetchMessage *)GetMsg(test->port))!=NULL){
  if(message->magic!=FETCH_MAGIC || message->msg.mn_Length!=sizeof(*message)){
   if(message->msg.mn_ReplyPort)nd_reply_finish((struct Message *)message);continue;
  }
  if(message->kind==1){
   int handed=0;
   if(test->transferred)message->kind=-1;
   else{strcpy(message->command,test->command);strcpy(message->directory,test->directory);test->transferred=handed=1;}
   /* The probe worker gave up waiting: the packet is reclaimed, the test failed. */
   if(nd_reply_finish((struct Message *)message)&&handed){test->transferred=0;ah_log_event(AH_LOG_TEST_FAILED,28);close_named_test(test,1);return AH_TEST_TIMEOUT;}
  }else if(message->kind==2){
   result=ah_connection_result((int)message->result,response_status(test),message->result==0&&valid_response(test));
   ah_log_event(result==AH_TEST_OK ? AH_LOG_TEST_OK:AH_LOG_TEST_FAILED,response_status(test)>=300 ? 1000L+response_status(test):(long)message->result);
   FreeVec(message);close_named_test(test,1);return result;
  }
 }
 if(weather_ticks()-test->started>2000UL){ah_log_event(AH_LOG_TEST_FAILED,28);close_named_test(test,0);return AH_TEST_TIMEOUT;}
 return AH_TEST_PENDING;
}
int amihass_connection_test_dialog(const struct AmiHASSConnection *connection,const char *token,struct NDPanel *panel,int cancel)
{
 struct AHConnectionTest test;int result,action,i;unsigned long elapsed,last=~0UL;char progress[96];
 for(i=1;i<=12;++i)nd_panel_enable(panel,i,0);
 for(i=100;i<=106;++i)if(i!=cancel)nd_panel_enable(panel,i,0);
 nd_panel_enable(panel,114,0);nd_panel_show_secret(panel,0);
 nd_panel_title(panel,"Testing server connection...");result=amihass_connection_test_start(&test,connection,token);
 while(result==AH_TEST_PENDING){
  elapsed=(weather_ticks()-test.started)/50UL;if(elapsed!=last){last=elapsed;sprintf(progress,ah_txf("Testing connection... %lu s (Close cancels)"),elapsed);nd_panel_set(panel,3,progress);}
  action=nd_panel_event(panel);if(action==-1 || action==cancel){ah_log_event(AH_LOG_TEST_CANCELLED,0);close_named_test(&test,0);result=AH_TEST_CANCELLED;break;}
  result=amihass_connection_test_poll(&test);nd_panel_wait(panel,0,5);
 }
 for(i=1;i<=12;++i)nd_panel_enable(panel,i,1);
 for(i=100;i<=106;++i)nd_panel_enable(panel,i,1);nd_panel_enable(panel,114,1);
 return result;
}
int amihass_probe_worker(const char *parent)
{if(!parent || strncmp(parent,"AmiHASS.Test.",13) || strlen(parent)>70)return 20;return fetch_worker(parent);}
const char *amihass_server_status(int configured,int network,int result)
{
 if(!configured)return "Not Configured";
 if(!network)return "Not Connected";
 if(result==AH_TEST_OK)return "Connected";
 if(result<0&&result!=AH_TEST_CANCELLED)return "Connection Failure";
 return "Not Connected";
}
