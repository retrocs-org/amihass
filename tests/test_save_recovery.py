"""Fault-inject production cfg/layout transactions; simulate power loss in a child."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
config=(root/'src/config.c').read_text();config=config[config.index('void amihass_config_defaults'):]
layout=(root/'src/dashboard_layout.c').read_text()
code=r'''
#include <assert.h>
#include <errno.h>
#include "recovery_io.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "amihass_config.h"
#include "dashboard.h"
#define ACCESS_READ 0
#define ULONG unsigned long
typedef long BPTR;
static int fault;
static int fresh_path(const char *p){size_t n=strlen(p);return (n>4&&!strcmp(p+n-4,".new"))||strstr(p,"/new")!=NULL;}
static int old_path(const char *p){size_t n=strlen(p);return n>4&&!strcmp(p+n-4,".old");}
static FILE *fault_fopen(const char *p,const char *mode){if(fault==7&&mode[0]=='r'){fault=0;errno=EACCES;return NULL;}if(fault==6&&mode[0]=='w'&&fresh_path(p)){fault=0;return NULL;}return fopen(p,mode);}
static int fault_fprintf(FILE *f,const char *fmt,...){int n;va_list ap;if(fault==1){fault=0;fwrite("cut",1,3,f);return -1;}va_start(ap,fmt);n=vfprintf(f,fmt,ap);va_end(ap);return n;}
static int fault_fclose(FILE *f){int n=fclose(f);if(fault==2){fault=0;return EOF;}return n;}
static int fault_rename(const char *a,const char *b){int n;if((fault==3||fault==4)&&fresh_path(a)){if(fault==3)fault=0;return -1;}if(fault==4&&old_path(a)){fault=0;return -1;}n=rename(a,b);if(fault==5&&old_path(b)&&!strstr(b,".token")&&!n)_exit(77);if(fault==8&&old_path(b)&&strstr(b,".token")&&!n)_exit(77);return n;}
static BPTR CreateDir(const char *p){return mkdir(p,0700)==0;}
static BPTR Lock(const char *p,int mode){struct stat st;(void)mode;return stat(p,&st)==0;}
static void UnLock(BPTR p){(void)p;}
static void *FindTask(void *p){(void)p;return (void *)(uintptr_t)getpid();}
static int DeleteFile(const char *p){struct stat st;if(stat(p,&st))return 0;return (S_ISDIR(st.st_mode)?rmdir(p):unlink(p))==0;}
static int Rename(const char *a,const char *b){return fault_rename(a,b)==0;}
void amihass_erase(void *p,unsigned long n){volatile char *v=p;while(n--)*v++=0;}
#define fopen fault_fopen
#define fprintf fault_fprintf
#define fclose fault_fclose
#define rename fault_rename
'''+config+'\n'+layout+r'''
#undef fopen
#undef fprintf
#undef fclose
#undef rename
static void configuration(int mode){struct AmiHASSConfig old,newer,read;pid_t child;int status;amihass_config_defaults(&old);strcpy(old.connection.url,"https://fixture.invalid");strcpy(old.token,"fixture-token-not-a-real-secret");old.sample_mode=1;assert(amihass_config_save(&old));newer=old;newer.connection.poll=300;fault=mode;if(mode==5){child=fork();assert(child>=0);if(!child){amihass_config_save(&newer);_exit(1);}waitpid(child,&status,0);assert(WIFEXITED(status)&&WEXITSTATUS(status)==77);fault=0;}else assert(!amihass_config_save(&newer));assert(amihass_config_load(&read)&&read.connection.poll==30&&read.recovered==(mode==4||mode==5)&&!strcmp(read.token,old.token));assert(amihass_config_save(&newer)&&amihass_config_load(&read)&&read.connection.poll==300);assert(access(AMIHASS_CONFIG_FILE ".old",F_OK)!=0);}
static void dashboard(int mode,int book){struct AHDashboardBook old,newer,read;const char *path=book?"book.cfg":"layout.cfg";pid_t child;int status,recovered=0;ah_dashboard_book_defaults(&old);ah_dashboard_sample(&old.pages[0]);if(book){old.read_only=1;old.pages[0].tiles[0].refresh=300;old.pages[0].tiles[0].timestamp=1;}assert(book?ah_dashboard_book_save(&old,path):ah_dashboard_save(&old.pages[0],path));newer=old;strcpy(newer.pages[0].tiles[0].label,"New label");fault=mode;if(mode==5){child=fork();assert(child>=0);if(!child){if(book)ah_dashboard_book_save(&newer,path);else ah_dashboard_save(&newer.pages[0],path);_exit(1);}waitpid(child,&status,0);assert(WIFEXITED(status)&&WEXITSTATUS(status)==77);fault=0;}else assert(!(book?ah_dashboard_book_save(&newer,path):ah_dashboard_save(&newer.pages[0],path)));ah_dashboard_book_defaults(&read);assert(book?ah_dashboard_book_load_status(&read,path,&recovered):ah_dashboard_load_status(&read.pages[0],path,&recovered));assert(recovered==(mode==4||mode==5));assert(!memcmp(&read.pages[0],&old.pages[0],sizeof(old.pages[0])));assert(book?ah_dashboard_book_save(&newer,path):ah_dashboard_save(&newer.pages[0],path));assert(book?ah_dashboard_book_load(&read,path):ah_dashboard_load(&read.pages[0],path));assert(!strcmp(read.pages[0].tiles[0].label,"New label"));}
int main(void){int mode;for(mode=1;mode<=6;++mode){configuration(mode);dashboard(mode,0);dashboard(mode,1);}
 /* Crash right after the old token is moved aside: settings and the old token survive, and the next save completes. */
 {struct AmiHASSConfig old,newer,read;pid_t child;int status;amihass_config_defaults(&old);strcpy(old.connection.url,"https://fixture.invalid");strcpy(old.token,"fixture-token-not-a-real-secret");assert(amihass_config_save(&old));newer=old;newer.connection.poll=300;strcpy(newer.token,"fixture-token-newer-not-real");fault=8;child=fork();assert(child>=0);if(!child){amihass_config_save(&newer);_exit(1);}waitpid(child,&status,0);assert(WIFEXITED(status)&&WEXITSTATUS(status)==77);fault=0;
  assert(amihass_config_load(&read)&&read.connection.poll==30&&!strcmp(read.token,old.token));assert(amihass_config_save(&newer)&&amihass_config_load(&read)&&read.connection.poll==300&&!strcmp(read.token,newer.token));}{struct AmiHASSConfig c;struct AHDashboardBook b;fault=7;assert(!amihass_config_load(&c)&&!c.token[0]);fault=7;assert(!ah_dashboard_book_load(&b,"book.cfg"));fault=7;assert(!ah_dashboard_load(&b.pages[0],"layout.cfg"));}puts("Save recovery: partial writes, close/space errors, install/rollback failures and interrupted renames preserve cfg, token and both Dashboard formats; retries pass.");return 0;}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.c').write_text(code);cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),str(root/'src/core.c'),'-o',str(p/'test')];subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],cwd=p,check=True)
