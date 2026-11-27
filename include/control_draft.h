#ifndef AH_CONTROL_DRAFT_H
#define AH_CONTROL_DRAFT_H
#include <string.h>
/* A single latest-value slot. No intermediate slider actions enter the bridge. */
struct AHControlDraft {char action[20];unsigned long due;};
static void ah_control_draft_offer(struct AHControlDraft *d,const char *action,unsigned long tick)
{if(strlen(action)>=sizeof(d->action))return;strcpy(d->action,action);d->due=tick+3;}
static const char *ah_control_draft_ready(struct AHControlDraft *d,unsigned long tick,int busy)
{return !busy&&d->action[0]&&tick>=d->due ? d->action:NULL;}
static void ah_control_draft_clear(struct AHControlDraft *d){d->action[0]=0;}
#endif
