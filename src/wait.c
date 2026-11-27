#include <exec/types.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdlib.h>
#include "wait.h"
void ah_wait_close(struct AHWait *w){if(w->opened)CloseDevice(w->request);if(w->request)DeleteIORequest(w->request);if(w->port)DeleteMsgPort(w->port);w->port=NULL;w->request=NULL;w->opened=0;}
/* Sleep until one of `signals` arrives or `ticks` (1/50 s) pass; returns the
 * caller's signals that woke it. Ctrl-C is re-armed only if it was asked for,
 * so the service still sees it and GUI loops are never woken by it forever. */
unsigned long ah_wait_ticks(struct AHWait *w,unsigned long signals,unsigned long ticks)
{struct timerequest *t;ULONG bit,woken;if(!ticks)ticks=1;if(!w->opened){w->port=CreateMsgPort();if(w->port)w->request=CreateIORequest(w->port,sizeof(*t));if(!w->request || OpenDevice(TIMERNAME,UNIT_VBLANK,w->request,0)){ah_wait_close(w);Delay(ticks>5 ? 5:ticks);return 0;}w->opened=1;}
 t=(struct timerequest *)w->request;t->tr_node.io_Command=TR_ADDREQUEST;t->tr_time.tv_secs=ticks/50UL;t->tr_time.tv_micro=(ticks%50UL)*20000UL;bit=1UL<<w->port->mp_SigBit;SendIO(w->request);woken=Wait(signals|bit);if(!CheckIO(w->request))AbortIO(w->request);WaitIO(w->request);SetSignal(0L,bit);if(woken&signals&SIGBREAKF_CTRL_C)SetSignal(SIGBREAKF_CTRL_C,SIGBREAKF_CTRL_C);return woken&signals;}
void ah_wait(struct AHWait *w,unsigned long signals)
{(void)ah_wait_ticks(w,signals|SIGBREAKF_CTRL_C,50UL);}
/* Elapsed 1/10 s since *stamp (0 = first call, counts as one step). Lets
 * loops that count steps keep real-time pacing when they sleep longer.
 * A clock step larger than a minute counts as a single step. */
unsigned long ah_elapsed100(unsigned long *stamp)
{struct DateStamp d;unsigned long now,steps;DateStamp(&d);now=((unsigned long)d.ds_Days*1440UL+(unsigned long)d.ds_Minute)*3000UL+(unsigned long)d.ds_Tick;if(!*stamp||now<*stamp||now-*stamp>3000UL){*stamp=now;return 1;}steps=(now-*stamp)/5UL;*stamp+=steps*5UL;return steps;}
/* GUI loops: return at once if `port` already holds input, else sleep until
 * input on it, an `extra` signal (a bridge reply port) or `ticks`/50 s. */
static struct AHWait shared_waiter;static int shared_registered;
static void shared_cleanup(void){ah_wait_close(&shared_waiter);}
void ah_port_wait(struct MsgPort *port,unsigned long extra,unsigned long ticks)
{if(port&&port->mp_MsgList.lh_Head->ln_Succ)return;if(!shared_registered){shared_registered=1;atexit(shared_cleanup);}(void)ah_wait_ticks(&shared_waiter,(port ? 1UL<<port->mp_SigBit:0UL)|extra,ticks);}
