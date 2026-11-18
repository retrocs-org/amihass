from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'src/main.c').read_text();start=s.index('static int service_present(');end=s.index('int main(',start)
program=r'''#include <assert.h>
#include <stdlib.h>
#define AMIHASS_PORT "AmiHASS.Service"
#define AMIHASS_STOP 6
#define AMIHASS_OK 1
struct AmiHASSClient {int unused;};struct AmiHASSMessage {int result;};
static int live,respond,accepted,stop_after,closes,ticks;
static void Forbid(void){}static void Permit(void){}
static void *FindPort(const char *p){(void)p;return live ? (void *)1:NULL;}
static void Delay(int n){(void)n;if(++ticks==stop_after)live=0;}
static void FreeVec(void *p){free(p);}
static void amihass_client_close(struct AmiHASSClient *c){(void)c;++closes;}
static int exchange(struct AmiHASSClient *c,int k,const char *e,const char *a,struct AmiHASSMessage **m){(void)c;(void)e;(void)a;assert(k==AMIHASS_STOP);if(!respond)return 0;*m=malloc(sizeof(**m));(*m)->result=accepted;return 1;}
'''+s[start:end]+r'''
int main(void){struct AmiHASSClient c;
live=0;assert(stop_service(&c)==0&&closes==1);
live=1;assert(stop_service(&c)==20&&closes==2);
respond=1;accepted=0;assert(stop_service(&c)==20&&closes==3);
accepted=1;assert(stop_service(&c)==20&&closes==4);
ticks=0;stop_after=3;assert(stop_service(&c)==0&&closes==5&&!live);return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(program);cmd=[os.environ.get('CC','cc'),'-std=c99',str(p/'test.c'),'-o',str(p/'test')]
 if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
print('AmiHASS shutdown: absent, unresponsive, rejected, still-running and confirmed stopped service passed.')
