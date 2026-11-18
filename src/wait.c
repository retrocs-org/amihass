#include <exec/types.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "wait.h"
void ah_wait_close(struct AHWait *w){if(w->opened)CloseDevice(w->request);if(w->request)DeleteIORequest(w->request);if(w->port)DeleteMsgPort(w->port);w->port=NULL;w->request=NULL;w->opened=0;}
void ah_wait(struct AHWait *w,unsigned long signals)
{struct timerequest *t;ULONG bit,woken;if(!w->opened){w->port=CreateMsgPort();if(w->port)w->request=CreateIORequest(w->port,sizeof(*t));if(!w->request || OpenDevice(TIMERNAME,UNIT_VBLANK,w->request,0)){ah_wait_close(w);Delay(5);return;}w->opened=1;}
 t=(struct timerequest *)w->request;t->tr_node.io_Command=TR_ADDREQUEST;t->tr_time.tv_secs=1;t->tr_time.tv_micro=0;bit=1UL<<w->port->mp_SigBit;SendIO(w->request);woken=Wait(signals|bit|SIGBREAKF_CTRL_C);if(!CheckIO(w->request))AbortIO(w->request);WaitIO(w->request);SetSignal(0L,bit);if(woken&SIGBREAKF_CTRL_C)SetSignal(SIGBREAKF_CTRL_C,SIGBREAKF_CTRL_C);}
