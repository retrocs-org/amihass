"""Attributes/history broker: WAIT then OK, paging, two-step history window, idle release."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1];source=(root/'tests/test_backend.py').read_text();scope={'__file__':str(root/'tests/test_backend.py')}
exec(compile(source[:source.index('with tempfile.TemporaryDirectory()')],str(root/'tests/test_backend.py'),'exec'),scope)
main=r'''
static void ask(struct Backend *b,struct AmiHASSMessage *m,int kind,const char *entity,int offset,int refresh){memset(m,0,sizeof(*m));m->magic=AMIHASS_MAGIC;m->abi=AMIHASS_ABI;m->msg.mn_Length=sizeof(*m);m->msg.mn_ReplyPort=(struct MsgPort*)3;m->kind=kind;strcpy(m->owner,"Detail.QA");strcpy(m->entity,entity);m->offset=offset;m->refresh=refresh;handle(b,m);}
static void respond(const char *text){FILE *f=fopen("response","wb");assert(f&&fputs(text,f)>=0);fclose(f);}
int main(void){struct Backend *b=calloc(1,sizeof(*b));struct AmiHASSMessage m;char json[4000];int i;
 sample_setting=1;reload(b);ask(b,&m,AMIHASS_HISTORY,"sensor.amihass_sample_temperature",0,0);assert(m.result==AMIHASS_OK&&m.total==48&&m.count==16);ask(b,&m,AMIHASS_HISTORY,"sensor.amihass_sample_temperature",32,0);assert(m.result==AMIHASS_OK&&m.count==16);
 ask(b,&m,AMIHASS_ATTRIBUTES,"sensor.amihass_sample_temperature",0,0);assert(m.result==AMIHASS_OK&&m.count==4&&!strcmp(m.items[0].id,"friendly_name"));ask(b,&m,AMIHASS_ATTRIBUTES,"sensor.real",0,0);assert(m.result==AMIHASS_UNCONFIGURED);
 sample_setting=0;reload(b);b->network=b->network_known=1;b->next_network=now+60;connection_ok(b,now);
 ask(b,&m,AMIHASS_ATTRIBUTES,"sensor.hall",0,0);assert(m.result==AMIHASS_WAIT&&b->detail[0]&&b->detail[0]->pending);ask(b,&m,AMIHASS_ATTRIBUTES,"sensor.other",0,0);assert(m.result==AMIHASS_BUSY);
 ask(b,&m,AMIHASS_ATTRIBUTES,"bad id",0,0);assert(m.result==AMIHASS_UNSUPPORTED);ask(b,&m,AMIHASS_HISTORY,"sensor.hall",0,49);assert(m.result==AMIHASS_UNSUPPORTED);
 sent_phase=0;schedule(b,"fixture");assert(sent_phase==7&&!strcmp(sent_path,"/api/states/sensor.hall"));
 strcpy(json,"{\"entity_id\":\"sensor.hall\",\"state\":\"1\",\"attributes\":{");for(i=0;i<20;++i)sprintf(json+strlen(json),"%s\"k%02d\":%d",i?",":"",i,i);strcat(json,"}}");respond(json);strcpy(b->directory,".");b->last_http=200;complete(b,1);b->job=NULL;
 ask(b,&m,AMIHASS_ATTRIBUTES,"sensor.hall",0,0);assert(m.result==AMIHASS_OK&&m.count==16&&m.total==20&&!strcmp(m.items[3].id,"k03")&&!strcmp(m.items[3].name,"3")&&!strcmp(m.items[3].device_class,"number"));
 ask(b,&m,AMIHASS_ATTRIBUTES,"sensor.hall",16,0);assert(m.result==AMIHASS_OK&&m.count==4&&!strcmp(m.items[0].id,"k16"));ask(b,&m,AMIHASS_ATTRIBUTES,"sensor.hall",30,0);assert(m.result==AMIHASS_UNSUPPORTED);
 /* 24 hours: one request on the server's default window. */
 ask(b,&m,AMIHASS_HISTORY,"sensor.hall",0,0);assert(m.result==AMIHASS_WAIT);schedule(b,"fixture");assert(sent_phase==7&&!strcmp(sent_path,"/api/history/period?filter_entity_id=sensor.hall&minimal_response&no_attributes"));
 respond("[[{\"entity_id\":\"sensor.hall\",\"state\":\"20.5\",\"attributes\":{},\"last_changed\":\"2026-11-24T00:00:00+00:00\"},{\"state\":\"unavailable\",\"last_changed\":\"x\"},{\"state\":\"21\",\"last_changed\":\"2026-11-24T01:00:00+00:00\"}]]");b->last_http=200;complete(b,1);b->job=NULL;
 ask(b,&m,AMIHASS_HISTORY,"sensor.hall",0,0);assert(m.result==AMIHASS_OK&&m.count==3&&!m.items[1].state[0]&&!strcmp(m.items[2].state,"21")&&!strcmp(m.items[2].updated,"2026-11-24T01:00:00+00:00"));
 /* Other windows: the server names the start first, then the history read. */
 ask(b,&m,AMIHASS_HISTORY,"sensor.hall",0,6);assert(m.result==AMIHASS_WAIT);schedule(b,"fixture");assert(sent_phase==7&&!strcmp(sent_path,"/api/template")&&strstr(sent_entity,"hours=6"));respond("2026-11-24T02:00:00Z 2026-11-24T08:00:00Z");b->last_http=200;complete(b,1);b->job=NULL;
 assert(b->detail[1]->pending);ask(b,&m,AMIHASS_HISTORY,"sensor.hall",0,6);assert(m.result==AMIHASS_WAIT);schedule(b,"fixture");assert(!strcmp(sent_path,"/api/history/period/2026-11-24T02:00:00Z?filter_entity_id=sensor.hall&minimal_response&no_attributes"));respond("[]");complete(b,1);b->job=NULL;ask(b,&m,AMIHASS_HISTORY,"sensor.hall",0,6);assert(m.result==AMIHASS_OK&&m.count==0);
 /* A window too large for one response is read in four pieces. */
 now+=61;ask(b,&m,AMIHASS_HISTORY,"sensor.busy",0,0);assert(m.result==AMIHASS_WAIT);schedule(b,"fixture");assert(strstr(sent_path,"filter_entity_id=sensor.busy")&&!strstr(sent_path,"end_time"));b->last_http=0;b->last_curl=63;complete(b,-1);b->job=NULL;assert(b->detail[1]->pending&&b->detail[1]->chunks==4&&b->detail[1]->stage==1);
 schedule(b,"fixture");assert(!strcmp(sent_path,"/api/template")&&strstr(sent_entity,"range(5)"));respond("2026-11-24T00:00:00Z 2026-11-24T06:00:00Z 2026-11-24T12:00:00Z 2026-11-24T18:00:00Z 2026-11-25T00:00:00Z");b->last_http=200;b->last_curl=0;complete(b,1);b->job=NULL;
 {int piece;for(piece=0;piece<4;++piece){char want[200],body[2000];int k;schedule(b,"fixture");sprintf(want,"/api/history/period/2026-11-24T%02d:00:00Z?filter_entity_id=sensor.busy&end_time=",piece*6);assert(!strncmp(sent_path,want,strlen(want)));
   strcpy(body,"[[");for(k=0;k<30;++k)sprintf(body+strlen(body),"%s{\"state\":\"%d\",\"last_changed\":\"t\"}",k?",":"",piece*100+k);strcat(body,"]]");respond(body);b->last_http=200;complete(b,1);b->job=NULL;}}
 ask(b,&m,AMIHASS_HISTORY,"sensor.busy",0,0);assert(m.result==AMIHASS_OK&&m.total==64&&!strcmp(m.items[0].state,"0"));ask(b,&m,AMIHASS_HISTORY,"sensor.busy",48,0);assert(m.count==16&&!strcmp(m.items[15].state,"329"));
 /* Failures stay local; a missing entity is UNAVAILABLE; idle caches are freed. */
 now+=61;ask(b,&m,AMIHASS_HISTORY,"sensor.gone",0,0);schedule(b,"fixture");b->last_http=404;complete(b,-1);b->job=NULL;ask(b,&m,AMIHASS_HISTORY,"sensor.gone",0,0);assert(m.result==AMIHASS_UNAVAILABLE&&b->connected);
 now+=61;schedule(b,"fixture");assert(!b->detail[0]&&!b->detail[1]);
 b->job=NULL;b->next_network=now+60;b->network=1;connection_ok(b,now);ask(b,&m,AMIHASS_ATTRIBUTES,"sensor.hall",0,0);sent_phase=0;schedule(b,"fixture");b->last_http=401;complete(b,-1);b->job=NULL;assert(sent_phase==7&&b->auth_blocked&&!b->detail[0]->pending&&b->detail[0]->result==AMIHASS_FAILED);
 detail_free(b,0);detail_free(b,1);if(b->subscriptions)FreeVec(b->subscriptions);free(b);puts("Detail broker: sample data, attribute paging, default and server-timed history windows, local failures and idle release passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'test.c').write_text(scope['preamble']+scope['structs']+scope['stubs']+scope['functions']+main)
    cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c')]+[str(root/'src'/name) for name in ('core.c','camera.c','sample.c','browser_data.c')]+['-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],cwd=p,check=True)
