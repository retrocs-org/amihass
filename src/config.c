/* All persistent bridge settings live in one atomically replaced cfg file. */
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amihass_config.h"
#include "amihass.h"
void amihass_config_defaults(struct AmiHASSConfig *c)
{memset(c,0,sizeof(*c));c->connection.poll=30;c->boot=1;c->reconnect=1;c->retry_interval=60;strcpy(c->service_dir,"SYS:Utilities/AmiHASS");strcpy(c->prefs_dir,"SYS:Prefs");strcpy(c->startup_dir,"SYS:WBStartup");}
static int config_line(FILE *file,char *out,unsigned int cap)
{unsigned int n=0;int ch;while((ch=fgetc(file))!=EOF){if(ch=='\n'){out[n]=0;return 1;}if(ch=='\r')continue;if(n+1>=cap)return 0;out[n++]=(char)ch;}return 0;}
static void legacy_path(char *out,unsigned int cap,const char *name)
{FILE *f=fopen(name,"rb");if(f){if(fgets(out,cap,f))out[strcspn(out,"\r\n")]=0;fclose(f);}}
int amihass_config_load(struct AmiHASSConfig *c)
{
 FILE *file;char header[32],numbers[80],raw[1030];int ok=0,fields;
 amihass_config_defaults(c);file=fopen(AMIHASS_CONFIG_FILE,"rb");
 if(file){
  if(config_line(file,header,sizeof(header)) && !strcmp(header,"AMIHASSCFG1") && config_line(file,c->connection.url,sizeof(c->connection.url)) && config_line(file,numbers,sizeof(numbers)) && ((fields=sscanf(numbers,"%u %d %d %d %u %d %d",&c->connection.poll,&c->connection.allow_http,&c->boot,&c->reconnect,&c->retry_interval,&c->low_memory,&c->sample_mode))==3 || fields==5 || fields==6 || fields==7) && config_line(file,c->token,sizeof(c->token)) && config_line(file,c->service_dir,sizeof(c->service_dir)) && config_line(file,c->prefs_dir,sizeof(c->prefs_dir)) && config_line(file,c->startup_dir,sizeof(c->startup_dir)) && fgetc(file)==EOF)ok=1;
  fclose(file);if(ok && c->connection.poll>=10 && c->connection.poll<=3600 && (c->boot==0 || c->boot==1) && (c->sample_mode==0 || c->sample_mode==1) && (c->low_memory==0 || c->low_memory==1) && (c->reconnect==0 || c->reconnect==1) && (c->retry_interval==60 || c->retry_interval==600 || c->retry_interval==900 || c->retry_interval==1800) && c->service_dir[0] && c->prefs_dir[0] && c->startup_dir[0] && ((!c->connection.url[0] && !c->token[0]) || (amihass_url(c->connection.url,!strncmp(c->connection.url,"http://",7)) && amihass_token(c->token,raw)))){c->connection.allow_http=!strncmp(c->connection.url,"http://",7);if(c->token[0])strcpy(c->token,raw);amihass_erase(raw,sizeof(raw));return 1;}
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
static int config_path_valid(const char *path)
{const unsigned char *p=(const unsigned char *)path;if(!*p || strlen(path)>=300)return 0;for(;*p;++p)if(*p<32 || *p==127)return 0;return 1;}
int amihass_config_save(const struct AmiHASSConfig *c)
{
 char token[1025],dir[130],fresh[160],backup[160];FILE *file;BPTR lock;int i,ok=0,old=0;
 if(c->connection.poll<10 || c->connection.poll>3600 || (c->boot!=0 && c->boot!=1) || (c->sample_mode!=0 && c->sample_mode!=1) || (c->low_memory!=0 && c->low_memory!=1) || (c->reconnect!=0 && c->reconnect!=1) || (c->retry_interval!=60 && c->retry_interval!=600 && c->retry_interval!=900 && c->retry_interval!=1800) || !config_path_valid(c->service_dir) || !config_path_valid(c->prefs_dir) || !config_path_valid(c->startup_dir))return 0;
 token[0]=0;if(c->connection.url[0] || c->token[0])if(!amihass_url(c->connection.url,!strncmp(c->connection.url,"http://",7)) || !amihass_token(c->token,token))return 0;
 lock=CreateDir("ENVARC:AmiHASS");if(lock)UnLock(lock);
 for(i=0;i<20;++i){sprintf(dir,"ENVARC:AmiHASS/Save-%08lx-%d",(ULONG)FindTask(NULL),i);lock=CreateDir(dir);if(lock){UnLock(lock);break;}}
 if(i==20){amihass_erase(token,sizeof(token));return 0;}sprintf(fresh,"%s/new",dir);sprintf(backup,"%s/old",dir);
 file=fopen(fresh,"wb");if(file){ok=fprintf(file,"AMIHASSCFG1\n%s\n%u %d %d %d %u %d %d\n%s\n%s\n%s\n%s\n",c->connection.url,c->connection.poll,!strncmp(c->connection.url,"http://",7),c->boot,c->reconnect,c->retry_interval,c->low_memory,c->sample_mode,token,c->service_dir,c->prefs_dir,c->startup_dir)>0;if(fclose(file))ok=0;}amihass_erase(token,sizeof(token));
 if(ok){lock=Lock(AMIHASS_CONFIG_FILE,ACCESS_READ);if(lock){UnLock(lock);old=Rename(AMIHASS_CONFIG_FILE,backup)!=0;if(!old)ok=0;}}
 if(ok && !Rename(fresh,AMIHASS_CONFIG_FILE))ok=0;
 if(!ok && old && !Rename(backup,AMIHASS_CONFIG_FILE)){DeleteFile(fresh);return 0;}
 DeleteFile(fresh);if(ok && old)DeleteFile(backup);DeleteFile(dir);
 if(ok){DeleteFile("ENVARC:AmiHASS/Private/connection");DeleteFile("ENVARC:AmiHASS/InstallPath");DeleteFile("ENV:AmiHASS/InstallPath");DeleteFile("ENVARC:AmiHASS/PrefsPath");DeleteFile("ENV:AmiHASS/PrefsPath");DeleteFile("ENVARC:AmiHASS/StartupPath");DeleteFile("ENV:AmiHASS/StartupPath");}
 return ok;
}
int amihass_config_forget(void)
{struct AmiHASSConfig c;int ok;if(!amihass_config_load(&c))return 0;c.connection.url[0]=0;amihass_erase(c.token,sizeof(c.token));ok=amihass_config_save(&c);amihass_erase(&c,sizeof(c));return ok;}
