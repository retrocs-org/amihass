/* See help.h. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "help.h"
#include "amihass_config.h"
#include "nd_interaction.h"
static int exists(const char *path){BPTR l=Lock((STRPTR)path,ACCESS_READ);if(l){UnLock(l);return 1;}return 0;}
int ah_help_open(void)
{
 char path[340],quoted[700],command[760];struct AmiHASSConfig *c;BPTR in,out;LONG rc;int found=0;
 strcpy(path,"PROGDIR:AmiHASS.guide");found=exists(path);
 if(!found&&(c=AllocVec(sizeof(*c),MEMF_PUBLIC|MEMF_CLEAR))!=NULL){if(amihass_config_load(c)&&strlen(c->service_dir)<300){strcpy(path,c->service_dir);found=AddPart(path,"AmiHASS.guide",sizeof(path))&&exists(path);}memset(c->token,0,sizeof(c->token));FreeVec(c);}
 if(!found){strcpy(path,"HELP:english/AmiHASS.guide");found=exists(path);}
 if(!found||!exists("SYS:Utilities/MultiView")||!nd_quote_path(quoted,sizeof(quoted),path))return 0;
 sprintf(command,"SYS:Utilities/MultiView %s",quoted);in=Open("NIL:",MODE_OLDFILE);out=Open("NIL:",MODE_NEWFILE);if(!in||!out){if(in)Close(in);if(out)Close(out);return 0;}
 rc=SystemTags(command,SYS_Input,in,SYS_Output,out,SYS_Asynch,TRUE,NP_StackSize,16384UL,TAG_DONE);if(rc<0){Close(in);Close(out);return 0;}return 1;
}
