#ifndef AH_ACTION_PROGRESS_H
#define AH_ACTION_PROGRESS_H
#include <string.h>
#include "amihass_core.h"
enum AHActionStage {AH_ACTION_IDLE,AH_ACTION_SENDING,AH_ACTION_WAITING,AH_ACTION_CONFIRMED,AH_ACTION_FAILED,AH_ACTION_DELIVERED};
struct AHActionProgress {int stage,error,timed_out;unsigned long started,serial,previous_delivery,delivery_ticket;char expected[20],previous_update[40],previous_media_title[64];};
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
/* Actions whose result is not a fixed state: a new last_updated confirms. */
static int ah_action_by_update(const char *a)
{return !strcmp(a,"Activate")||!strcmp(a,"Run")||!strcmp(a,"Confirm and run")||!strcmp(a,"Trigger")||!strcmp(a,"Next option")||!strcmp(a,"Previous option")||!strncmp(a,"Option ",7)||!strncmp(a,"Value ",6)||!strncmp(a,"Humidity ",9)||!strcmp(a,"Stop")||!strcmp(a,"Locate")||!strcmp(a,"Stop valve");}
static int ah_action_read(struct AHActionProgress *p,unsigned long serial,const struct AmiHASSEntity *value,const struct AHControls *controls)
{int match;if(p->stage!=AH_ACTION_WAITING||serial==p->serial)return 0;p->serial=serial;if(!strcmp(p->expected,"Press")){if(p->delivery_ticket&&controls->delivery==p->delivery_ticket){p->stage=AH_ACTION_DELIVERED;return 1;}return 0;}match=!strcmp(p->expected,"Turn on") ? (strcmp(value->state,"off")&&strcmp(value->state,"unavailable")&&strcmp(value->state,"unknown")):!strcmp(p->expected,"Turn off") ? !strcmp(value->state,"off"):ah_action_by_update(p->expected) ? value->updated[0]&&strcmp(value->updated,p->previous_update):amihass_control_matches(controls,value->state,p->expected);if(!strcmp(p->expected,"Next")||!strcmp(p->expected,"Previous"))match=(!strcmp(p->expected,"Next") ? controls->media_next:controls->media_previous)&&controls->media_title[0]&&strcmp(controls->media_title,p->previous_media_title);if(match)p->stage=AH_ACTION_CONFIRMED;return match;}
static const char *ah_action_label(const struct AHActionProgress *p)
{switch(p->stage){case AH_ACTION_SENDING:return "Sending";case AH_ACTION_WAITING:return "Waiting for confirmation";case AH_ACTION_DELIVERED:return "Delivered to Home Assistant";case AH_ACTION_CONFIRMED:return "Confirmed";case AH_ACTION_FAILED:return p->timed_out ? "Failed: confirmation timed out":"Failed";default:return "";}}
#endif
