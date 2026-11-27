static unsigned int camera_watchers(struct Backend *b,unsigned long now)
{unsigned int i,n=0;for(i=0;i<16;++i){if(b->camera_clients[i].port&&(now<b->camera_clients[i].seen||now-b->camera_clients[i].seen>120))b->camera_clients[i].port=NULL;if(b->camera_clients[i].port)++n;}return n;}
static void camera_release(struct Backend *b,struct MsgPort *port,unsigned long now)
{unsigned int i;for(i=0;i<16;++i)if(b->camera_clients[i].port==port)b->camera_clients[i].port=NULL;if(!camera_watchers(b,now)){if(b->camera.pending){++b->camera_cancels;if(b->job&&b->phase==6)b->camera.discard=1;else{ah_camera_clear(&b->camera);}}if(b->low_memory&&!b->camera.pending)ah_camera_clear(&b->camera);}}
static int camera_watch(struct Backend *b,struct MsgPort *port,unsigned long now)
{unsigned int i,free_slot=16;if(!port)return 1;camera_watchers(b,now);for(i=0;i<16;++i){if(b->camera_clients[i].port==port){b->camera_clients[i].seen=now;return 1;}if(!b->camera_clients[i].port)free_slot=i;}if(free_slot==16)return 0;b->camera_clients[free_slot].port=port;b->camera_clients[free_slot].seen=now;return 1;}
/* Included in the central broker; one shared bounded image, no filesystem paths in IPC. */
static void camera_request(struct Backend *b,struct AmiHASSMessage *m,unsigned long now)
{struct AHCamera *c=&b->camera;int fresh=m->refresh!=0,small=!strcmp(m->domain,"thumb");char path[160];if(!m->owner[0]||!ah_camera_path(m->entity,path,sizeof(path))||m->offset<0||m->offset>(long)AH_CAMERA_LIMIT||m->refresh<0||m->refresh>1||(m->refresh&&(m->offset||m->action[0]))){m->result=AMIHASS_UNSUPPORTED;return;}
 if((b->sample_mode&&strcmp(m->entity,"camera.amihass_sample_driveway"))||(!b->sample_mode&&strstr(m->entity,".amihass_sample_"))){m->result=AMIHASS_UNCONFIGURED;return;}
 if(!b->sample_mode&&(!b->configured||b->auth_blocked||!b->network)){m->result=b->configured?AMIHASS_FAILED:AMIHASS_UNCONFIGURED;return;}
 if(c->pending&&strcmp(c->entity,m->entity)){m->result=AMIHASS_BUSY;return;}
 if(c->pending&&c->discard){m->result=AMIHASS_BUSY;return;}if(!camera_watch(b,m->msg.mn_ReplyPort,now)){m->result=AMIHASS_BUSY;return;}
 if(!strcmp(c->entity,m->entity)&&c->requested&&now>=c->requested&&now-c->requested<5)fresh=0;
 if(!b->sample_mode&&!c->pending&&!c->data&&!strcmp(b->camera_last_entity,m->entity)&&b->camera_last_request&&now>=b->camera_last_request&&now-b->camera_last_request<5){m->result=AMIHASS_BUSY;return;}
 /* A tile thumbnail can use a held full-size image; the viewer never gets a thumbnail. */
 if((strcmp(c->entity,m->entity)||fresh||(c->small&&!small))&&!m->offset&&!m->action[0]&&!c->pending){unsigned long serial=++b->camera_serial;ah_camera_clear(c);c->small=small;c->serial=serial;c->requested=c->touched=now;strcpy(c->entity,m->entity);sprintf(c->version,"%08lx-%08lx",b->generation,serial);c->pending=1;c->result=AMIHASS_WAIT;if(b->sample_mode){c->pending=0;c->result=ah_camera_sample(c)?AMIHASS_OK:AMIHASS_FAILED;c->updated=now;}}
 ah_camera_reply(c,m,now);}
static void camera_complete(struct Backend *b,int result,unsigned long now)
{struct AHCamera *c=&b->camera;char path[200],format[8];FILE *f;unsigned char *data=NULL;size_t n=0;unsigned int width,height;int ok=0;c->pending=0;c->result=AMIHASS_FAILED;if(c->discard){ah_camera_clear(c);return;}
 if(result==1&&b->last_http==200){sprintf(path,"%s/response",b->directory);f=fopen(path,"rb");if(f){data=malloc(AH_CAMERA_LIMIT);if(data){n=fread(data,1,AH_CAMERA_LIMIT,f);ok=!ferror(f)&&fgetc(f)==EOF&&ah_camera_probe(data,(unsigned long)n,&width,&height,format);if(!ok)c->result=AMIHASS_UNSUPPORTED;}fclose(f);}}
 if(ok){if(c->data){memset(c->data,0,c->size);free(c->data);}c->data=data;c->size=(unsigned long)n;c->width=width;c->height=height;strcpy(c->format,format);c->updated=c->touched=now;c->result=AMIHASS_OK;b->camera_bytes+=(unsigned long)n;b->diagnostics.body_bytes+=(unsigned long)n;connection_ok(b,now);}
 else {++b->camera_failed;if(data){memset(data,0,n);free(data);}}/* Camera errors do not disconnect other entities. */}
