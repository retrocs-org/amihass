"""Exercise SDK close with in-flight replies and fire-and-forget lease cleanup."""
import os,re,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'SDK/amihass_client.c').read_text()
def fn(signature):
    a=s.index(signature);b=s.index('{',a);depth=1;i=b+1
    while depth:
        if s[i]=='{':depth+=1
        elif s[i]=='}':depth-=1
        i+=1
    return s[a:i]
code=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "amihass.h"
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
static int live,available=1,queued;static struct AmiHASSMessage *queue[20],*incoming;
static void *AllocVec(unsigned long n,int flags){void *p=calloc(1,n);(void)flags;assert(p);++live;return p;}
static void FreeVec(void *p){assert(p);--live;free(p);}
static unsigned long ticks(void){return 100;}
static const char (*send_ids)[96];static unsigned int send_count;
static void Forbid(void){}static void Permit(void){}
static struct MsgPort *FindPort(const char *name){return available&&!strcmp(name,AMIHASS_PORT) ? (struct MsgPort *)1:NULL;}
static void PutMsg(struct MsgPort *p,struct Message *m){assert(p==(struct MsgPort *)1);queue[queued++]=(struct AmiHASSMessage *)m;}
static struct MsgPort *nd_reply_port(void){return AllocVec(sizeof(struct MsgPort),0);}
static void nd_reply_delete(struct MsgPort *p){FreeVec(p);}
static void nd_reply_abandon(struct MsgPort *p){p->abandoned=1;}
static void *GetMsg(struct MsgPort *p){struct AmiHASSMessage *m=incoming;(void)p;incoming=NULL;return m;}
'''
for name in ('static int client_entity_id(','static int send_request(','int amihass_query_many(','int amihass_send(','int amihass_send_scope(','int amihass_send_area(','int amihass_subscribe(','int amihass_unsubscribe(','static void release_selections(','struct AmiHASSMessage *amihass_reply(','void amihass_client_close('):code+=fn(name)+'\n'
code+=r'''
int main(void){struct AmiHASSClient c;struct MsgPort *port;struct AmiHASSMessage *m;memset(&c,0,sizeof(c));assert(!amihass_subscribe(&c,"Client","light.room",1)&&!queued&&!live);assert(amihass_subscribe(&c,"Client","light.room",300));port=c.port;assert(queued==1&&queue[0]->kind==AMIHASS_SUBSCRIBE&&queue[0]->refresh==AMIHASS_QUERY_INTERVAL(300));assert(!amihass_subscribe(&c,"Client","light.room",30));amihass_client_close(&c);assert(!c.port&&!c.pending&&port->abandoned&&queued==2);assert(queue[1]->kind==AMIHASS_UNSUBSCRIBE&&!queue[1]->msg.mn_ReplyPort&&queue[1]->age==(ULONG)(unsigned long)port);FreeVec(port);FreeVec(queue[0]);FreeVec(queue[1]);assert(!live);
 queued=0;assert(amihass_send_area(&c,AMIHASS_DEVICES,"Browser",NULL,16,0,"light","desk","Kitchen"));m=queue[0];assert(!strcmp(m->item.id,"Kitchen")&&m->offset==16);incoming=m;m->count=0;m->total=0;assert(amihass_reply(&c)==m&&!c.pending);FreeVec(m);amihass_client_close(&c);assert(queued==2&&!queue[1]->msg.mn_ReplyPort);FreeVec(queue[1]);assert(!live);
 {char ids[2][96];queued=0;strcpy(ids[0],"light.room");strcpy(ids[1],"sensor.temperature_1");assert(amihass_query_many(&c,"Multi",(const char (*)[96])ids,2,0,0));m=queue[0];assert(m->kind==AMIHASS_QUERY_MANY&&m->count==2&&!strcmp(m->items[1].id,ids[1]));assert(!amihass_query_many(&c,"Multi",(const char (*)[96])ids,2,0,0));incoming=m;assert(amihass_reply(&c)==m);FreeVec(m);amihass_client_close(&c);FreeVec(queue[1]);assert(!live);queued=0;strcpy(ids[0],"../invalid");assert(!amihass_query_many(&c,"Multi",(const char (*)[96])ids,2,0,0)&&!queued&&!live);strcpy(ids[0],"light.Room");assert(!amihass_query_many(&c,"Multi",(const char (*)[96])ids,1,0,0));strcpy(ids[0],"light.room");assert(!amihass_query_many(&c,"Multi",(const char (*)[96])ids,0,0,0)&&!amihass_query_many(&c,"Multi",(const char (*)[96])ids,17,0,0));}
 queued=0;available=0;assert(!amihass_subscribe(&c,"Client","light.room",30));amihass_client_close(&c);assert(!live&&!queued);return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir();(p/'exec/ports.h').write_text('typedef unsigned long ULONG;typedef long LONG;struct MsgPort {int abandoned;};struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'test.c').write_text(code)
    cmd=[os.environ.get('CC','cc'),'-std=c99','-I'+str(p),'-I'+str(ROOT/'SDK'),str(p/'test.c'),'-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
print('SDK subscriptions: pending close, lease cleanup ownership, scoped requests and absent service passed.')
