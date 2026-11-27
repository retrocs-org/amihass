"""Production push IPC/cache boundary with fake local messages; no sockets."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'tests/test_backend.py').read_text();namespace={'__file__':str(root/'tests/test_backend.py')}
exec(compile(source[:source.index('with tempfile.TemporaryDirectory()')],str(root/'tests/test_backend.py'),'exec'),namespace)
header=(root/'include/push_backend.h').read_text().split('static void push_step(')[0]
stubs=r'''
static struct AHPushMessage packet;static int incoming,replies;
static struct Message *GetMsg(struct MsgPort *p){(void)p;if(!incoming)return NULL;incoming=0;return (struct Message*)&packet;}
static int nd_reply_finish(struct Message *m){assert(m==(struct Message*)&packet);++replies;return 0;}
static void send_push(struct Backend *b,int kind){packet.msg.mn_Length=sizeof(packet);packet.magic=AH_PUSH_MAGIC;packet.generation=b->generation;packet.kind=kind;incoming=1;push_events(b);assert(!incoming);}
'''
# send_push needs the production handler declaration before its definition.
stubs=stubs.replace('static void send_push','static void push_events(struct Backend *b);\nstatic void send_push')
main=r'''
int main(void){struct Backend *b=calloc(1,sizeof(*b));int a;struct AmiHASSMessage request;reload(b);connection_ok(b,now);b->network=b->network_known=1;b->next_network=now+60;b->push_enabled=1;b->push_port=(struct MsgPort*)1;
 message_unused: a=subscription(b,"Push","light.desk");memset(&request,0,sizeof(request));strcpy(request.owner,"Push");strcpy(request.entity,"light.desk");assert(select_lease(b,&request,now,30));refresh_leases(b,now);
 memset(&packet,0,sizeof(packet));send_push(b,1);assert(!packet.stop&&packet.count==1&&!strcmp(packet.ids[0],"light.desk"));
 memset(&packet,0,sizeof(packet));strcpy(packet.entity.id,"light.desk");strcpy(packet.entity.state,"on");packet.controls.brightness=packet.controls.level_known=1;packet.controls.level=50;send_push(b,2);assert(b->subscriptions[a].pushed&&b->subscriptions[a].result==AMIHASS_OK&&b->subscriptions[a].controls.level==50&&b->push_updates==1);
 memset(&packet,0,sizeof(packet));packet.ready=1;send_push(b,1);assert(b->push_ready);b->subscriptions[a].due=0;sent_phase=0;schedule(b,"unused");assert(!sent_phase); /* routine polls suppressed */
 b->subscriptions[a].urgent=1;b->subscriptions[a].due=0;schedule(b,"unused");assert(sent_phase==1);b->job=NULL;b->subscriptions[a].urgent=0;
 memset(&packet,0,sizeof(packet));send_push(b,3);assert(!b->push_ready&&!b->subscriptions[a].pushed&&b->subscriptions[a].result==AMIHASS_STALE&&b->push_retry>now);
 memset(&packet,0,sizeof(packet));packet.error=1;send_push(b,3);assert(b->auth_blocked&&!b->connected&&!b->queued);
 memset(&packet,0,sizeof(packet));send_push(b,1);assert(packet.stop);reload(b);b->push_enabled=1;b->network=1;b->low_memory=1;memset(&packet,0,sizeof(packet));send_push(b,1);assert(packet.stop);
 b->low_memory=0;memset(&packet,0,sizeof(packet));send_push(b,1);assert(packet.stop); /* no selections, never unfiltered */
 b->stopping=1;memset(&packet,0,sizeof(packet));send_push(b,1);assert(packet.stop&&replies==9);
 {/* Persistent notifications keep the connection wanted without entities. */struct AmiHASSMessage q;int k;
  b->stopping=0;memset(&q,0,sizeof(q));q.kind=AMIHASS_NOTIFICATIONS;notes_request(b,&q,now);assert(q.result==AMIHASS_WAIT&&b->notes_asked==now);
  memset(&packet,0,sizeof(packet));send_push(b,1);assert(!packet.stop&&!packet.count);
  memset(&packet,0,sizeof(packet));packet.note_op=AH_NOTE_CLEAR;send_push(b,4);memset(&packet,0,sizeof(packet));packet.note_op=AH_NOTE_SET;strcpy(packet.note.id,"a1");strcpy(packet.note.title,"Door");for(k=0;k<200;++k)packet.note.message[k]=(char)('a'+k%26);send_push(b,4);
  memset(&packet,0,sizeof(packet));packet.note_op=AH_NOTE_SET;strcpy(packet.note.id,"b2");strcpy(packet.note.message,"Update ready");send_push(b,4);
  memset(&q,0,sizeof(q));notes_request(b,&q,now);assert(q.result==AMIHASS_OK&&q.count==2&&q.total==2&&!strcmp(q.items[0].id,"a1")&&!strcmp(q.items[0].name,"Door")&&strlen(q.items[0].state)==39&&q.age==b->notes_serial);
  memset(&q,0,sizeof(q));strcpy(q.entity,"a1");notes_request(b,&q,now);assert(q.result==AMIHASS_OK&&q.total==200&&q.count==4&&strlen(q.items[3].name)==11&&q.items[1].name[0]=='l');
  memset(&q,0,sizeof(q));strcpy(q.entity,"zz");notes_request(b,&q,now);assert(q.result==AMIHASS_UNAVAILABLE);
  memset(&packet,0,sizeof(packet));packet.note_op=AH_NOTE_REMOVE;strcpy(packet.note.id,"a1");send_push(b,4);assert(b->notes_count==1&&!strcmp(b->notes[0].id,"b2"));
  for(k=0;k<10;++k){memset(&packet,0,sizeof(packet));packet.note_op=AH_NOTE_SET;sprintf(packet.note.id,"n%d",k);send_push(b,4);}assert(b->notes_count==AH_NOTES&&!strcmp(b->notes[AH_NOTES-1].id,"n9")&&!strcmp(b->notes[0].id,"n2"));
  now+=121;memset(&packet,0,sizeof(packet));send_push(b,1);assert(packet.stop);/* nobody asked for two minutes */
  b->push_enabled=0;memset(&q,0,sizeof(q));notes_request(b,&q,now);assert(q.result==AMIHASS_UNSUPPORTED&&!strcmp(q.item.state,AMIHASS_STATE_LIVE_UPDATES_OFF));}
 {if(b->subscriptions)FreeVec(b->subscriptions);free(b);}puts("Push IPC: selected union, shared controls, routine-poll suppression, urgent REST reads, fallback, auth latch, Low memory, empty-selection refusal and persistent notifications passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#ifndef TEST_PORTS\n#define TEST_PORTS\n#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n#endif\n')
    code=namespace['preamble']+'\n#include "push_worker.h"\n'+namespace['structs']+namespace['stubs']+namespace['functions']+stubs+header+main
    (p/'test.c').write_text(code);cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c')]+[str(root/'src'/name) for name in ('core.c','camera.c','sample.c','browser_data.c')]+['-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
