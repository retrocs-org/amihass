/* Included in the central broker: full attributes (ATTRIBUTES) and recent
 * history (HISTORY) of one entity at a time. Each kind keeps one cached
 * answer in a buffer allocated on first use and freed when idle, so clients
 * that never ask for either pay nothing. Requests poll like SNAPSHOT: WAIT
 * until the fetch completes, then OK with a page of items. */
static void start_failed(struct Backend*,unsigned long);
static void connection_ok(struct Backend*,unsigned long);
#define AH_DETAIL_ATTRS 0
#define AH_DETAIL_HISTORY 1
/* History: stage 1 asks the server for the window boundaries, stage 0 reads
 * the window in `chunks` pieces (more pieces when a piece is too large). */
struct AHDetail {char entity[96],start[24],bounds[13][24];int pending,result,stage,hours,count,total,chunks,chunk;unsigned long requested,updated,touched;struct AmiHASSEntity items[AH_DETAIL_ITEMS];};
static void detail_free(struct Backend *b,int kind)
{if(b->detail[kind]){FreeVec(b->detail[kind]);b->detail[kind]=NULL;}}
/* Idle cache is dropped after a minute (five seconds when memory is low). */
static void detail_reclaim(struct Backend *b,unsigned long now)
{int k;for(k=0;k<2;++k){struct AHDetail *d=b->detail[k];if(d&&!d->pending&&!(b->job&&b->phase==7&&b->index==k)&&(now<d->touched||now-d->touched>(b->low_memory?5UL:60UL)))detail_free(b,k);}}
static void detail_sample(struct AHDetail *d,int kind,unsigned long now)
{int i;static const char *keys[][3]={{"friendly_name","AmiHASS sample","string"},{"unit_of_measurement","\xb0""C","string"},{"device_class","temperature","string"},{"state_class","measurement","string"}};
 d->count=d->total=0;
 if(kind==AH_DETAIL_ATTRS){for(i=0;i<4;++i){memset(&d->items[i],0,sizeof(d->items[i]));strcpy(d->items[i].id,keys[i][0]);strcpy(d->items[i].name,keys[i][1]);strcpy(d->items[i].device_class,keys[i][2]);}d->count=d->total=4;}
 else{static const int wave[16]={0,38,71,92,100,92,71,38,0,-38,-71,-92,-100,-92,-71,-38};for(i=0;i<48;++i){long v=2150+wave[(i+(int)(now/300UL))%16]*3/2;memset(&d->items[i],0,sizeof(d->items[i]));sprintf(d->items[i].state,"%ld.%02ld",v/100,v%100);}d->count=d->total=48;}
 d->pending=0;d->result=AMIHASS_OK;d->updated=now;}
static void detail_page(struct AHDetail *d,struct AmiHASSMessage *m,unsigned long now)
{int i;m->result=d->result;m->age=d->updated&&now>=d->updated?now-d->updated:0;m->total=d->total;m->count=0;d->touched=now;
 if(d->result!=AMIHASS_OK)return;if(m->offset>d->count){m->result=AMIHASS_UNSUPPORTED;return;}
 for(i=0;i<AMIHASS_PAGE&&m->offset+i<d->count;++i)m->items[i]=d->items[m->offset+i];m->count=i;}
