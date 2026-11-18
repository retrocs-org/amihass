#ifndef AH_REXX_BRIDGE_H
#define AH_REXX_BRIDGE_H
#include "amihass.h"
#define AH_REXX_PORT "AMIHASS"
struct RexxMsg;
struct AHRexx {struct MsgPort *port;struct AmiHASSClient client;struct RexxMsg *pending;int kind;};
int ah_rexx_parse(const char *command,struct AmiHASSMessage *request);
void ah_rexx_format(const struct AmiHASSMessage *message,char *out,unsigned int capacity);
void ah_rexx_open(struct AHRexx *host);
void ah_rexx_step(struct AHRexx *host);
void ah_rexx_close(struct AHRexx *host);
unsigned long ah_rexx_signals(struct AHRexx *host);
#endif
