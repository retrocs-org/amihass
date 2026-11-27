/* Installer lifecycle and boot setup; persistent settings use AmiHASS.cfg. */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nd_version.h"
#include "amihass_config.h"
const char nd_version[]=ND_VER_STRING("SetupStartup");
long __stack=65536;
struct Library *IconBase=NULL;
void amihass_erase(void *p,unsigned long n){volatile unsigned char *b=p;while(n--)*b++=0;}
static int running(void){int live;Forbid();live=FindPort("AmiHASS.Service")!=NULL;Permit();return live;}
static int erase_file(const char *path){BPTR lock=Lock((STRPTR)path,ACCESS_READ);if(!lock)return 1;UnLock(lock);return DeleteFile((STRPTR)path)!=0;}
static int quoted(char *out,unsigned int cap,const char *in)
{unsigned int n=0;if(cap<3)return 0;out[n++]='"';while(*in){if((unsigned char)*in<32 || n+4>=cap)return 0;if(*in=='*' || *in=='"')out[n++]='*';out[n++]=*in++;}out[n++]='"';out[n]=0;return 1;}
static int launch(const char *program,const char *argument)
{char q[1100],command[1200];BPTR input,output;LONG rc;if(!quoted(q,sizeof(q),program))return 0;sprintf(command,"%s%s%s",q,argument ? " ":"",argument ? argument:"");input=Open("NIL:",MODE_OLDFILE);output=Open("NIL:",MODE_NEWFILE);if(!input||!output){if(input)Close(input);if(output)Close(output);return 0;}rc=SystemTags(command,SYS_Input,input,SYS_Output,output,SYS_Asynch,TRUE,NP_StackSize,65536UL,TAG_DONE);if(rc<0){Close(input);Close(output);return 0;}return 1;}
static int export_paths(const struct AmiHASSConfig *c)
{BPTR lock=CreateDir("ENV:AmiHASS");if(lock)UnLock(lock);return SetVar("AmiHASS/InstallerService",(STRPTR)c->service_dir,-1,GVF_GLOBAL_ONLY) && SetVar("AmiHASS/InstallerPrefs",(STRPTR)c->prefs_dir,-1,GVF_GLOBAL_ONLY) && SetVar("AmiHASS/InstallerStartup",(STRPTR)c->startup_dir,-1,GVF_GLOBAL_ONLY) && SetVar("AmiHASS/InstallerBoot",c->boot ? "1":"0",-1,GVF_GLOBAL_ONLY);}
static void clean_exports(void)
{DeleteVar("AmiHASS/InstallerService",GVF_GLOBAL_ONLY);DeleteVar("AmiHASS/InstallerPrefs",GVF_GLOBAL_ONLY);DeleteVar("AmiHASS/InstallerStartup",GVF_GLOBAL_ONLY);DeleteVar("AmiHASS/InstallerBoot",GVF_GLOBAL_ONLY);}
static int boot_entry(const char *directory,const char *program,int enabled)
{
 char alias[520],icon[530];BPTR lock,file;struct DiskObject *disk;STRPTR previous,*old_types,*types;int i,count=0,has_flag=0,ok=0,created=0;
 strcpy(alias,directory);if(!AddPart(alias,"AmiHASS",sizeof(alias)))return 0;sprintf(icon,"%s.info",alias);
 if(!enabled)return erase_file(alias)&&erase_file(icon);
 lock=CreateDir((STRPTR)directory);if(lock)UnLock(lock);
 IconBase=OpenLibrary("icon.library",36);if(!IconBase)return 0;disk=GetDiskObject(alias);
 if(!disk){file=Open(alias,MODE_NEWFILE);if(!file)goto done;Close(file);created=1;disk=GetDefDiskObject(WBPROJECT);if(!disk)goto done;}
 previous=disk->do_DefaultTool;old_types=disk->do_ToolTypes;
 if(old_types){while(old_types[count])++count;has_flag=FindToolType(old_types,"DONOTWAIT")!=NULL;}
 types=malloc((count+(has_flag ? 1:2))*sizeof(STRPTR));if(!types){FreeDiskObject(disk);goto done;}
 for(i=0;i<count;++i)types[i]=old_types[i];if(!has_flag)types[count++]="DONOTWAIT";types[count]=NULL;
 disk->do_DefaultTool=(STRPTR)program;disk->do_ToolTypes=types;disk->do_StackSize=65536;
 ok=PutDiskObject(alias,disk)!=0;disk->do_DefaultTool=previous;disk->do_ToolTypes=old_types;free(types);FreeDiskObject(disk);
done:if(!ok && created)DeleteFile(alias);CloseLibrary(IconBase);IconBase=NULL;return ok;
}
static int purge_tree(const char *path,int depth)
{
 BPTR lock;struct FileInfoBlock *fib;char child[600];int ok=1;
 if(depth>8)return 0;lock=Lock((STRPTR)path,ACCESS_READ);if(!lock)return 1;
 fib=AllocDosObject(DOS_FIB,NULL);if(!fib){UnLock(lock);return 0;}
 if(!Examine(lock,fib)){FreeDosObject(DOS_FIB,fib);UnLock(lock);return 0;}
 if(fib->fib_DirEntryType==2){while(ExNext(lock,fib)){strcpy(child,path);if(!AddPart(child,fib->fib_FileName,sizeof(child)) || !purge_tree(child,depth+1)){ok=0;break;}}}
 FreeDosObject(DOS_FIB,fib);UnLock(lock);return ok && erase_file(path);
}
static int purge_settings(void)
{
 BPTR lock;struct FileInfoBlock *fib;char child[600];int ok=1;
 if(!purge_tree("ENVARC:AmiHASS/Private",0))return 0;
 lock=Lock("ENVARC:AmiHASS",ACCESS_READ);
 if(lock){fib=AllocDosObject(DOS_FIB,NULL);if(!fib){UnLock(lock);return 0;}
 if(!Examine(lock,fib))ok=0;else while(ExNext(lock,fib)){if(!strncmp(fib->fib_FileName,"Save-",5)){strcpy(child,"ENVARC:AmiHASS/");strcat(child,fib->fib_FileName);if(!purge_tree(child,0)){ok=0;break;}}}
 FreeDosObject(DOS_FIB,fib);UnLock(lock);}
 if(!ok)return 0;
 return erase_file("ENVARC:AmiHASS/CameraRefresh.cfg") && erase_file("ENVARC:AmiHASS/CameraRefresh.cfg.old") && erase_file("ENVARC:AmiHASS/CameraRefresh.cfg.new") && erase_file("ENVARC:AmiHASS/Favourites.cfg") && erase_file("ENVARC:AmiHASS/Favourites.cfg.old") && erase_file("ENVARC:AmiHASS/Favourites.cfg.new") && erase_file("ENVARC:AmiHASS/Servers.token") && erase_file("ENVARC:AmiHASS/Servers.token.new") && erase_file("ENVARC:AmiHASS/Servers.token.old") && erase_file("ENVARC:AmiHASS/Servers.cfg") && erase_file("ENVARC:AmiHASS/Servers.cfg.new") && erase_file("ENVARC:AmiHASS/Servers.cfg.old") && erase_file("ENVARC:AmiHASS/Dashboard.cfg") && erase_file("ENVARC:AmiHASS/DashboardPages.cfg") && erase_file("ENVARC:AmiHASS/DashboardPages.cfg.new") && erase_file("ENVARC:AmiHASS/DashboardPages.cfg.old") && erase_file("ENVARC:AmiHASS/Dashboard.cfg.new") && erase_file("ENVARC:AmiHASS/Dashboard.cfg.old") && erase_file(AMIHASS_CONFIG_FILE) && erase_file(AMIHASS_CONFIG_FILE ".old") && erase_file(AMIHASS_TOKEN_FILE) && erase_file(AMIHASS_TOKEN_FILE ".old") && erase_file("ENV:AmiHASS/AmiHASS.cfg") && erase_file("ENV:AmiHASS/AmiHASS.cfg.old") && erase_file("ENV:AmiHASS/AmiHASS.token") && erase_file("ENV:AmiHASS/AmiHASS.token.old") && erase_file("ENVARC:AmiHASS/InstallPath") && erase_file("ENVARC:AmiHASS/PrefsPath") && erase_file("ENVARC:AmiHASS/StartupPath");
}
int main(int argc,char **argv)
{
 struct AmiHASSConfig config,old;char program[520],directory[520];int ok=0,i;
 if(argc>1 && !strcmp(argv[1],"PURGE"))return purge_settings() ? 0:20;
 if(argc>1 && !strcmp(argv[1],"STATE"))return running() ? 0:5;
 if(argc>1 && !strcmp(argv[1],"CLEAN")){clean_exports();return 0;}
 if(argc>2 && (!strcmp(argv[1],"RESTART") || !strcmp(argv[1],"OPEN"))){
  if(!launch(argv[2],!strcmp(argv[1],"RESTART") ? "RESTORE":NULL))return 20;
  if(!strcmp(argv[1],"RESTART")){for(i=0;i<100;++i){if(running())return 0;Delay(5);}return 20;}return 0;
 }
 if(!amihass_config_load(&config))return 20;
 if(argc>1 && !strcmp(argv[1],"EXPORT")){ok=export_paths(&config);amihass_erase(&config,sizeof(config));return ok ? 0:20;}
 old=config;
 if(argc<2 || strlen(argv[1])>=sizeof(program)){amihass_erase(&config,sizeof(config));return 20;}
 strcpy(program,argv[1]);strcpy(directory,program);*FilePart(directory)=0;if(strlen(directory)>=sizeof(config.service_dir))goto done;strcpy(config.service_dir,directory);
 if(argc>2)config.boot=!strcmp(argv[2],"ON");
 if(argc>3){if(strlen(argv[3])>=sizeof(config.prefs_dir))goto done;strcpy(config.prefs_dir,argv[3]);}
 if(argc>4){if(strlen(argv[4])>=sizeof(config.startup_dir))goto done;strcpy(config.startup_dir,argv[4]);}
 if(!boot_entry(config.startup_dir,program,config.boot))goto done;
 if(strcmp(old.startup_dir,config.startup_dir) && !boot_entry(old.startup_dir,program,0))goto done;
 ok=amihass_config_save(&config);
done:amihass_erase(&old,sizeof(old));amihass_erase(&config,sizeof(config));return ok ? 0:20;
}
