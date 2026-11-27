#ifndef AH_PUSH_WORKER_H
#define AH_PUSH_WORKER_H
#include <exec/ports.h>
#include "push_state.h"
#define AH_PUSH_MAGIC 0x41485033UL
#define AH_PUSH_GUARD "AmiHASS.PushWorker"
/* kind 1 heartbeat/selection, 2 entity update, 3 failure, 4 notification (note_op). */
struct AHPushMessage {struct Message msg;unsigned long magic,generation;int kind,stop,ready,count,error,note_op;char ids[AH_PUSH_SELECTED][96];struct AmiHASSEntity entity;struct AHControls controls;struct AHNote note;};
int amihass_push_worker(const char *parent);
int amihass_http_worker(const char *service);
#endif
