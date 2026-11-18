#ifndef AH_ACTION_PROGRESS_H
#define AH_ACTION_PROGRESS_H
#include <string.h>
#include "amihass_core.h"
enum AHActionStage {AH_ACTION_IDLE,AH_ACTION_SENDING,AH_ACTION_WAITING,AH_ACTION_CONFIRMED,AH_ACTION_FAILED};
struct AHActionProgress {int stage,error,timed_out;unsigned long started,serial;char expected[20],previous_update[40];};
static int ah_action_pending(const struct AHActionProgress *p)
{return p->stage==AH_ACTION_SENDING||p->stage==AH_ACTION_WAITING;}
static void ah_action_begin(struct AHActionProgress *p,const char *action,const struct AmiHASSEntity *value,unsigned long serial,unsigned long now)
{memset(p,0,sizeof(*p));p->stage=AH_ACTION_SENDING;p->serial=serial;p->started=now;strncpy(p->expected,!strcmp(action,"Toggle") ? (!strcmp(value->state,"on") ? "Turn off":"Turn on"):action,19);strncpy(p->previous_update,value->updated,39);}
static void ah_action_fail(struct AHActionProgress *p,int error)
{if(ah_action_pending(p)){p->stage=AH_ACTION_FAILED;p->error=error;}}
static void ah_action_ack(struct AHActionProgress *p,int accepted,int error)
{if(p->stage!=AH_ACTION_SENDING)return;if(accepted)p->stage=AH_ACTION_WAITING;else ah_action_fail(p,error);}
static void ah_action_tick(struct AHActionProgress *p,unsigned long now)
{if(ah_action_pending(p)&&now-p->started>=450UL){p->stage=AH_ACTION_FAILED;p->timed_out=1;}}
static int ah_action_read(struct AHActionProgress *p,unsigned long serial,const struct AmiHASSEntity *value,const struct AHControls *controls)
{int match;if(p->stage!=AH_ACTION_WAITING||serial==p->serial)return 0;p->serial=serial;match=!strcmp(p->expected,"Turn on") ? !strcmp(value->state,"on"):!strcmp(p->expected,"Turn off") ? !strcmp(value->state,"off"):!strcmp(p->expected,"Activate") ? value->updated[0]&&strcmp(value->updated,p->previous_update):amihass_control_matches(controls,value->state,p->expected);if(match)p->stage=AH_ACTION_CONFIRMED;return match;}
static const char *ah_action_label(const struct AHActionProgress *p)
{switch(p->stage){case AH_ACTION_SENDING:return "Sending";case AH_ACTION_WAITING:return "Waiting for confirmation";case AH_ACTION_CONFIRMED:return "Confirmed";case AH_ACTION_FAILED:return p->timed_out ? "Failed: confirmation timed out":"Failed";default:return "";}}
#endif
