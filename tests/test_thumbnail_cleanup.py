"""Exercise the production preview transfer timeout/failed-reply cleanup."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'src/camera_thumbs.c').read_text();a=s.index('int ah_thumbs_step(');b=s.index('int ah_thumbs_draw(',a)
code=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define AMIHASS_WAIT 2
#define AMIHASS_BUSY 3
struct AmiHASSClient {void *pending;};
struct AmiHASSMessage {int result;long offset,total;struct {char updated[20];}item;};
struct AHCameraThumbs {int loading,fresh;unsigned long ticks,next,total,offset;time_t started;struct AmiHASSClient client;char status[96],entity[96],version[20];unsigned char *data;};
static int closes,frees,mode;static time_t fixture_now=100;
static time_t mock_time(time_t *p){(void)p;return fixture_now;}
#define time mock_time
static void release(void *p){assert(p);++frees;free(p);}
#define free release
static void amihass_client_close(struct AmiHASSClient *c){c->pending=NULL;++closes;}
static struct AmiHASSMessage *amihass_reply(struct AmiHASSClient *c){if(mode)c->pending=NULL;return NULL;}
static const unsigned char *amihass_snapshot_data(struct AmiHASSMessage *m,unsigned long *n){(void)m;(void)n;return NULL;}
static int amihass_snapshot(struct AmiHASSClient *c,const char *o,const char *e,long offset,const char *v,int fresh){(void)c;(void)o;(void)e;(void)offset;(void)v;(void)fresh;return 0;}
static int amihass_snapshot_size(struct AmiHASSClient *c,const char *o,const char *e,long offset,const char *v,int fresh,int small){(void)small;return amihass_snapshot(c,o,e,offset,v,fresh);}
static int decode(struct AHCameraThumbs *t){(void)t;return 0;}
static void FreeVec(void *p){(void)p;}
'''+s[a:b]+r'''
int main(void){struct AHCameraThumbs t;int i;for(i=0;i<100;++i){memset(&t,0,sizeof(t));t.loading=1;t.started=1;t.total=4096;t.data=malloc(t.total);assert(ah_thumbs_step(&t)==1&&!t.loading&&!t.data&&!t.client.pending);}assert(frees==100);memset(&t,0,sizeof(t));t.loading=1;t.started=100;t.total=2048;t.data=malloc(t.total);t.client.pending=&t;mode=1;assert(ah_thumbs_step(&t)==1&&!t.loading&&!t.data&&!t.client.pending&&frees==101);assert(!ah_thumbs_step(&t)&&frees==101);puts("Thumbnail cleanup: repeated timeout and lost bridge replies release encoded data immediately.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'check.c').write_text(code);command=[os.environ.get('CC','cc'),'-std=c99',str(p/'check.c'),'-o',str(p/'check')]
 if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
 subprocess.run(command,check=True);subprocess.run([str(p/'check')],check=True)
