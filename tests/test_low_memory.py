"""Exercise the actual idle-reclamation gate without expunging host libraries."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
s=(root/'src/backend.c').read_text();a=s.index('static void reclaim_idle(');b=s.index('static void job_events(',a)
source=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "memory_stats.h"
#define MEMF_FAST 1
static unsigned long AvailMem(int flags){(void)flags;return 1000000UL;}
typedef long BPTR;typedef long LONG;
#define AMIHASS_WAIT 1
#define FETCH_GUARD "AmiHASS.Fetch"
#define MODE_OLDFILE 0
#define MODE_NEWFILE 1
#define SYS_Input 1
#define SYS_Output 2
#define NP_StackSize 3
#define TAG_DONE 0
#define AH_LOG_MEMORY_RECLAIM 0
struct Backend {struct AHMemoryStats memory;int low_memory,job,queued,catalog_state;unsigned long reclaim_due;};
static unsigned long now=100;static int busy,probe,flushes,closes,fail_open;
static unsigned long seconds(void){return now;}
static int present(const char *name){return !strcmp(name,FETCH_GUARD) ? busy:probe;}
static BPTR Open(const char *name,int mode){(void)name;(void)mode;return !fail_open;}
static void Close(BPTR h){assert(h);++closes;}
static LONG SystemTags(const char *cmd,...){assert(!strcmp(cmd,"C:Avail FLUSH"));++flushes;return 0;}
static void ah_log_event(int event,long code){(void)event;assert(code==0);}
'''+s[a:b]+r'''
int main(void){struct Backend b={0};b.reclaim_due=100;reclaim_idle(&b);assert(!flushes);b.low_memory=1;now=99;reclaim_idle(&b);assert(!flushes);
now=100;b.job=1;reclaim_idle(&b);b.job=0;b.queued=1;reclaim_idle(&b);b.queued=0;b.catalog_state=AMIHASS_WAIT;reclaim_idle(&b);b.catalog_state=0;busy=1;reclaim_idle(&b);busy=0;probe=1;reclaim_idle(&b);probe=0;assert(!flushes&&b.reclaim_due);
reclaim_idle(&b);assert(flushes==1&&!b.reclaim_due&&closes==2);reclaim_idle(&b);assert(flushes==1);
b.reclaim_due=100;fail_open=1;reclaim_idle(&b);assert(!b.reclaim_due&&flushes==1&&b.memory.flushes==1);
puts("Low memory: disabled, idle deadline, busy downloader/probe, queued work, one-shot flush and failed-open cleanup passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(source);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
