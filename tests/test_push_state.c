#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "push_state.h"
#include "ami2ha/ws.h"
static int seen;static struct AmiHASSEntity value;static struct AHControls controls;
static struct AHNote notes[8];static int note_ops[16],note_n,note_count;
static int note(void *unused,int op,const struct AHNote *n){int i;(void)unused;note_ops[note_n++]=op;if(op==AH_NOTE_CLEAR){note_count=0;return 1;}for(i=0;i<note_count;++i)if(!strcmp(notes[i].id,n->id))break;if(op==AH_NOTE_REMOVE){if(i<note_count)notes[i]=notes[--note_count];return 1;}if(i==note_count)++note_count;notes[i]=*n;return 1;}
static int update(void *unused,const struct AmiHASSEntity *e,const struct AHControls *c){(void)unused;++seen;value=*e;controls=*c;return 1;}
int main(void){static struct AHPushState s;char ids[2][96]={"light.fixture","sensor.fixture"};a2h_buf b;ws_stream stream;ws_msg m;unsigned char frame[]={0x01,0x02,'h','i',0x89,0x01,'?',0x80,0x01,'!'};
 assert(ah_push_select(&s,(const char (*)[96])ids,0)&&!s.count&&s.next_id==1);/* notifications only */assert(!ah_push_select(&s,(const char (*)[96])ids,AH_PUSH_SELECTED+1));assert(ah_push_select(&s,(const char (*)[96])ids,2));
 assert(ah_push_event(&s,"{\"id\":1,\"type\":\"event\",\"event\":{\"a\":{\"light.fixture\":{\"s\":\"on\",\"a\":{\"friendly_name\":\"Lampa \\u00e5\\u00e4\\u00f6\",\"supported_color_modes\":[\"rgb\"],\"brightness\":128,\"rgb_color\":[255,0,0]}},\"light.unwanted\":{\"s\":\"on\",\"a\":{}}}}}",update,NULL));assert(seen==1&&controls.brightness&&controls.color&&controls.level_known&&controls.level==50&&strstr(value.name,"Lampa"));
 assert(ah_push_event(&s,"{\"id\":1,\"type\":\"event\",\"event\":{\"c\":{\"light.fixture\":{\"+\":{\"s\":\"off\"},\"-\":{\"a\":[\"rgb_color\",\"supported_color_modes\",\"brightness\"]}}}}}",update,NULL));assert(seen==2&&!controls.color&&!controls.brightness);
 assert(ah_push_event(&s,"{\"id\":1,\"type\":\"event\",\"event\":{\"r\":[\"light.fixture\"]}}",update,NULL));assert(seen==3&&!strcmp(value.state,"unavailable")&&!controls.color);
 /* Unknown or just-released subscriptions and other entities' keys are ignored. */
 seen=0;assert(ah_push_event(&s,"{\"id\":99,\"type\":\"event\",\"event\":{\"a\":{\"light.fixture\":{\"s\":\"on\",\"a\":{}}}}}",update,NULL)&&!seen);assert(ah_push_event(&s,"{\"id\":1,\"type\":\"event\",\"event\":{\"c\":{\"sensor.fixture\":{\"+\":{\"s\":\"1\"}}}}}",update,NULL)&&!seen);
 /* Subscription 2 is sensor.fixture: a change before its full state stays on REST. */
 assert(ah_push_event(&s,"{\"id\":2,\"type\":\"event\",\"event\":{\"c\":{\"sensor.fixture\":{\"+\":{\"s\":\"1\"}}}}}",update,NULL)&&!seen);
 /* Long states truncate; unused or oversized attributes are dropped, not fatal. */
 {static char big[9000];int k;strcpy(big,"{\"id\":2,\"type\":\"event\",\"event\":{\"a\":{\"sensor.fixture\":{\"s\":\"This is a fairly long text sensor state value that keeps going\",\"a\":{\"unit_of_measurement\":\"W\",\"effect_list\":[");for(k=0;k<600;++k){if(k)strcat(big,",");strcat(big,"\"Effect\"");}strcat(big,"],\"media_title\":\"");for(k=0;k<1200;++k)strcat(big,"x");strcat(big,"\"}}}}}");
  assert(ah_push_event(&s,big,update,NULL)&&seen==1&&strlen(value.state)==39&&!strcmp(value.unit,"W")&&!strstr(s.entities[1].attrs,"effect_list")&&!strstr(s.entities[1].attrs,"media_title"));}
 /* Selection changes without reconnecting: removal frees a slot, additions get new IDs. */
 {unsigned long sub=0;assert(ah_push_remove(&s,"light.fixture",&sub)&&sub==1&&s.count==1&&!strcmp(s.entities[0].id,"sensor.fixture"));assert(ah_push_add(&s,"switch.fixture")==1&&s.entities[1].sub==3);assert(ah_push_add(&s,"switch.fixture")<0&&ah_push_add(&s,"Bad id")<0);seen=0;assert(ah_push_event(&s,"{\"id\":1,\"type\":\"event\",\"event\":{\"a\":{\"light.fixture\":{\"s\":\"on\",\"a\":{}}}}}",update,NULL)&&!seen);assert(ah_push_event(&s,"{\"id\":3,\"type\":\"event\",\"event\":{\"a\":{\"switch.fixture\":{\"s\":\"on\",\"a\":{}}}}}",update,NULL)&&seen==1&&!strcmp(value.id,"switch.fixture"));}assert(!ah_push_event(&s,"bad",update,NULL));
 buf_init(&b);ws_stream_init(&stream);assert(buf_append(&b,frame,sizeof(frame)));assert(ws_stream_next(&stream,&b,&m)==WS_EV_PING&&m.len==1);assert(ws_stream_next(&stream,&b,&m)==WS_EV_MESSAGE&&m.len==3&&!memcmp(m.data,"hi!",3));ws_stream_free(&stream);buf_free(&b);buf_init(&b);assert(!buf_reserve(&b,65537)&&b.failed);buf_free(&b);
 /* An over-long message is discarded in pieces; the stream continues afterwards. */
 {static unsigned char head[10]={0x81,127,0,0,0,0,0,1,0,1};unsigned char chunk[4096],tail[]={0x81,0x02,'o','k'};unsigned long left=65537UL;ws_event ev=WS_EV_NONE;buf_init(&b);ws_stream_init(&stream);memset(chunk,'x',sizeof(chunk));assert(buf_append(&b,head,sizeof(head)));
  while(left){unsigned long n=left<sizeof(chunk) ? left:sizeof(chunk);assert(buf_append(&b,chunk,n));left-=n;ev=ws_stream_next(&stream,&b,&m);assert(ev==WS_EV_NONE||(ev==WS_EV_SKIPPED&&!left));assert(b.len<8192);}
  assert(ev==WS_EV_SKIPPED);assert(buf_append(&b,tail,sizeof(tail)));assert(ws_stream_next(&stream,&b,&m)==WS_EV_MESSAGE&&m.len==2&&!memcmp(m.data,"ok",2));ws_stream_free(&stream);buf_free(&b);}
 {/* Persistent notifications, in the shape Home Assistant 2026.2 sends them. */
  char big[700];int k;
  assert(ah_push_notifications("{\"type\":\"current\",\"notifications\":{\"a1\":{\"created_at\":\"2026-11-24T10:00:00+00:00\",\"message\":\"Battery low \\u00e5\",\"notification_id\":\"a1\",\"title\":\"Door\"},\"b2\":{\"created_at\":\"2026-11-24T11:00:00+00:00\",\"message\":\"Update ready\",\"notification_id\":\"b2\",\"title\":null}}}",note,NULL));
  assert(note_n==3&&note_ops[0]==AH_NOTE_CLEAR&&note_count==2&&!strcmp(notes[0].title,"Door")&&!strcmp(notes[0].message,"Battery low \345")&&!notes[1].title[0]&&!strcmp(notes[1].created,"2026-11-24T11:00:00+00:00"));
  assert(ah_push_notifications("{\"type\":\"removed\",\"notifications\":{\"a1\":{\"created_at\":\"2026-11-24T10:00:00+00:00\",\"message\":\"x\",\"notification_id\":\"a1\",\"title\":\"Door\"}}}",note,NULL)&&note_count==1&&!strcmp(notes[0].id,"b2"));
  strcpy(big,"{\"type\":\"added\",\"notifications\":{\"c3\":{\"message\":\"");for(k=0;k<600;++k)strcat(big,"m");strcat(big,"\",\"notification_id\":\"c3\",\"title\":\"Long\"}}}");
  assert(ah_push_notifications(big,note,NULL)&&note_count==2&&strlen(notes[1].message)==255);
  assert(ah_push_notifications("{\"type\":\"future\",\"notifications\":{}}",note,NULL)&&!ah_push_notifications("{\"type\":\"added\",\"notifications\":[1]}",note,NULL)&&!ah_push_notifications("{\"type\":\"added\"}",note,NULL));
 }
 puts("Push: persistent notifications, per-entity subscriptions, selection changes, typed full/delta/removal updates, attribute filtering, long states, unknown IDs, malformed input, fragmentation, ping, 64 KiB bounds and oversized-message skipping passed.");return 0;}
