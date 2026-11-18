from pathlib import Path
import os,re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'SDK/amihass_client.c').read_text()
def fn(name):
 start=s.index(name);start=s.rfind('\n',0,start)+1;brace=s.index('{',start);depth=1;i=brace+1
 while depth:
  if s[i]=='{':depth+=1
  elif s[i]=='}':depth-=1
  i+=1
 return s[start:i]
program=r'''#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "amihass.h"
static int maintenance,running,launches,freed,closed;static unsigned long now;static struct AmiHASSMessage *incoming;
static void Forbid(void){}static void Permit(void){}
static struct MsgPort *FindPort(const char *p){return !strcmp(p,"AmiHASS.Maintenance") ? (maintenance ? (struct MsgPort *)1:NULL):(running ? (struct MsgPort *)1:NULL);}
int amihass_launch(const char *p,const char *args){assert(!strcmp(p,"AmiHASSService")&&!strcmp(args,"SERVICE"));++launches;return 1;}
static void *GetMsg(struct MsgPort *p){struct AmiHASSMessage *m=incoming;(void)p;incoming=NULL;return m;}
static unsigned long ticks(void){return now;}
static void FreeVec(void *p){++freed;free(p);}
void amihass_client_close(struct AmiHASSClient *p){++closed;p->pending=NULL;}
'''+fn('int amihass_start(')+'\n'+fn('struct AmiHASSMessage *amihass_reply(')+r'''
int main(void){struct AmiHASSClient c;struct AmiHASSMessage *m;memset(&c,0,sizeof(c));maintenance=1;assert(!amihass_start(NULL)&&!launches);maintenance=0;running=1;assert(amihass_start(NULL)&&!launches);running=0;assert(amihass_start(NULL)&&launches==1);
c.port=(struct MsgPort *)1;m=calloc(1,sizeof(*m));m->magic=AMIHASS_MAGIC;m->abi=AMIHASS_ABI;m->msg.mn_Length=sizeof(*m);c.pending=m;incoming=m;assert(amihass_reply(&c)==m&&!c.pending);free(m);
m=calloc(1,sizeof(*m));m->magic=AMIHASS_MAGIC;m->abi=99;m->msg.mn_Length=sizeof(*m);incoming=m;c.pending=m;assert(!amihass_reply(&c)&&freed==1&&!c.pending);
c.pending=(struct AmiHASSMessage *)1;c.started=0;now=501;assert(!amihass_reply(&c)&&closed==1);return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'test.c').write_text(program)
 cmd=[os.environ.get('CC','cc'),'-std=c99','-I'+str(p),'-I'+str(ROOT/'SDK'),str(p/'test.c'),'-o',str(p/'test')]
 if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
print('AmiHASS client: maintenance blocks restart, existing service reuse, ABI validation and timeout passed.')
