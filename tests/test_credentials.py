#!/usr/bin/env python3
"""Test native credential-file transactions without AmigaOS or real secrets."""
import os,re,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
config=(ROOT/'src/config.c').read_text();config=config[config.index('void amihass_config_defaults'): ]
connection=(ROOT/'src/connection.c').read_text();connection=connection[connection.index('int amihass_connection_load'): ]
program=r'''
#include <assert.h>
#include <errno.h>
#include "recovery_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include "amihass.h"
#include "amihass_config.h"
#define ACCESS_READ 0
#define ULONG unsigned long
typedef long BPTR;
static int fail_install,fail_token;
static BPTR CreateDir(const char *p){return mkdir(p,0700)==0;}
static BPTR Lock(const char *p,int m){struct stat s;(void)m;return stat(p,&s)==0;}
static void UnLock(BPTR p){(void)p;}
static void *FindTask(void *p){(void)p;return (void *)(uintptr_t)42;}
static int DeleteFile(const char *p){struct stat s;if(stat(p,&s))return 0;return (S_ISDIR(s.st_mode) ? rmdir(p):unlink(p))==0;}
static int Rename(const char *a,const char *b){if(fail_token && !strcmp(b,AMIHASS_TOKEN_FILE ".old")){fail_token=0;return 0;}if(fail_install && !strcmp(b,AMIHASS_CONFIG_FILE) && strstr(a,"/new")){fail_install=0;return 0;}return rename(a,b)==0;}
void amihass_erase(void *p,unsigned long n){volatile char *c=p;while(n--)*c++=0;}
'''+config+'\n'+connection+r'''
int main(void){struct AmiHASSConnection c,loaded;struct AmiHASSConfig cfg;char token[1025];FILE *f;
 assert(amihass_config_load(&cfg));assert(!cfg.connection.url[0] && cfg.boot==1 && cfg.connection.poll==30);
 cfg.push_updates=1;cfg.background_budget=25;cfg.sample_mode=1;cfg.low_memory=1;cfg.boot=0;cfg.connection.poll=60;cfg.reconnect=0;cfg.retry_interval=900;strcpy(cfg.service_dir,"WORK:Apps/Bridge");strcpy(cfg.prefs_dir,"WORK:Prefs");strcpy(cfg.startup_dir,"WORK:Startup");
 assert(amihass_config_save(&cfg));assert(amihass_config_load(&cfg) && cfg.push_updates && cfg.background_budget==25 && cfg.sample_mode && cfg.low_memory && !cfg.boot && cfg.connection.poll==60 && !cfg.reconnect && cfg.retry_interval==900);

 strcpy(c.url,"https://old.fixture.local");c.poll=30;c.allow_http=0;
 assert(amihass_connection_save(&c,"test-only-old-connection-token"));
 assert(amihass_connection_load(&loaded,token));assert(!strcmp(token,"test-only-old-connection-token") && !strcmp(c.url,loaded.url));
 strcpy(c.url,"https://new.fixture.local");fail_install=1;
 assert(!amihass_connection_save(&c,"test-only-new-connection-token"));
 assert(amihass_connection_load(&loaded,token));assert(!strcmp(token,"test-only-old-connection-token") && !strcmp(loaded.url,"https://old.fixture.local"));
 assert(amihass_connection_save(&c,"test-only-new-connection-token"));assert(amihass_connection_load(&loaded,token));assert(!strcmp(token,"test-only-new-connection-token") && !strcmp(loaded.url,c.url));
 assert(!amihass_connection_save(&c,"short"));assert(amihass_connection_load(&loaded,token) && !strcmp(token,"test-only-new-connection-token"));
 assert(amihass_config_load(&cfg) && cfg.push_updates && cfg.background_budget==25 && cfg.sample_mode && cfg.low_memory && !cfg.boot && !cfg.reconnect && cfg.retry_interval==900 && !strcmp(cfg.service_dir,"WORK:Apps/Bridge") && !strcmp(cfg.prefs_dir,"WORK:Prefs") && !strcmp(cfg.startup_dir,"WORK:Startup"));
 memset(token,'x',1024);token[1024]=0;assert(amihass_connection_save(&c,token));memset(token,0,sizeof(token));assert(amihass_connection_load(&loaded,token) && strlen(token)==1024);
 assert(amihass_config_forget());assert(amihass_config_load(&cfg) && !cfg.token[0] && !cfg.connection.url[0] && !cfg.boot && !strcmp(cfg.prefs_dir,"WORK:Prefs"));
 assert(!amihass_connection_load(&loaded,token));
 cfg.retry_interval=61;assert(!amihass_config_save(&cfg));cfg.retry_interval=900;
 assert(DeleteFile(AMIHASS_CONFIG_FILE));
 f=fopen(AMIHASS_CONFIG_FILE,"wb");assert(f);fputs("AMIHASSCFG1\nhttps://legacy.fixture.local\n45 0 1\nfixture-legacy-token-for-testing\nSYS:Utilities/AmiHASS\nSYS:Prefs\nSYS:WBStartup\n",f);fclose(f);
 assert(amihass_config_load(&cfg) && !cfg.sample_mode && !cfg.low_memory && cfg.reconnect==1 && cfg.retry_interval==60);assert(DeleteFile(AMIHASS_CONFIG_FILE));assert(CreateDir("ENVARC:AmiHASS/Private"));
 f=fopen("ENVARC:AmiHASS/Private/connection","wb");assert(f);fputs("NDHASS1\nhttps://legacy.fixture.local\n45 0\nfixture-legacy-token-for-testing\n",f);fclose(f);
 f=fopen("ENVARC:AmiHASS/InstallPath","wb");assert(f);fputs("WORK:Legacy\n",f);fclose(f);
 assert(amihass_config_load(&cfg) && cfg.connection.poll==45 && !strcmp(cfg.service_dir,"WORK:Legacy") && !strcmp(cfg.token,"fixture-legacy-token-for-testing"));
 assert(amihass_config_save(&cfg));assert(!Lock("ENVARC:AmiHASS/Private/connection",0) && !Lock("ENVARC:AmiHASS/Private",0) && !Lock("ENVARC:AmiHASS/InstallPath",0) && Lock(AMIHASS_TOKEN_FILE,0));
 {struct AmiHASSConfig again;assert(amihass_config_load(&again)&&!strcmp(again.token,"fixture-legacy-token-for-testing")&&!strcmp(again.service_dir,"WORK:Legacy")&&again.connection.poll==45);}
 f=fopen(AMIHASS_CONFIG_FILE,"wb");assert(f);fputs("broken\n",f);fclose(f);assert(!amihass_config_load(&cfg) && !cfg.token[0]);
 {const char *bad[]={"121","-1","25junk","999999999999999999999999"};int i;for(i=0;i<4;++i){f=fopen(AMIHASS_CONFIG_FILE,"wb");assert(f);fprintf(f,"AMIHASSCFG1\n\n30 0 1 1 60 0 1 %s\n\nSYS:Utilities/AmiHASS\nSYS:Prefs\nSYS:WBStartup\n",bad[i]);fclose(f);assert(!amihass_config_load(&cfg)&&!cfg.token[0]);}}
 /* The token lives in its own file; AmiHASS.cfg can be shared safely. */
 {char text[2048];size_t n;struct AmiHASSConfig t;amihass_config_defaults(&t);strcpy(t.connection.url,"https://split.fixture.local");strcpy(t.token,"split-token-fixture-value");t.remote_security=1;assert(amihass_config_save(&t));
  f=fopen(AMIHASS_CONFIG_FILE,"rb");assert(f);n=fread(text,1,sizeof(text)-1,f);fclose(f);text[n]=0;assert(!strstr(text,"split-token")&&strstr(text,"split.fixture.local")&&strstr(text," 1\n\n"));
  f=fopen(AMIHASS_TOKEN_FILE,"rb");assert(f);n=fread(text,1,sizeof(text)-1,f);fclose(f);text[n]=0;assert(!strcmp(text,"AMIHASSTOK1\nsplit-token-fixture-value\n"));
  assert(amihass_config_load(&t)&&!strcmp(t.token,"split-token-fixture-value")&&t.remote_security==1);
  /* Failed installs keep URL and token as a matching pair. */
  strcpy(t.connection.url,"https://other.fixture.local");strcpy(t.token,"other-token-fixture-value");fail_install=1;assert(!amihass_config_save(&t));assert(amihass_config_load(&t)&&!strcmp(t.token,"split-token-fixture-value")&&!strcmp(t.connection.url,"https://split.fixture.local"));
  strcpy(t.connection.url,"https://other.fixture.local");strcpy(t.token,"other-token-fixture-value");fail_token=1;assert(!amihass_config_save(&t));assert(amihass_config_load(&t)&&!strcmp(t.token,"split-token-fixture-value")&&!strcmp(t.connection.url,"https://split.fixture.local"));
  /* A damaged token file leaves the server unconfigured, not all settings lost. */
  f=fopen(AMIHASS_TOKEN_FILE,"wb");fputs("garbage\n",f);fclose(f);assert(amihass_config_load(&t)&&!t.token[0]&&!strcmp(t.connection.url,"https://split.fixture.local")&&t.remote_security==1);
  /* Older cfg with an embedded token: used, then moved out on the next save. */
  assert(DeleteFile(AMIHASS_TOKEN_FILE));f=fopen(AMIHASS_CONFIG_FILE,"wb");fputs("AMIHASSCFG1\nhttps://embedded.fixture.local\n30 0 1 1 60 0 0 0 0\nembedded-token-fixture-value\nSYS:Utilities/AmiHASS\nSYS:Prefs\nSYS:WBStartup\n",f);fclose(f);
  assert(amihass_config_load(&t)&&!strcmp(t.token,"embedded-token-fixture-value")&&!t.remote_security);assert(amihass_config_save(&t));
  f=fopen(AMIHASS_CONFIG_FILE,"rb");n=fread(text,1,sizeof(text)-1,f);fclose(f);text[n]=0;assert(!strstr(text,"embedded-token"));assert(amihass_config_load(&t)&&!strcmp(t.token,"embedded-token-fixture-value"));
  /* Boot copies in ENV: (RAM) are removed by every save and by scrubbing. */
  assert(CreateDir("ENV:AmiHASS")||1);f=fopen("ENV:AmiHASS/AmiHASS.token","wb");fputs("stale",f);fclose(f);f=fopen("ENV:AmiHASS/AmiHASS.cfg","wb");fputs("stale",f);fclose(f);assert(amihass_config_save(&t));assert(!Lock("ENV:AmiHASS/AmiHASS.token",0)&&!Lock("ENV:AmiHASS/AmiHASS.cfg",0));
  f=fopen("ENV:AmiHASS/AmiHASS.token","wb");fputs("stale",f);fclose(f);amihass_config_scrub_env();assert(!Lock("ENV:AmiHASS/AmiHASS.token",0));
  /* Forget removes the token file. */
  assert(amihass_config_forget());assert(!Lock(AMIHASS_TOKEN_FILE,0)&&!Lock(AMIHASS_TOKEN_FILE ".old",0));assert(amihass_config_load(&t)&&!t.token[0]&&!t.connection.url[0]);
  t.remote_security=2;assert(!amihass_config_save(&t));}
 /* Development-era layouts: damaged legacy path files keep the defaults so
  * the migrated settings can still be saved. */
 {struct AmiHASSConfig t;assert(DeleteFile(AMIHASS_CONFIG_FILE));f=fopen("ENVARC:AmiHASS/InstallPath","wb");fputs("WORK:\001Bad\n",f);fclose(f);f=fopen("ENVARC:AmiHASS/PrefsPath","wb");fputs("WORK:GoodPrefs\n",f);fclose(f);
  assert(amihass_config_load(&t)&&!strcmp(t.service_dir,"SYS:AmiHASS")&&!strcmp(t.prefs_dir,"WORK:GoodPrefs")&&!t.connection.url[0]);assert(amihass_config_save(&t));assert(!Lock("ENVARC:AmiHASS/InstallPath",0)&&!Lock("ENVARC:AmiHASS/PrefsPath",0));
  /* New settings always win over leftover legacy files, which the next save removes. */
  f=fopen("ENVARC:AmiHASS/InstallPath","wb");fputs("WORK:Old\n",f);fclose(f);assert(amihass_config_load(&t)&&!strcmp(t.service_dir,"SYS:AmiHASS"));assert(amihass_config_save(&t)&&!Lock("ENVARC:AmiHASS/InstallPath",0));}
 puts("Unified cfg defaults, migration, field preservation, full token, separate token file, ENV: scrubbing, legacy layouts, forget and failed-save rollback passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp=Path(temporary);(tmp/'exec').mkdir();(tmp/'ENV:AmiHASS').mkdir();(tmp/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort; struct Message { unsigned short mn_Length; struct MsgPort *mn_ReplyPort; };\n')
    (tmp/'test.c').write_text(program);cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(tmp),'-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(tmp/'test.c'),str(ROOT/'src/core.c'),'-o',str(tmp/'test')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(tmp/'test')],cwd=tmp,check=True)
