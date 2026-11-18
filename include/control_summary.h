#ifndef AH_CONTROL_SUMMARY_H
#define AH_CONTROL_SUMMARY_H
#include <stdio.h>
#include <string.h>
#include "amihass_core.h"
static void ah_control_summary(const char *entity,const struct AHControls *c,int known,char out[96])
{char path[96],body[180];out[0]=0;if(!known){strcpy(out,"Checking supported controls...");return;}if(amihass_action(entity,"Toggle",path,body))strcpy(out,"On/off");if(c->brightness)strcat(out,out[0] ? ", brightness":"Brightness");if(c->color)strcat(out,out[0] ? ", RGB color":"RGB color");if(c->temperature){char value[40];amihass_number_text(c->minimum,value);snprintf(out,96,"Temperature %s to ",value);amihass_number_text(c->maximum,value);strcat(out,value);strcat(out," ");strcat(out,c->unit);}if(!out[0])strcpy(out,"Read only / no advertised controls");}
#endif
