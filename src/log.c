/* Bounded, temporary local diagnostics. Credentials are never log arguments. */
#include <exec/types.h>
#include <dos/datetime.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "ah_log.h"
static struct MsgPort *log_lock(void)
{
 struct MsgPort *port=CreateMsgPort();int owner=0;
 if(!port)return NULL;
 Forbid();if(!FindPort("AmiHASS.LogWriter")){port->mp_Node.ln_Name="AmiHASS.LogWriter";AddPort(port);owner=1;}Permit();
 if(!owner){DeleteMsgPort(port);return NULL;}return port;
}
static void log_unlock(struct MsgPort *port)
{Forbid();RemPort(port);Permit();DeleteMsgPort(port);}
static void log_timestamp(char stamp[40])
{
 struct DateTime date;char day[16],time[16];memset(&date,0,sizeof(date));
 DateStamp(&date.dat_Stamp);date.dat_Format=FORMAT_DOS;date.dat_StrDate=day;date.dat_StrTime=time;
 if(DateToStr(&date))sprintf(stamp,"%s %s",day,time);else strcpy(stamp,"Unknown time");
}
static long log_size(void)
{
 BPTR file=Open(AH_LOG_FILE,MODE_OLDFILE);LONG size;
 if(!file)return 0;
 size=Seek(file,0L,OFFSET_END);if(size>=0)size=Seek(file,0L,OFFSET_CURRENT);Close(file);return size;
}
static int log_rotate(void)
{DeleteFile(AH_LOG_PREVIOUS);return Rename(AH_LOG_FILE,AH_LOG_PREVIOUS)!=0;}
static void log_append(const char *line,unsigned long length)
{
 BPTR file=Open(AH_LOG_FILE,MODE_READWRITE);if(!file)return;
 if(Seek(file,0L,OFFSET_END)>=0)Write(file,(APTR)line,length);
 Close(file);
}
static const char *event_text(enum AHLogEvent event,long code)
{
 switch(event){
 case AH_LOG_MEMORY_RECLAIM:return code==0 ? "Low memory mode: unused libraries reclaimed.":"Low memory mode: memory reclamation failed.";
 case AH_LOG_ACTION_BLOCKED:return "Door or cover action refused: confirm it in Dashboard or allow script control in General Settings.";
 case AH_LOG_SELECTION_FULL:return "Entity limit reached: close a Dashboard page or Browser details to free slots.";
 case AH_LOG_SERVICE_STARTED:return "Service started.";
 case AH_LOG_SERVICE_STOPPING:return "Service stop requested.";
 case AH_LOG_SERVICE_STOPPED:return "Service stopped.";
 case AH_LOG_SETTINGS_RELOADED:return "Service reloaded saved server settings.";
 case AH_LOG_NETWORK_READY:return "Network ready: active interface with an IPv4 address.";
 case AH_LOG_NETWORK_DOWN:return "Network unavailable: no active interface with an IPv4 address.";
 case AH_LOG_TEST_HTTP:return "Connection test started using HTTP (unencrypted).";
 case AH_LOG_TEST_HTTPS:return "Connection test started using HTTPS (certificate verification enabled).";
 case AH_LOG_TEST_OK:return "Connection test passed: authenticated Home Assistant API reached.";
 case AH_LOG_TEST_NO_NETWORK:return "Connection test blocked: TCP/IP network is not ready.";
 case AH_LOG_TEST_BUSY:return "Connection test deferred: previous downloader is still finishing.";
 case AH_LOG_TEST_CONFIG:return "Connection test blocked: server URL or token is not configured.";
 case AH_LOG_TEST_NO_CURL:return "Connection test failed: C:curl is missing or inaccessible.";
 case AH_LOG_TEST_PREPARE:return "Connection test failed: could not prepare or launch the downloader.";
 case AH_LOG_TEST_CANCELLED:return "Connection test cancelled.";
 case AH_LOG_TEST_FAILED:case AH_LOG_DOWNLOAD_FAILED:
  if(code==3)return "Request could not be prepared locally; check the AmiHASS installation.";
  return ah_connection_text(ah_connection_result(code>=1000 ? 22:(int)code,code>=1000 ? (int)(code-1000):0,0));
 case AH_LOG_STATE_FAILED:return "Service could not refresh an entity state.";
 case AH_LOG_CATALOG_FAILED:return "Service could not refresh the entity catalogue.";
 case AH_LOG_ACTION_FAILED:return "Service action request failed; controls will not be replayed.";
 case AH_LOG_SERVER_SAVED:return "Server settings saved.";
 case AH_LOG_SERVER_REMOVED:return "Saved server credentials removed.";

 case AH_LOG_SERVER_CONNECTED:return "Home Assistant connection established.";
 case AH_LOG_SERVER_LOST:return "Home Assistant connection lost; automatic retry scheduled.";
 case AH_LOG_RETRY_PAUSED:return "Home Assistant disconnected; automatic reconnect is disabled.";
 case AH_LOG_SERVICE_UNCONFIGURED:return "Service has no configured server credentials.";
 case AH_LOG_SAVE_FAILED:return "Settings could not be saved; check storage and startup files.";
 default:return NULL;
 }
}
void ah_log_event(enum AHLogEvent event,long code)
{
 const char *message=event_text(event,code);struct MsgPort *guard;char stamp[40],line[256];long size=0;size_t length;
 if(!message)return;guard=log_lock();if(!guard)return;
 log_timestamp(stamp);
 if(event==AH_LOG_TEST_FAILED || event==AH_LOG_DOWNLOAD_FAILED)sprintf(line,"[%s] %s (code %ld)\n",stamp,message,code);
 else sprintf(line,"[%s] %s\n",stamp,message);
 length=strlen(line);size=log_size();if(size<0)goto done;
 if(size+(long)length>AH_LOG_LIMIT && !log_rotate())goto done;
 log_append(line,(unsigned long)length);
done:log_unlock(guard);
}
