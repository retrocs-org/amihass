#ifndef AH_PUSH_STATE_H
#define AH_PUSH_STATE_H
#include "amihass_core.h"
#define AH_PUSH_SELECTED 48
/* Only the attributes AmiHASS reads are kept (see amihass_attribute_wanted). */
#define AH_PUSH_ATTRS 1024
/* One Home Assistant subscribe_entities subscription per selected entity, so
 * the selection can change without reconnecting and one oversized entity only
 * falls back to REST polling instead of breaking the whole connection. */
struct AHPushEntity {char id[96],state[40],attrs[AH_PUSH_ATTRS];unsigned long sub;int valid;};
struct AHPushState {struct AHPushEntity entities[AH_PUSH_SELECTED];unsigned int count;unsigned long next_id;};
typedef int (*AHPushUpdate)(void*,const struct AmiHASSEntity*,const struct AHControls*);
int ah_push_select(struct AHPushState*,const char ids[][96],unsigned int count);
int ah_push_find(const struct AHPushState*,const char *id);
int ah_push_add(struct AHPushState*,const char *id);
int ah_push_remove(struct AHPushState*,const char *id,unsigned long *sub);
int ah_push_event(struct AHPushState*,const char *json,AHPushUpdate,void*);
/* Home Assistant persistent notifications (persistent_notification/subscribe).
 * op: AH_NOTE_CLEAR before a full list, AH_NOTE_SET to add or replace one,
 * AH_NOTE_REMOVE to drop one. Text is cut to whole glyphs. */
#define AH_NOTE_CLEAR 1
#define AH_NOTE_SET 2
#define AH_NOTE_REMOVE 3
#define AH_NOTES 8
struct AHNote {char id[64],title[64],message[256],created[40];};
typedef int (*AHNoteUpdate)(void*,int op,const struct AHNote*);
/* Returns 0 for malformed JSON or a failed callback; 1 otherwise. */
int ah_push_notifications(const char *event,AHNoteUpdate,void*);
#endif
