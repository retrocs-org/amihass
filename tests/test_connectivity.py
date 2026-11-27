"""Exercise native connection-test launch/poll/cleanup against a loopback API."""
from pathlib import Path
import http.server, json, os, subprocess, tempfile, threading
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'src/connectivity.c').read_text()
source=source[source.index('void amihass_connection_test_close('):]
TOKEN='fixture-connectivity-token-for-tests'
mode='ok'
class API(http.server.BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        global mode
        assert self.path=='/api/'
        status=200
        body={'message':'API running.'}
        if self.headers.get('Authorization')!='Bearer '+TOKEN or mode=='unauthorized':status=401
        elif mode=='redirect':status=302
        elif mode=='missing':status=404
        elif mode=='wrong':body={'message':'Some other server'}
        payload=(b'{bad json' if mode=='malformed' else json.dumps(body).encode())
        self.send_response(status);self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.write(payload)
prelude=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "connectivity.h"
#include "nd_panel.h"
#include "ah_log.h"
void ah_log_event(enum AHLogEvent event,long code){(void)event;(void)code;}
#include "nd_interaction.h"
#define FETCH_GUARD "AmiHASS.Probe"
#define FETCH_PREFIX "T:AmiHASS-Probe-"
#define FETCH_MAGIC 0x4e445746UL
#define MEMF_PUBLIC 0
#define ACCESS_READ 0
#define MODE_OLDFILE 0
#define MODE_NEWFILE 1
#define SYS_Input 1
#define SYS_Output 2
#define SYS_Asynch 3
#define NP_StackSize 4
#define TAG_DONE 0
#define TRUE 1
typedef intptr_t BPTR;
struct FetchMessage {struct Message msg;ULONG magic;int kind;LONG result;char command[1800],directory[160];};
struct {int LibList;} execbase,*SysBase=&execbase;
static struct FetchMessage *incoming;
static int workbench;
static int network=1,launches,curls,ports,allocations,drop_reply;
static unsigned long clock_ticks=100;
int amihass_network_running(void){return network;}
static void Forbid(void){}static void Permit(void){}static void Delay(int n){(void)n;}
/* Production loops wait on signals; in tests this is one scripted Delay step. */
void nd_panel_wait(struct NDPanel *p,unsigned long e,unsigned long t){(void)p;(void)e;(void)t;Delay(5);}

static void *FindName(void *list,const char *name){(void)list;assert(!strcmp(name,"bsdsocket.library"));return network ? (void *)1:NULL;}
static struct MsgPort *FindPort(const char *name){assert(!strcmp(name,FETCH_GUARD));return NULL;}
static void *FindTask(void *p){(void)p;return (void *)(uintptr_t)42;}
static unsigned long weather_ticks(void){return clock_ticks;}
static BPTR Lock(const char *path,int mode){(void)mode;assert(!strcmp(path,"C:curl"));return 1;}
static void UnLock(BPTR lock){(void)lock;}
static int NameFromLock(BPTR lock,char *path,size_t size){snprintf(path,size,"%s",lock==1 ? "/usr/bin/curl":".");return 1;}
static BPTR CreateDir(const char *path){return mkdir(path,0700)==0;}
static int AddPart(char *path,const char *file,size_t size){if(strlen(path)+strlen(file)+2>size)return 0;strcat(path,"/");strcat(path,file);return 1;}
static int GetProgramName(char *name,size_t n){if(workbench){name[0]=0;return 0;}snprintf(name,n,"AmiHASSPrefs");return 1;}
static BPTR GetProgramDir(void){return 2;}
static char *FilePart(char *name){char *p=strrchr(name,'/');return p ? p+1:name;}
static BPTR Open(const char *name,int mode){(void)name;(void)mode;return 1;}
static void Close(BPTR f){(void)f;}
static struct MsgPort *CreateMsgPort(void){++ports;return calloc(1,sizeof(struct MsgPort));}
static void DeleteMsgPort(struct MsgPort *p){--ports;free(p);}
static void *AllocVec(size_t size,int flags){(void)flags;++allocations;return calloc(1,size);}
static void FreeVec(void *p){--allocations;free(p);}
static void AddPort(struct MsgPort *p){(void)p;}
static void *GetMsg(struct MsgPort *p){struct FetchMessage *m=incoming;(void)p;incoming=NULL;return m;}
static LONG SystemTags(const char *command,...){(void)command;++launches;assert(strstr(command," PROBE AmiHASS.Test."));incoming=AllocVec(sizeof(*incoming),0);incoming->magic=FETCH_MAGIC;incoming->kind=1;incoming->msg.mn_Length=sizeof(*incoming);return 0;}
static void ReplyMsg(struct Message *base){struct FetchMessage *m=(struct FetchMessage *)base;char command[2200];int result;
 if(m->kind==-1){FreeVec(m);return;}
 assert(m->kind==1);assert(!strstr(m->command,"fixture-connectivity-token"));assert(!strstr(m->command,"--insecure"));assert(!strstr(m->command,"--location"));
 ++curls;snprintf(command,sizeof(command),"%s >'%s/fetch.log'",m->command,m->directory);result=system(command);m->result=WIFEXITED(result) ? WEXITSTATUS(result):99;m->kind=2;
 if(drop_reply){FreeVec(m);return;}incoming=m;
}
/* Production uses owned reply ports: a live worker receives the reply. */
static int nd_reply_finish(struct Message *m){ReplyMsg(m);return 0;}
static void fetch_clean(const char *directory){char path[300];const char *names[]={"request.conf","response","fetch.log"};int i;for(i=0;i<3;++i){snprintf(path,sizeof(path),"%s/%s",directory,names[i]);unlink(path);}rmdir(directory);}
static void fetch_close_port(struct MsgPort *port){if(incoming){FreeVec(incoming);incoming=NULL;}DeleteMsgPort(port);}
static int fetch_worker(const char *parent){(void)parent;return 0;}
void amihass_erase(void *p,unsigned long n){memset(p,0,n);}
void nd_panel_show_secret(struct NDPanel *p,int show){(void)p;(void)show;}
void nd_panel_enable(struct NDPanel *p,unsigned int id,int enable){(void)p;(void)id;(void)enable;}
void nd_panel_set(struct NDPanel *p,unsigned int id,const char *text){(void)p;(void)id;assert(text&&strstr(text,"Testing connection"));}
void nd_panel_title(struct NDPanel *p,const char *title){(void)p;(void)title;}
int nd_panel_event(struct NDPanel *p){(void)p;return -1;}
'''
main=r'''
int main(int argc,char **argv){struct AHConnectionTest test;struct AmiHASSConnection c;int result,expected;char directory[160];
 assert(argc==4);strcpy(c.url,argv[1]);c.poll=30;c.allow_http=1;expected=atoi(argv[3]);
 network=0;assert(amihass_connection_test_start(&test,&c,argv[2])==AH_TEST_NO_NETWORK&&!launches&&!curls&&!ports);network=1;
 workbench=1;result=amihass_connection_test_start(&test,&c,argv[2]);assert(result==AH_TEST_PENDING);strcpy(directory,test.directory);workbench=0;
 assert(strstr(test.command,!strncmp(c.url,"https://",8) ? "--proto =https":"--proto =http"));
 result=amihass_connection_test_poll(&test);assert(result==expected);assert(!ports&&!allocations&&!test.port&&access(directory,F_OK));
 result=amihass_connection_test_start(&test,&c,argv[2]);assert(result==AH_TEST_PENDING);strcpy(directory,test.directory);network=0;
 assert(amihass_connection_test_poll(&test)==AH_TEST_NO_NETWORK&&!ports&&!allocations&&access(directory,F_OK));network=1;
 result=amihass_connection_test_start(&test,&c,argv[2]);assert(result==AH_TEST_PENDING);strcpy(directory,test.directory);
 amihass_connection_test_close(&test);assert(!ports&&!allocations&&access(directory,F_OK));
 result=amihass_connection_test_start(&test,&c,argv[2]);assert(result==AH_TEST_PENDING);strcpy(directory,test.directory);FreeVec(incoming);incoming=NULL;clock_ticks+=2100;
 assert(amihass_connection_test_poll(&test)==AH_TEST_TIMEOUT&&!ports&&!allocations&&access(directory,F_OK));
 assert(!strcmp(amihass_server_status(0,1,AH_TEST_FAILED),"Not Configured"));
 assert(!strcmp(amihass_server_status(1,0,AH_TEST_FAILED),"Not Connected"));
 assert(!strcmp(amihass_server_status(1,1,AH_TEST_FAILED),"Connection Failure"));
 assert(!strcmp(amihass_server_status(1,1,AH_TEST_OK),"Connected"));
 assert(amihass_connection_test_dialog(&c,argv[2],NULL,104)==AH_TEST_CANCELLED);assert(!ports&&!allocations);return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir()
    (p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef unsigned long ULONG;typedef long LONG;struct MsgPort {struct {char *ln_Name;} mp_Node;};struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n')
    (p/'test.c').write_text(prelude+source+main)
    command=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(p/'test.c'),'-DAH_HOST_TEST','-include','ah_locale.h',str(ROOT/'src/ah_locale.c'),str(ROOT/'src/core.c'),str(ROOT/'src/quote.c'),'-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
    subprocess.run(command,check=True)
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),API);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    url='http://127.0.0.1:'+str(server.server_port)
    try:
        for mode in ('ok','unauthorized','redirect','missing','wrong','malformed'):
            subprocess.run([str(p/'test'),url,TOKEN,str({'ok':1,'unauthorized':-9,'redirect':-12,'missing':-11,'wrong':-14,'malformed':-14}[mode])],cwd=p,check=True)
        mode='ok'
        subprocess.run([str(p/'test'),url,'wrong-fixture-token-for-testing','-9'],cwd=p,check=True)
        # HTTPS to a plaintext server must fail, never fall back to HTTP.
        subprocess.run([str(p/'test'),url.replace('http:','https:'),TOKEN,'-16'],cwd=p,check=True)
    finally:server.shutdown();server.server_close();thread.join()
    subprocess.run([str(p/'test'),url,TOKEN,'-18'],cwd=p,check=True)
print('Connection tests: API authentication, HTTP/HTTPS, redirects, bad responses, offline preflight, cancellation, timeout and cleanup passed.')
