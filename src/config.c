/* All persistent bridge settings live in one atomically replaced cfg file. */
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include "recovery_io.h"
#include <stdlib.h>
#include <string.h>
#include "amihass_config.h"
#include "amihass.h"
void amihass_config_defaults(struct AmiHASSConfig *c)
{memset(c,0,sizeof(*c));c->connection.poll=30;c->boot=1;c->reconnect=1;c->retry_interval=60;strcpy(c->service_dir,"SYS:AmiHASS");strcpy(c->prefs_dir,"SYS:Prefs");strcpy(c->startup_dir,"SYS:WBStartup");}
static int config_line(FILE *file,char *out,unsigned int cap)
{unsigned int n=0;int ch;while((ch=fgetc(file))!=EOF){if(ch=='\n'){out[n]=0;return 1;}if(ch=='\r')continue;if(n+1>=cap)return 0;out[n++]=(char)ch;}return 0;}
static int config_path_valid(const char *path)
{const unsigned char *p=(const unsigned char *)path;if(!*p || strlen(path)>=300)return 0;for(;*p;++p)if(*p<32 || *p==127)return 0;return 1;}
/* Pre-cfg releases kept each path in its own file. A damaged or overlong
 * entry keeps the default instead of making every later save fail. */
static void legacy_path(char *out,unsigned int cap,const char *name)
{char value[300];FILE *f=fopen(name,"rb");if(!f)return;if(fgets(value,sizeof(value),f)){value[strcspn(value,"\r\n")]=0;if(config_path_valid(value)&&strlen(value)<cap)strcpy(out,value);}fclose(f);}
/* AMIHASSTOK1 file: 1 valid token, 0 none, -1 unreadable or damaged. */
static int token_file(char *out)
{FILE *f=fopen(AMIHASS_TOKEN_FILE,"rb");char header[16],line[1030];int ok;if(!f){if(!ah_file_missing(AMIHASS_TOKEN_FILE))return -1;f=fopen(AMIHASS_TOKEN_FILE ".old","rb");if(!f)return 0;}
 ok=config_line(f,header,sizeof(header))&&!strcmp(header,"AMIHASSTOK1")&&config_line(f,line,sizeof(line))&&fgetc(f)==EOF&&amihass_token(line,out);fclose(f);amihass_erase(line,sizeof(line));return ok ? 1:-1;}
