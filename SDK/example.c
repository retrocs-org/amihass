/* Independent SDK client: never reads the bridge configuration or token. */
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "amihass.h"
const char example_version[]="$VER: AmiHASSExample 0.1";
static struct AmiHASSMessage *exchange(struct AmiHASSClient *c,int kind,const char *entity,const char *action)
{struct AmiHASSMessage *m;int i;if(!amihass_send(c,kind,"SDK.Example",entity,action,0,kind==AMIHASS_QUERY ? AMIHASS_QUERY_ONCE:0,NULL,NULL))return NULL;for(i=0;i<150;++i){m=amihass_reply(c);if(m)return m;if(!c->pending)break;Delay(5);}amihass_client_close(c);return NULL;}
static int reading(struct AmiHASSClient *c,const char *entity)
{struct AmiHASSMessage *m;int i,result;for(i=0;i<100;++i){m=exchange(c,AMIHASS_QUERY,entity,NULL);if(!m)return 20;result=m->result;if(result==AMIHASS_OK||result==AMIHASS_UNAVAILABLE){printf("%s = %s %s\n",m->item.id,m->item.state,m->item.unit);FreeVec(m);return result==AMIHASS_OK ? 0:5;}FreeVec(m);if(result!=AMIHASS_WAIT){printf("Query: %s\n",amihass_problem(result));return 10;}Delay(5);}puts("Query timed out.");return 10;}
int main(int argc,char **argv)
{struct AmiHASSClient client;struct AmiHASSMessage *m;int i,result=0,kind=AMIHASS_STATUS;const char *entity=NULL,*action=NULL;memset(&client,0,sizeof(client));
 if(argc==2&&!strcmp(argv[1],"LIST"))kind=AMIHASS_LIST;
 else if(argc==3&&!strcmp(argv[1],"QUERY")){kind=AMIHASS_QUERY;entity=argv[2];}
 else if(argc==4&&!strcmp(argv[1],"ACTION")){kind=AMIHASS_ACTION;entity=argv[2];action=argv[3];}
 else if(argc!=1&&!(argc==2&&!strcmp(argv[1],"STATUS"))){puts("AmiHASSExample [STATUS|LIST|QUERY entity|ACTION entity \"Toggle\"]");return 20;}
 if(kind==AMIHASS_QUERY){result=reading(&client,entity);goto done;}
 if(kind==AMIHASS_ACTION){result=reading(&client,entity);if(result)goto done;m=exchange(&client,kind,entity,action);if(!m){result=20;goto done;}i=m->result;FreeVec(m);if(i!=AMIHASS_WAIT){printf("Action: %s\n",amihass_problem(i));result=10;goto done;}puts("Action accepted; reading actual bridge state.");result=reading(&client,entity);goto done;}
 for(i=0;i<100;++i){m=exchange(&client,kind,NULL,NULL);if(!m){puts("Start AmiHASSService using Preferences first.");result=20;goto done;}if(m->result!=AMIHASS_WAIT)break;FreeVec(m);Delay(5);}if(i==100){result=10;goto done;}
 if(m->result!=AMIHASS_OK){puts(amihass_problem(m->result));result=10;}
 else if(kind==AMIHASS_STATUS)printf("Bridge source: %s; state: %s; ready: %ld\n",m->status.server,m->item.state,(long)m->status.connected);
 else{for(i=0;i<m->count;++i)printf("%s | %s | %s\n",m->items[i].id,m->items[i].name,m->items[i].state);printf("%ld of %ld entities; SDK pages hold %d records.\n",(long)m->count,(long)m->total,AMIHASS_PAGE);}
 FreeVec(m);
done:amihass_client_close(&client);return result;}
