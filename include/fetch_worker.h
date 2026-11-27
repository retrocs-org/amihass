#ifndef ND_FETCH_WORKER_H
#define ND_FETCH_WORKER_H
#include "nd_reply.h"
#define FETCH_MAGIC 0x4e445746UL
#ifndef FETCH_GUARD
#define FETCH_GUARD "AmiHASS.Fetch"
#endif
#ifndef FETCH_PREFIX
#define FETCH_PREFIX "T:AmiHASS-"
#endif
#ifndef FETCH_DIR_VALID
#define FETCH_DIR_VALID(path) (!strncmp((path),FETCH_PREFIX,sizeof(FETCH_PREFIX)-1))
#endif
#ifndef FETCH_STACK_SIZE
#define FETCH_STACK_SIZE 131072UL
#endif
struct FetchMessage {struct Message msg;ULONG magic;int kind;LONG result;char command[1800],directory[160];};
/* Monotonic ticks (1/50 s); a wall-clock step is absorbed, not a timeout. */
static ULONG weather_ticks(void)
{static ULONG mono=1UL,last;static int started;struct DateStamp d;ULONG wall;DateStamp(&d);wall=((ULONG)d.ds_Days*1440UL+(ULONG)d.ds_Minute)*3000UL+(ULONG)d.ds_Tick;if(started&&wall>=last&&wall-last<=3000UL)mono+=wall-last;started=1;last=wall;return mono;}
static int fetch_send(const char *name,struct FetchMessage *m)
{struct MsgPort *p;int sent=0;Forbid();p=FindPort((STRPTR)name);if(p){PutMsg(p,(struct Message *)m);sent=1;}Permit();return sent;}
static void fetch_clean(const char *directory)
{
    char path[200];if(!directory || !FETCH_DIR_VALID(directory) || strlen(directory)>150)return;
    sprintf(path,"%s/response",directory);DeleteFile(path);sprintf(path,"%s/fetch.log",directory);DeleteFile(path);
#ifdef FETCH_PROGRESS_FILE
    sprintf(path,"%s/%s",directory,FETCH_PROGRESS_FILE);DeleteFile(path);
#endif
    DeleteFile((STRPTR)directory);
}
static void fetch_close_port(struct MsgPort *port)
{
    struct FetchMessage *m;if(!port)return;
    Forbid();RemPort(port);Permit();
    while((m=(struct FetchMessage *)GetMsg(port))!=NULL){
        if(m->kind==1 && m->msg.mn_ReplyPort){m->kind=-1;nd_reply_finish((struct Message *)m);}
        else{fetch_clean(m->directory);FreeVec(m);}
    }
    DeleteMsgPort(port);
}
/* A separately loaded process owns the downloader and its unique directory.
 * If its resident quits or times out, it finishes and cleans up by itself. */
static int fetch_worker(const char *parent)
{
    struct MsgPort *guard=CreateMsgPort(),*reply=nd_reply_port();
    struct FetchMessage *m=AllocVec(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);
    BPTR input,output;char log[200],quoted_log[430],command[2300];int owner=0,replied=0,i;
    if(!guard || !reply || !m){if(guard)DeleteMsgPort(guard);if(reply)nd_reply_delete(reply);if(m)FreeVec(m);return 20;}
    ((struct Process *)FindTask(NULL))->pr_WindowPtr=(APTR)-1;
    Forbid();if(!FindPort(FETCH_GUARD)){guard->mp_Node.ln_Name=FETCH_GUARD;AddPort(guard);owner=1;}Permit();
    if(!owner){DeleteMsgPort(guard);nd_reply_delete(reply);FreeVec(m);return 10;}
    m->magic=FETCH_MAGIC;m->kind=1;m->msg.mn_Length=sizeof(*m);m->msg.mn_ReplyPort=reply;
    if(fetch_send(parent,m)){
        for(i=0;i<150 && !replied;i+=2){if(GetMsg(reply))replied=1;else Delay(2);}
        Forbid();if(!replied && GetMsg(reply))replied=1;Permit();
        /* Abandon atomically: the service frees the packet and port when it
         * answers later (nd_reply_finish), so nothing leaks either way. */
        if(!replied){nd_reply_abandon(reply);Forbid();RemPort(guard);Permit();DeleteMsgPort(guard);return 20;}
        nd_reply_delete(reply);reply=NULL;
        if(m->kind==1 && m->command[0]){
            sprintf(log,"%s/fetch.log",m->directory);input=Open("NIL:",MODE_OLDFILE);output=Open("NIL:",MODE_NEWFILE);
            /* Keep Shell return-code diagnostics out of curl's numeric status.
             * The command's stdout alone is redirected; its response uses --output. */
            command[0]=0;if(nd_quote_path(quoted_log,sizeof(quoted_log),log)&&strlen(m->command)+strlen(quoted_log)+3<sizeof(command))sprintf(command,"%s >%s",m->command,quoted_log);
            m->result=-1;
            if(input && output && command[0])m->result=SystemTags(command,SYS_Input,input,SYS_Output,output,NP_Error,output,NP_CloseError,FALSE,NP_Priority,-1L,NP_StackSize,FETCH_STACK_SIZE,TAG_DONE);
            if(input)Close(input);if(output)Close(output);
            Forbid();RemPort(guard);Permit();DeleteMsgPort(guard);guard=NULL;
            m->kind=2;m->msg.mn_ReplyPort=NULL;
            if(fetch_send(parent,m))m=NULL;else fetch_clean(m->directory);
        }
    }
    if(reply)nd_reply_delete(reply);if(m)FreeVec(m);
    if(guard){Forbid();RemPort(guard);Permit();DeleteMsgPort(guard);}return 0;
}
#endif
