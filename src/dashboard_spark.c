/* History-graph tiles; see dashboard_spark.h. */
#include <stdio.h>
#include <string.h>
#ifndef AH_HOST_TEST
#include <proto/exec.h>
#else
void FreeVec(void *);
#endif
#include "dashboard_spark.h"
void ah_sparks_init(struct AHSparks *s){memset(s,0,sizeof(*s));s->tile=-1;}
void ah_sparks_close(struct AHSparks *s){amihass_client_close(&s->client);s->tile=-1;s->again=0;}
void ah_sparks_reset(struct AHSparks *s){ah_sparks_close(s);memset(s->entity,0,sizeof(s->entity));memset(s->at,0,sizeof(s->at));memset(s->count,0,sizeof(s->count));}
unsigned long ah_sparks_signal(const struct AHSparks *s){return amihass_client_signal(&s->client);}
int ah_sparks_waiting(const struct AHSparks *s){return s->tile>=0;}
static int ask(struct AHSparks *s,int tile,int offset)
{s->offset=offset;return amihass_history(&s->client,"AmiHASS.Dashboard",s->entity[tile],s->hours[tile],offset);}
int ah_sparks_step(struct AHSparks *s,const struct AHDashboardLayout *l,time_t now)
{
 struct AmiHASSMessage *m;unsigned int i;int changed=0,k;long v;
 /* A tile that no longer shows a graph, or shows another entity, drops its points. */
 for(i=0;i<AH_DASH_TILES;++i){const struct AHDashboardTile *t=&l->tiles[i];int wanted=AH_SPARK_IS(t->display)&&t->entity[0];
  if(!wanted&&(s->count[i]||s->entity[i][0])){s->count[i]=0;s->entity[i][0]=0;++s->serial[i];changed=1;}
  else if(wanted&&(strcmp(s->entity[i],t->entity)||s->hours[i]!=AH_SPARK_HOURS(t->display))){strcpy(s->entity[i],t->entity);s->hours[i]=AH_SPARK_HOURS(t->display);s->count[i]=0;s->at[i]=0;++s->serial[i];changed=1;if(s->tile==(int)i)ah_sparks_close(s);}}
 if(s->tile>=0){
  if(s->again){if(now==s->asked)return changed;s->again=0;if(!ask(s,s->tile,s->offset)){s->at[s->tile]=now;s->tile=-1;}return changed;}
  m=amihass_reply(&s->client);
  if(!m){if(!s->client.pending){s->at[s->tile]=now;s->tile=-1;}return changed;}
  if(m->result==AMIHASS_WAIT&&++s->tries<45){s->again=1;s->asked=now;}
  else if(m->result==AMIHASS_OK){for(k=0;k<m->count&&s->fill<AH_SPARK_POINTS;++k)s->fresh[s->fill++]=amihass_history_value(m->items[k].state,&v)?v:AH_SPARK_GAP;
   if(m->count>0&&m->offset+m->count<m->total&&m->offset+m->count<AH_SPARK_POINTS){if(!ask(s,s->tile,(int)(m->offset+m->count))){s->at[s->tile]=now;s->tile=-1;}}
   else{memcpy(s->value[s->tile],s->fresh,sizeof(s->fresh[0])*(unsigned int)s->fill);s->count[s->tile]=s->fill;s->at[s->tile]=now;++s->serial[s->tile];changed=1;s->tile=-1;}}
  else{/* failure or slow server: keep any old graph, try again next round */s->at[s->tile]=now;s->tile=-1;}
  FreeVec(m);return changed;}
 /* Next stale graph tile, one at a time. */
 for(i=0;i<AH_DASH_TILES;++i)if(s->entity[i][0]&&AH_SPARK_IS(l->tiles[i].display)&&(!s->at[i]||now<s->at[i]||now-s->at[i]>=AH_SPARK_SECONDS)){
  s->fill=0;s->tries=0;s->again=0;if(ask(s,(int)i,0))s->tile=(int)i;else s->at[i]=now;break;}
 return changed;
}
int ah_sparks_range(const struct AHSparks *s,unsigned int tile,long *low,long *high)
{int i,found=0;if(tile>=AH_DASH_TILES)return 0;for(i=0;i<s->count[tile];++i){long v=s->value[tile][i];if(v==AH_SPARK_GAP)continue;if(!found||v<*low)*low=v;if(!found||v>*high)*high=v;found=1;}return found;}
void ah_sparks_text(long v,char *out)
{long a=v<0?-v:v;if(a>=10000L)sprintf(out,"%s%ld",v<0?"-":"",(a+50)/100);else sprintf(out,"%s%ld.%ld",v<0?"-":"",(a+5)/100,((a+5)/10)%10);}
int ah_sparks_shape(const struct AHSparks *s,unsigned int tile,int height,int *ys,int cap)
{int i,n;long lo,hi,span;if(tile>=AH_DASH_TILES||height<2||height>120)return 0;n=s->count[tile];if(n>cap)n=cap;if(n<2||!ah_sparks_range(s,tile,&lo,&hi))return 0;span=hi-lo;
 /* span < 2e8; scale it below 2^24 so the product stays inside 32 bits */
 for(i=0;i<n;++i){long d,w=span;if(s->value[tile][i]==AH_SPARK_GAP){ys[i]=-1;continue;}d=hi-s->value[tile][i];while(w>16777216L){w>>=1;d>>=1;}ys[i]=w?(int)(d*(long)(height-1)/w):(height-1)/2;}return n;}