static void detail_request(struct Backend *b,struct AmiHASSMessage *m,unsigned long now)
{
 int kind=m->kind==AMIHASS_HISTORY?AH_DETAIL_HISTORY:AH_DETAIL_ATTRS,hours=kind?(m->refresh?(int)m->refresh:24):0,fresh=!kind&&m->refresh==1;struct AHDetail *d;unsigned long keep=kind?60UL:10UL;
 if(!m->owner[0]||!amihass_entity_id(m->entity)||m->offset<0||m->offset>AH_DETAIL_ITEMS||(kind&&(hours<1||hours>48))||(!kind&&(m->refresh<0||m->refresh>1))){m->result=AMIHASS_UNSUPPORTED;return;}
 if((b->sample_mode&&!strstr(m->entity,".amihass_sample_"))||(!b->sample_mode&&strstr(m->entity,".amihass_sample_"))){m->result=AMIHASS_UNCONFIGURED;return;}
 if(!b->sample_mode&&(!b->configured||b->auth_blocked||!b->network)){m->result=b->configured?AMIHASS_FAILED:AMIHASS_UNCONFIGURED;return;}
 d=b->detail[kind];
 if(!d){d=AllocVec(sizeof(*d),MEMF_PUBLIC|MEMF_CLEAR);if(!d){m->result=AMIHASS_BUSY;return;}b->detail[kind]=d;}
 if(d->pending){if(strcmp(d->entity,m->entity)||d->hours!=hours){m->result=AMIHASS_BUSY;return;}m->result=AMIHASS_WAIT;d->touched=now;return;}
 /* A held answer serves page requests and repeats; a different entity,
  * window or an expired answer starts a new fetch. */
 if(strcmp(d->entity,m->entity)||d->hours!=hours||(!m->offset&&(fresh||now<d->updated||now-d->updated>=keep))||d->result==AMIHASS_WAIT){
  if(m->offset&&!strcmp(d->entity,m->entity)&&d->hours==hours&&d->result==AMIHASS_OK){detail_page(d,m,now);return;}
  strcpy(d->entity,m->entity);d->hours=hours;d->count=d->total=0;d->start[0]=0;d->bounds[0][0]=0;d->chunks=1;d->chunk=0;d->stage=kind&&hours!=24;d->requested=d->touched=now;d->result=AMIHASS_WAIT;d->pending=1;
  if(b->sample_mode)detail_sample(d,kind,now);
 }
 detail_page(d,m,now);
}
/* Called by schedule() when no state read or action is due. */
static int detail_start(struct Backend *b,const char *program,unsigned long now)
{
 int k;char path[240],body[320];
 for(k=0;k<2;++k){struct AHDetail *d=b->detail[k];int ok;if(!d||!d->pending)continue;
  if(k==AH_DETAIL_ATTRS){sprintf(path,"/api/states/%s",d->entity);ok=job_start(b,program,path,NULL,7,k);}
  else if(d->stage==1)ok=amihass_history_bounds_request(d->hours,d->chunks,body,sizeof(body))&&job_start(b,program,"/api/template",body,7,k);
  else ok=amihass_history_range_path(d->entity,d->bounds[d->chunks>1?d->chunk:0],d->chunks>1?d->bounds[d->chunk+1]:NULL,path,sizeof(path))&&job_start(b,program,path,NULL,7,k);
  if(!ok){d->pending=0;d->result=AMIHASS_FAILED;if(b->start_local)start_failed(b,now);}
  (void)now;return 1;}
 return 0;
}
static void detail_complete(struct Backend *b,int result,unsigned long now)
{
 struct AHDetail *d=b->index>=0&&b->index<2?b->detail[b->index]:NULL;char path[200],*data=NULL;FILE *f;size_t n=0;int count=-1;
 if(!d||!d->pending)return;
 if(result==1&&b->last_http==200){sprintf(path,"%s/response",b->directory);f=fopen(path,"rb");if(f){data=AllocVec(AH_HISTORY_LIMIT+1,MEMF_PUBLIC|MEMF_CLEAR);if(data){n=fread(data,1,AH_HISTORY_LIMIT,f);if(ferror(f)||fgetc(f)!=EOF||memchr(data,0,n)){FreeVec(data);data=NULL;}}fclose(f);}}
 if(data){b->diagnostics.body_bytes+=(unsigned long)n;
  if(b->index==AH_DETAIL_ATTRS)count=amihass_attributes_page(data,d->items,AH_DETAIL_ITEMS,&d->total);
  else if(d->stage==1){if(amihass_history_bounds(data,d->bounds,d->chunks+1)){d->stage=0;d->chunk=0;d->count=0;FreeVec(data);connection_ok(b,now);return;/* still pending: the history reads follow */}}
  else{int got=amihass_history_points(data,(unsigned long)n,d->items+d->count,(unsigned int)(AH_DETAIL_ITEMS/d->chunks));
   if(got>=0&&d->chunk+1<d->chunks){d->count+=got;++d->chunk;FreeVec(data);connection_ok(b,now);return;/* next piece */}
   count=got>=0?d->count+got:-1;d->total=count;}
  FreeVec(data);}
 /* Too large for one response (a busy sensor): read the window in 4, then 12 pieces. */
 else if(b->index==AH_DETAIL_HISTORY&&b->last_curl==63&&d->chunks<12){d->chunks=d->chunks==1?4:12;d->stage=1;d->chunk=0;d->count=0;return;}
 d->pending=0;if(b->index==AH_DETAIL_HISTORY&&count>=0)d->count=count;d->count=count>0?count:0;if(d->total<d->count)d->total=d->count;d->result=count>=0?AMIHASS_OK:b->last_http==404?AMIHASS_UNAVAILABLE:AMIHASS_FAILED;d->updated=now;
 if(count>=0)connection_ok(b,now);/* Detail errors do not disconnect other entities. */
}
