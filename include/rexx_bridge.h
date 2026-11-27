#ifndef AH_REXX_BRIDGE_H
#define AH_REXX_BRIDGE_H
#include "amihass.h"
#define AH_REXX_PORT "AMIHASS"
struct RexxMsg;
/* WAITEVENT: wait until an entity changes (or reaches a given state). */
#define AH_REXX_WAIT_DOMAIN "waitevent"
#define AH_WAIT_MAX 8
/* ids/states: the watched entities; entity..updated: the one that changed. */
struct AHRexxWait {char ids[AH_WAIT_MAX][96],states[AH_WAIT_MAX][40],target[40],entity[96],state[40],previous[40],updated[40];int count,again;unsigned char primed[AH_WAIT_MAX];long deadline,asked;};
/* After a SUBSCRIBE reply: 0 keep waiting, 1 changed/matched, 2 timed out,
 * 3 entity unavailable for good (unsupported or not configured). */
int ah_rexx_wait_check(struct AHRexxWait *w,const struct AmiHASSMessage *reply,long now);
/* One ordinary command and one WAITEVENT can be in progress at a time; the
 * wait has its own SDK client so other commands keep working meanwhile. */
struct AHRexx {struct MsgPort *port;struct AmiHASSClient client,wait_client;struct RexxMsg *pending,*waiter;int kind,again,tries;long asked;struct AmiHASSMessage request;struct AHRexxWait wait;};
int ah_rexx_parse(const char *command,struct AmiHASSMessage *request);
void ah_rexx_format(const struct AmiHASSMessage *message,char *out,unsigned int capacity);
void ah_rexx_open(struct AHRexx *host);
void ah_rexx_step(struct AHRexx *host);
void ah_rexx_close(struct AHRexx *host);
unsigned long ah_rexx_signals(struct AHRexx *host);
#endif