void amihass_config_scrub_env(void)
{DeleteFile("ENV:AmiHASS/AmiHASS.cfg");DeleteFile("ENV:AmiHASS/AmiHASS.cfg.old");DeleteFile("ENV:AmiHASS/AmiHASS.token");DeleteFile("ENV:AmiHASS/AmiHASS.token.old");DeleteFile("ENV:AmiHASS/Servers.token");DeleteFile("ENV:AmiHASS/Servers.token.old");DeleteFile("ENV:AmiHASS/Servers.cfg");DeleteFile("ENV:AmiHASS/Servers.cfg.old");DeleteFile("ENV:AmiHASS/Private/connection");DeleteFile("ENV:AmiHASS/Private");}
static int config_numbers(struct AmiHASSConfig *c,const char *text)
{unsigned long values[10],value;unsigned int count=0;const char *p=text;while(*p){while(*p==' '||*p=='\t')++p;if(!*p)break;if(count==10||*p<'0'||*p>'9')return 0;value=0;while(*p>='0'&&*p<='9'){value=value*10UL+(unsigned long)(*p++-'0');if(value>3600UL)return 0;}if(*p&&*p!=' '&&*p!='\t')return 0;values[count++]=value;}if(count!=3&&count!=5&&count!=6&&count!=7&&count!=8&&count!=9&&count!=10)return 0;c->connection.poll=(unsigned int)values[0];c->connection.allow_http=(int)values[1];c->boot=(int)values[2];if(count>=5){c->reconnect=(int)values[3];c->retry_interval=(unsigned int)values[4];}if(count>=6)c->low_memory=(int)values[5];if(count>=7)c->sample_mode=(int)values[6];if(count>=8)c->background_budget=(unsigned int)values[7];if(count>=9)c->push_updates=(int)values[8];if(count>=10)c->remote_security=(int)values[9];return 1;}
int amihass_config_load(struct AmiHASSConfig *c)
{
 FILE *file;char header[32],numbers[80],raw[1030];int ok=0;
 amihass_config_defaults(c);file=fopen(AMIHASS_CONFIG_FILE,"rb");if(!file){if(!ah_file_missing(AMIHASS_CONFIG_FILE))return 0;file=fopen(AMIHASS_CONFIG_FILE ".old","rb");if(file)c->recovered=1;if(!file&&!ah_file_missing(AMIHASS_CONFIG_FILE ".old"))return 0;}
 if(file){
  if(config_line(file,header,sizeof(header)) && !strcmp(header,"AMIHASSCFG1") && config_line(file,c->connection.url,sizeof(c->connection.url)) && config_line(file,numbers,sizeof(numbers)) && config_numbers(c,numbers) && config_line(file,c->token,sizeof(c->token)) && config_line(file,c->service_dir,sizeof(c->service_dir)) && config_line(file,c->prefs_dir,sizeof(c->prefs_dir)) && config_line(file,c->startup_dir,sizeof(c->startup_dir)) && fgetc(file)==EOF)ok=1;
  fclose(file);if(ok && (c->push_updates==0||c->push_updates==1) && c->background_budget<=120 && c->connection.poll>=10 && c->connection.poll<=3600 && (c->boot==0 || c->boot==1) && (c->sample_mode==0 || c->sample_mode==1) && (c->low_memory==0 || c->low_memory==1) && (c->reconnect==0 || c->reconnect==1) && (c->retry_interval==60 || c->retry_interval==600 || c->retry_interval==900 || c->retry_interval==1800) && (c->remote_security==0 || c->remote_security==1) && c->service_dir[0] && c->prefs_dir[0] && c->startup_dir[0] && (c->connection.url[0] ? amihass_url(c->connection.url,!strncmp(c->connection.url,"http://",7)):!c->token[0]) && (!c->token[0] || amihass_token(c->token,raw))){
   c->connection.allow_http=!strncmp(c->connection.url,"http://",7);if(c->token[0])strcpy(c->token,raw);
   /* AmiHASS.token wins; a token still inside AmiHASS.cfg (older layout) is
    * used until the next save moves it. A damaged token file leaves the
    * server unconfigured instead of rejecting all settings. */
   if(c->connection.url[0]){if(token_file(raw)>0)strcpy(c->token,raw);}else amihass_erase(c->token,sizeof(c->token));
   amihass_erase(raw,sizeof(raw));return 1;}
  amihass_erase(raw,sizeof(raw));amihass_config_defaults(c);return 0;
 }
 /* Read old releases only for migration; the next save removes these files. */
 legacy_path(c->service_dir,sizeof(c->service_dir),"ENVARC:AmiHASS/InstallPath");
 legacy_path(c->prefs_dir,sizeof(c->prefs_dir),"ENVARC:AmiHASS/PrefsPath");
 legacy_path(c->startup_dir,sizeof(c->startup_dir),"ENVARC:AmiHASS/StartupPath");
 file=fopen("ENVARC:AmiHASS/Private/connection","rb");
 if(file){if(config_line(file,header,sizeof(header)) && !strcmp(header,"NDHASS1") && config_line(file,c->connection.url,sizeof(c->connection.url)) && config_line(file,numbers,sizeof(numbers)) && sscanf(numbers,"%u %d",&c->connection.poll,&c->connection.allow_http)==2 && config_line(file,raw,sizeof(raw)) && c->connection.poll>=10 && c->connection.poll<=3600 && amihass_url(c->connection.url,c->connection.allow_http) && amihass_token(raw,c->token))ok=1;fclose(file);}
 amihass_erase(raw,sizeof(raw));if(!ok){c->connection.url[0]=0;amihass_erase(c->token,sizeof(c->token));c->connection.poll=30;}return 1;
}
static int write_token(const char *path,const char *token)
{FILE *f=fopen(path,"wb");int ok;if(!f)return 0;ok=fprintf(f,"AMIHASSTOK1\n%s\n",token)>0;if(fclose(f))ok=0;return ok;}
/* Two files, one transaction: the token is installed first and rolled back
 * if the settings file cannot be replaced, so URL and token always match. */
