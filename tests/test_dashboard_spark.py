"""History-graph tiles: paged fetch, WAIT retry, refresh period, entity change and shape scaling."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dashboard_spark.h"
static int sends,last_offset,result=AMIHASS_OK,points=40;static char last_entity[96];static struct AmiHASSMessage *queued;
void FreeVec(void *p){free(p);}
int amihass_history(struct AmiHASSClient *c,const char *owner,const char *entity,int hours,int offset){int i;struct AmiHASSMessage *m;(void)owner;assert((hours==24||hours==6||hours==1)&&!c->pending);++sends;last_offset=offset;strcpy(last_entity,entity);m=calloc(1,sizeof(*m));m->result=result;m->offset=offset;m->total=result==AMIHASS_OK?points:0;for(i=0;i<AMIHASS_PAGE&&offset+i<m->total;++i)sprintf(m->items[i].state,"%d.5",offset+i);m->count=result==AMIHASS_OK?i:0;queued=m;c->pending=m;return 1;}
struct AmiHASSMessage *amihass_reply(struct AmiHASSClient *c){struct AmiHASSMessage *m=queued;queued=NULL;c->pending=NULL;return m;}
void amihass_client_close(struct AmiHASSClient *c){if(queued){free(queued);queued=NULL;}c->pending=NULL;}
unsigned long amihass_client_signal(const struct AmiHASSClient *c){(void)c;return 0;}
int main(void){static struct AHSparks s;static struct AHDashboardLayout l;int ys[64],n,i;time_t now=1000;memset(&l,0,sizeof(l));ah_sparks_init(&s);
 strcpy(l.tiles[2].entity,"sensor.power");l.tiles[2].display=AH_SPARK_DISPLAY;strcpy(l.tiles[3].entity,"sensor.other");
 ah_sparks_step(&s,&l,now);assert(sends==1&&s.tile==2&&!strcmp(last_entity,"sensor.power"));
 while(s.tile>=0)ah_sparks_step(&s,&l,now);assert(sends==3&&last_offset==32&&s.count[2]==40&&s.value[2][39]==3950&&!s.count[3]);
 n=ah_sparks_shape(&s,2,10,ys,64);assert(n==40&&ys[0]==9&&ys[39]==0);for(i=1;i<n;++i)assert(ys[i]<=ys[i-1]);
 ah_sparks_step(&s,&l,now+10);assert(sends==3);/* fresh for five minutes */
 result=AMIHASS_WAIT;ah_sparks_step(&s,&l,now+AH_SPARK_SECONDS);assert(sends==4);ah_sparks_step(&s,&l,now+AH_SPARK_SECONDS);assert(s.again);ah_sparks_step(&s,&l,now+AH_SPARK_SECONDS);assert(sends==4);
 result=AMIHASS_OK;points=5;ah_sparks_step(&s,&l,now+AH_SPARK_SECONDS+1);assert(sends==5);ah_sparks_step(&s,&l,now+AH_SPARK_SECONDS+1);assert(s.tile<0&&s.count[2]==5);
 result=AMIHASS_FAILED;ah_sparks_step(&s,&l,now+2*AH_SPARK_SECONDS+1);ah_sparks_step(&s,&l,now+2*AH_SPARK_SECONDS+1);assert(s.count[2]==5&&s.tile<0);/* old graph kept */
 strcpy(l.tiles[2].entity,"sensor.new");ah_sparks_step(&s,&l,now+2*AH_SPARK_SECONDS+2);assert(!s.count[2]&&!strcmp(last_entity,"sensor.new"));ah_sparks_close(&s);
 l.tiles[2].display=0;s.count[2]=3;ah_sparks_step(&s,&l,now+3*AH_SPARK_SECONDS);assert(!s.count[2]);
 s.count[5]=2;s.value[5][0]=-99999999;s.value[5][1]=99999999;n=ah_sparks_shape(&s,5,64,ys,64);assert(n==2&&ys[0]==63&&ys[1]==0);s.value[5][1]=-99999999;n=ah_sparks_shape(&s,5,64,ys,64);assert(ys[0]==31&&ys[1]==31);
 {long lo,hi;char t[16];memset(&s,0,sizeof(s));ah_sparks_init(&s);s.count[1]=4;s.value[1][0]=1000;s.value[1][1]=AH_SPARK_GAP;s.value[1][2]=3000;s.value[1][3]=2000;
  assert(ah_sparks_range(&s,1,&lo,&hi)&&lo==1000&&hi==3000);n=ah_sparks_shape(&s,1,11,ys,64);assert(n==4&&ys[0]==10&&ys[1]==-1&&ys[2]==0&&ys[3]==5);
  ah_sparks_text(2150,t);assert(!strcmp(t,"21.5"));ah_sparks_text(-49,t);assert(!strcmp(t,"-0.5"));ah_sparks_text(123456,t);assert(!strcmp(t,"1235"));
  s.count[1]=2;s.value[1][0]=s.value[1][1]=AH_SPARK_GAP;assert(!ah_sparks_range(&s,1,&lo,&hi)&&!ah_sparks_shape(&s,1,11,ys,64));
  /* the graph window follows the reading: 6 h and 1 h */
  memset(&l,0,sizeof(l));strcpy(l.tiles[0].entity,"sensor.p");l.tiles[0].display=6;sends=0;result=AMIHASS_OK;points=3;ah_sparks_step(&s,&l,5000);assert(sends==1&&s.hours[0]==6);while(s.tile>=0)ah_sparks_step(&s,&l,5000);l.tiles[0].display=7;ah_sparks_step(&s,&l,5001);assert(sends==2&&s.hours[0]==1&&!s.count[0]);while(s.tile>=0)ah_sparks_step(&s,&l,5001);}
 ah_sparks_reset(&s);puts("Dashboard history graphs: paged fetch, WAIT retry, five-minute refresh, kept graph on failure, entity change and 32-bit scaling passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'t.c').write_text(program)
    cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),'-DAH_HOST_TEST',str(p/'t.c'),str(root/'src/dashboard_spark.c'),str(root/'src/core.c'),'-o',str(p/'t')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'t')],check=True)
