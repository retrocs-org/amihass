"""Check real editor capability reads through pending, failure and cancellation."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static int editor_capabilities(');b=s.index('\nstatic ',a+10)
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "amihass_core.h"
#include "nd_panel.h"
#define AMIHASS_QUERY 2
#define AMIHASS_QUERY_ONCE 1
#define AMIHASS_OK 1
#define AMIHASS_WAIT 2
#define AMIHASS_BUSY 3
struct AmiHASSClient {void *pending;};struct AmiHASSMessage{int result;unsigned int count;struct AmiHASSEntity items[16];};struct NDPanel{int x;};static struct NDPanel panel;static struct AmiHASSMessage reply;static int mode,sends,polls,closed,events;
static int amihass_send(struct AmiHASSClient *c,int kind,const char *owner,const char *entity,const char *action,int offset,int flags,const char *domain,const char *filter){assert(kind==AMIHASS_QUERY&&!action&&!offset&&flags==AMIHASS_QUERY_ONCE);(void)owner;(void)entity;(void)domain;(void)filter;++sends;if(mode==3)return 0;c->pending=&reply;return 1;}
static struct AmiHASSMessage *amihass_reply(struct AmiHASSClient *c){++polls;if(mode==4)return NULL;c->pending=NULL;memset(&reply,0,sizeof(reply));reply.result=mode==2 ? 10:polls==1 ? AMIHASS_WAIT:AMIHASS_OK;reply.count=1;strcpy(reply.items[0].id,"control.lock");strcpy(reply.items[0].state,"yes");return &reply;}
static void amihass_client_close(struct AmiHASSClient *c){c->pending=NULL;++closed;}static void FreeVec(void *p){(void)p;}static void Delay(int n){assert(n==5);}
/* Production loops wait on signals; in tests this is one scripted Delay step. */
void nd_panel_wait(struct NDPanel *p,unsigned long e,unsigned long t){(void)p;(void)e;(void)t;Delay(5);}
unsigned long amihass_client_signal(const struct AmiHASSClient *c){(void)c;return 0;}

struct NDPanel *nd_panel_open(const char *title,const struct NDPanelField *fields,unsigned int count,const char **buttons,unsigned int n){(void)title;(void)fields;assert(count==1&&n==1&&!strcmp(buttons[0],"Cancel"));return &panel;}
int nd_panel_event(struct NDPanel *p){(void)p;++events;return mode==1 ? 100:0;}void nd_panel_close(struct NDPanel *p){assert(p==&panel);}
'''+s[a:b]+r'''
int main(void){struct AHControls c;for(mode=0;mode<5;++mode){sends=polls=closed=events=0;memset(&c,0xff,sizeof(c));if(mode==0){assert(editor_capabilities("lock.sample",&c)&&c.lock&&sends==2&&polls==2);}else{assert(!editor_capabilities("lock.sample",&c)&&!c.lock);if(mode==1)assert(!sends&&!polls);if(mode==4)assert(polls==300&&sends==1);}assert(closed==1);}puts("Dashboard editor: pending capabilities, cancellation, unavailable service, failed queries and bounded timeout passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),str(root/'src/core.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