int amihass_config_save(const struct AmiHASSConfig *c)
{
 char token[1025],dir[130],fresh[160],fresh_token[160],backup[160];FILE *file;BPTR lock;int i,ok=0,old=0,old_token=0,new_token=0;
 if((c->remote_security!=0&&c->remote_security!=1)||(c->push_updates!=0&&c->push_updates!=1)||c->background_budget>120 || c->connection.poll<10 || c->connection.poll>3600 || (c->boot!=0 && c->boot!=1) || (c->sample_mode!=0 && c->sample_mode!=1) || (c->low_memory!=0 && c->low_memory!=1) || (c->reconnect!=0 && c->reconnect!=1) || (c->retry_interval!=60 && c->retry_interval!=600 && c->retry_interval!=900 && c->retry_interval!=1800) || !config_path_valid(c->service_dir) || !config_path_valid(c->prefs_dir) || !config_path_valid(c->startup_dir))return 0;
 token[0]=0;if(c->connection.url[0] || c->token[0])if(!amihass_url(c->connection.url,!strncmp(c->connection.url,"http://",7)) || !amihass_token(c->token,token))return 0;
 lock=CreateDir("ENVARC:AmiHASS");if(lock)UnLock(lock);
 for(i=0;i<20;++i){sprintf(dir,"ENVARC:AmiHASS/Save-%08lx-%d",(ULONG)FindTask(NULL),i);lock=CreateDir(dir);if(lock){UnLock(lock);break;}}
 if(i==20){amihass_erase(token,sizeof(token));return 0;}sprintf(fresh,"%s/new",dir);sprintf(fresh_token,"%s/token",dir);strcpy(backup,AMIHASS_CONFIG_FILE ".old");
 file=fopen(fresh,"wb");if(file){ok=fprintf(file,"AMIHASSCFG1\n%s\n%u %d %d %d %u %d %d %u %d %d\n\n%s\n%s\n%s\n",c->connection.url,c->connection.poll,!strncmp(c->connection.url,"http://",7),c->boot,c->reconnect,c->retry_interval,c->low_memory,c->sample_mode,c->background_budget,c->push_updates,c->remote_security,c->service_dir,c->prefs_dir,c->startup_dir)>0;if(fclose(file))ok=0;}
 if(ok&&token[0])ok=write_token(fresh_token,token);amihass_erase(token,sizeof(token));
 if(ok){lock=Lock(AMIHASS_TOKEN_FILE,ACCESS_READ);if(lock){UnLock(lock);DeleteFile(AMIHASS_TOKEN_FILE ".old");old_token=Rename(AMIHASS_TOKEN_FILE,AMIHASS_TOKEN_FILE ".old")!=0;if(!old_token)ok=0;}}
 if(ok&&c->token[0]){new_token=Rename(fresh_token,AMIHASS_TOKEN_FILE)!=0;if(!new_token)ok=0;}
 if(ok){lock=Lock(AMIHASS_CONFIG_FILE,ACCESS_READ);if(lock){UnLock(lock);DeleteFile(backup);old=Rename(AMIHASS_CONFIG_FILE,backup)!=0;if(!old)ok=0;}}
 if(ok && !Rename(fresh,AMIHASS_CONFIG_FILE))ok=0;
 if(!ok){if(old)Rename(backup,AMIHASS_CONFIG_FILE);if(new_token)DeleteFile(AMIHASS_TOKEN_FILE);if(old_token)Rename(AMIHASS_TOKEN_FILE ".old",AMIHASS_TOKEN_FILE);DeleteFile(fresh);DeleteFile(fresh_token);DeleteFile(dir);return 0;}
 DeleteFile(fresh);DeleteFile(fresh_token);DeleteFile(backup);DeleteFile(AMIHASS_TOKEN_FILE ".old");DeleteFile(dir);
 DeleteFile("ENVARC:AmiHASS/Private/connection");DeleteFile("ENVARC:AmiHASS/Private");DeleteFile("ENVARC:AmiHASS/InstallPath");DeleteFile("ENV:AmiHASS/InstallPath");DeleteFile("ENVARC:AmiHASS/PrefsPath");DeleteFile("ENV:AmiHASS/PrefsPath");DeleteFile("ENVARC:AmiHASS/StartupPath");DeleteFile("ENV:AmiHASS/StartupPath");
 amihass_config_scrub_env();return 1;
}
int amihass_config_forget(void)
{struct AmiHASSConfig c;int ok;if(!amihass_config_load(&c))return 0;c.connection.url[0]=0;amihass_erase(c.token,sizeof(c.token));ok=amihass_config_save(&c);amihass_erase(&c,sizeof(c));return ok;}
