"""Drive production room selection through paging, errors and cancellation."""
from pathlib import Path
import os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'src/browser.c').read_text();code=s[s.index('static void set_room('):s.index('static int show_details(')]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amihass.h"
#include "nd_panel.h"
#define MEMF_PUBLIC 0
#define MEMF_CLEAR 0
#define GTCY_Labels 1
#define GTCY_Active 2
#define TAG_DONE 0
struct AHBrowserLive {char area[96];};
struct Browser {void *window,*area;struct AHBrowserLive live;char room_id[96],room_name[64];const char *room_labels[4];};
struct NDPanel {unsigned int choice;const char **labels;int enabled[105];};
static struct NDPanel ui;static int mode,stage,allocs,requests,force_wait;
void *AllocVec(size_t n,int flags){void *p=calloc(1,n);if(p)++allocs;return p;}
void FreeVec(void *p){if(p){--allocs;free(p);}}
void Delay(int n){(void)n;}
void GT_SetGadgetAttrs(void *g,void *w,void *r,...){(void)g;(void)w;(void)r;}
void ah_browser_live_close(struct AHBrowserLive *p){(void)p;}
const char *amihass_problem(int result){return "Connection failed";}
struct NDPanel *nd_panel_open(const char *t,const struct NDPanelField *f,unsigned int n,const char **b,unsigned int c){memset(&ui,0,sizeof(ui));return &ui;}
void nd_panel_choices(struct NDPanel *p,unsigned int f,const char **labels,unsigned int active){assert(f==0);p->labels=labels;p->choice=active;}
unsigned int nd_panel_choice(struct NDPanel *p,unsigned int f){return p->choice;}
void nd_panel_set(struct NDPanel *p,unsigned int f,const char *s){(void)p;(void)f;(void)s;}
void nd_panel_enable(struct NDPanel *p,unsigned int id,int enabled){assert(id<105);p->enabled[id]=enabled;}
void nd_panel_close(struct NDPanel *p){assert(p==&ui);}
void amihass_client_close(struct AmiHASSClient *c){if(c->pending)FreeVec(c->pending);memset(c,0,sizeof(*c));}
int amihass_send(struct AmiHASSClient *c,int kind,const char *owner,const char *entity,const char *action,int offset,int refresh,const char *domain,const char *filter){
 struct AmiHASSMessage *m;int i;assert(kind==AMIHASS_AREAS&&!entity&&!action&&!domain&&!filter&&!c->pending);++requests;
 if(mode==3)return 0;
 m=AllocVec(sizeof(*m),0);m->kind=kind;m->result=AMIHASS_OK;m->offset=offset;
 if(mode==4){m->total=m->count=1;strcpy(m->items[0].id,"study");strcpy(m->items[0].name,"Study");}
 else if(mode==5 && requests==1)m->result=AMIHASS_FAILED;
 else {m->total=40;m->count=40-offset;if(m->count>16)m->count=16;for(i=0;i<m->count;++i){sprintf(m->items[i].id,"room_%d",offset+i);strcpy(m->items[i].name,"Same display name");}}
 c->pending=m;return 1;
}
struct AmiHASSMessage *amihass_reply(struct AmiHASSClient *c){struct AmiHASSMessage *m=c->pending;if(force_wait)return NULL;c->pending=NULL;return m;}
int nd_panel_event(struct NDPanel *p){
 if(mode==1)return 103; /* All rooms remains available during a pending reply. */
 if(mode==2)return -1;
 if(mode==3)return 103; /* All rooms remains available without a running service. */
 if(mode==5 && stage==0 && p->enabled[102]){++stage;return 102;} /* retry failed request */
 if(!p->labels[1])return 0;
 if(mode==0){if(stage==0){assert(p->enabled[101]&&!p->enabled[100]);++stage;return 101;}if(stage==1){assert(p->enabled[101]&&p->enabled[100]);++stage;return 101;}if(stage==2){assert(!p->enabled[101]&&p->enabled[100]);++stage;return 100;}}
 p->choice=3;assert(!strcmp(p->labels[3],"Same display name"));return 103;
}
'''
main=r'''
int main(void){struct Browser b;int ok;
 for(mode=0;mode<6;++mode){memset(&b,0,sizeof(b));strcpy(b.room_id,"existing");strcpy(b.room_name,"Existing room");stage=requests=0;force_wait=mode==1||mode==2;ok=choose_room(&b,mode==4);assert(!allocs);
  if(mode==0){assert(ok&&requests==4&&!strcmp(b.room_id,"room_18")&&!strcmp(b.live.area,"room_18"));}
  if(mode==1||mode==3)assert(ok&&!b.room_id[0]);
  if(mode==2)assert(!ok&&!strcmp(b.room_id,"existing"));
  if(mode==4)assert(ok&&!strcmp(b.room_id,"study")&&!strcmp(b.room_name,"Study"));
  if(mode==5)assert(ok&&requests==2&&!strcmp(b.room_id,"room_2"));
 }
 puts("Room picker: paged discovery, previous/next, stable IDs with duplicate names, offline All rooms, pending cancellation and retry passed.");return 0;
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'exec').mkdir();(p/'exec/ports.h').write_text('typedef unsigned long ULONG;typedef long LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n')
 (p/'test.c').write_text(prefix+code+main)
 command=[os.environ.get('CC','cc'),'-std=c99','-Wno-pointer-to-int-cast','-I'+str(p),'-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(p/'test.c'),'-o',str(p/'test')]
 if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
 subprocess.run(command,check=True);subprocess.run([str(p/'test')],check=True,timeout=10)
