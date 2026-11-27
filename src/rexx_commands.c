/* Bounded command grammar shared by the ARexx host and its tests. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rexx_bridge.h"
int ah_rexx_parse(const char *text,struct AmiHASSMessage *m)
{char words[6][96];unsigned int count=0,n;char quote;const unsigned char *p=(const unsigned char *)text;int i;char *end;long offset;if(!p||strlen(text)>512)return 0;memset(m,0,sizeof(*m));strcpy(m->owner,"AmiHASS.ARexx");
 while(*p){while(isspace(*p))++p;if(!*p)break;if(count==6)return 0;n=0;quote=(*p=='\''||*p=='"') ? *p++:0;while(*p&&(quote ? *p!=quote:!isspace(*p))){if(*p<32||*p==127||n>=95)return 0;words[count][n++]=(char)*p++;}words[count++][n]=0;if(quote){if(*p++!=quote)return 0;if(*p&&!isspace(*p))return 0;}}
 if(!count)return 0;for(i=0;words[0][i];++i)words[0][i]=(char)toupper((unsigned char)words[0][i]);
 if(!strcmp(words[0],"HELP")&&count==1)return 1;
 if(!strcmp(words[0],"STATUS")&&count==1)m->kind=AMIHASS_STATUS;
 else if(!strcmp(words[0],"QUERY")&&count==2&&amihass_entity_id(words[1])){m->kind=AMIHASS_QUERY;m->refresh=AMIHASS_QUERY_ONCE;strcpy(m->entity,words[1]);}
 else if(!strcmp(words[0],"ACTION")&&(count==3||(count==4&&amihass_alarm_code(words[3])))&&amihass_entity_id(words[1])&&strlen(words[2])<20){m->kind=AMIHASS_ACTION;strcpy(m->entity,words[1]);strcpy(m->action,words[2]);if(count==4)strcpy(m->filter,words[3]);/* alarm code, used once */}
 else if(!strcmp(words[0],"DEVICES")&&count<=3){m->kind=AMIHASS_DEVICES;if(count>1){if(strlen(words[1])>63)return 0;strcpy(m->filter,words[1]);}if(count==3){offset=strtol(words[2],&end,10);if(*end||offset<0||offset>100000)return 0;m->offset=offset;}}
 else if((!strcmp(words[0],"LIST")&&count<=4)||(!strcmp(words[0],"ENTITIES")&&count>=2&&count<=5)){m->kind=AMIHASS_LIST;i=1;if(!strcmp(words[0],"ENTITIES")){if(!amihass_device_id(words[1]))return 0;strcpy(m->entity,words[i++]);}if((unsigned int)i<count){if(strlen(words[i])>23)return 0;strcpy(m->domain,words[i++]);}if((unsigned int)i<count){if(strlen(words[i])>63)return 0;strcpy(m->filter,words[i++]);}if((unsigned int)i<count){offset=strtol(words[i],&end,10);if(*end||offset<0||offset>100000)return 0;m->offset=offset;}}
 else if(!strcmp(words[0],"ATTRIBUTES")&&count>=2&&count<=3&&amihass_entity_id(words[1])){m->kind=AMIHASS_ATTRIBUTES;strcpy(m->entity,words[1]);if(count==3){offset=strtol(words[2],&end,10);if(*end||offset<0||offset>64)return 0;m->offset=offset;}}
 else if(!strcmp(words[0],"HISTORY")&&count>=2&&count<=4&&amihass_entity_id(words[1])){m->kind=AMIHASS_HISTORY;strcpy(m->entity,words[1]);m->refresh=24;if(count>=3){offset=strtol(words[2],&end,10);if(*end||offset<1||offset>48)return 0;m->refresh=offset;}if(count==4){offset=strtol(words[3],&end,10);if(*end||offset<0||offset>64)return 0;m->offset=offset;}}
 else if(!strcmp(words[0],"NOTIFICATIONS")&&count<=2){m->kind=AMIHASS_NOTIFICATIONS;if(count==2){offset=strtol(words[1],&end,10);if(*end||offset<0||offset>8)return 0;m->offset=offset;}}
 else if(!strcmp(words[0],"NOTIFICATION")&&count==2&&words[1][0]){m->kind=AMIHASS_NOTIFICATIONS;strcpy(m->entity,words[1]);}
 else if(!strcmp(words[0],"WAITEVENT")&&count>=2&&count<=4){/* one entity, or up to 8 separated by commas */
  char *list=words[1],*comma;int n=0;while(list&&*list){comma=strchr(list,',');if(comma)*comma=0;if(n>=AH_WAIT_MAX||!amihass_entity_id(list))return 0;strcpy(m->items[n++].id,list);list=comma?comma+1:NULL;}
  if(!n)return 0;m->count=n;m->kind=n==1?AMIHASS_SUBSCRIBE:AMIHASS_SUBSCRIBE_MANY;m->refresh=AMIHASS_QUERY_INTERVAL(5);strcpy(m->entity,m->items[0].id);strcpy(m->domain,AH_REXX_WAIT_DOMAIN);m->offset=60;
  if(count>=3){offset=strtol(words[2],&end,10);if(*end||offset<1||offset>3600)return 0;m->offset=offset;}if(count==4){if(strlen(words[3])>39)return 0;strcpy(m->filter,words[3]);}}
 else return 0;return 1;}
static int wait_reading(struct AHRexxWait *w,const struct AmiHASSEntity *e)
{int j,hit;for(j=0;j<w->count;++j)if(!strcmp(w->ids[j],e->id))break;if(j==w->count)return 0;
 /* Only state changes count; attribute-only updates do not end a wait. With
  * a target state, any reading in that state ends it (also the first). */
 hit=w->target[0] ? !strcmp(e->state,w->target):w->primed[j]&&strcmp(e->state,w->states[j]);
 if(hit){strcpy(w->entity,e->id);strcpy(w->previous,w->primed[j] ? w->states[j]:"");strcpy(w->state,e->state);strcpy(w->updated,e->updated);}
 strcpy(w->states[j],e->state);w->primed[j]=1;return hit;}
int ah_rexx_wait_check(struct AHRexxWait *w,const struct AmiHASSMessage *r,long now)
{
 int i,usable=0;
 if(r->result==AMIHASS_UNSUPPORTED||r->result==AMIHASS_UNCONFIGURED)return 3;
 if(r->kind==AMIHASS_SUBSCRIBE_MANY||r->kind==AMIHASS_QUERY_MANY){if(r->result==AMIHASS_OK){for(i=0;i<r->count&&i<AMIHASS_PAGE;++i){int res=AMIHASS_ITEM_RESULT(r,i);if(res!=AMIHASS_UNSUPPORTED)++usable;if((res==AMIHASS_OK||res==AMIHASS_STALE)&&wait_reading(w,&r->items[i]))return 1;}if(r->count>0&&!usable)return 3;}}
 else if(r->result==AMIHASS_OK&&wait_reading(w,&r->item))return 1;
 return now>=w->deadline ? 2:0;
}
void ah_rexx_format(const struct AmiHASSMessage *m,char *out,unsigned int cap)
{unsigned int used;int i,n;if(!cap)return;out[0]=0;
 if(m->kind==AMIHASS_NOTIFICATIONS&&m->item.id[0]){n=snprintf(out,cap,"id=%s\ntitle=%s\ncreated=%s\nmessage=",m->item.id,m->item.name,m->item.updated);for(i=0;n>0&&i<m->count&&i<AMIHASS_PAGE;++i){used=strlen(out);if(used+strlen(m->items[i].name)+1>=cap)break;strcat(out,m->items[i].name);}out[cap-1]=0;return;}
 if(m->kind==AMIHASS_NOTIFICATIONS){snprintf(out,cap,"total=%ld\nserial=%lu\ncount=%ld",(long)m->total,(unsigned long)m->age,(long)m->count);for(i=0;i<m->count&&i<AMIHASS_PAGE;++i){used=strlen(out);if(used+2>=cap)break;n=snprintf(out+used,cap-used,"\n%s\t%s\t%s\t%s",m->items[i].id,m->items[i].name,m->items[i].updated,m->items[i].state);if(n<0||(unsigned int)n>=cap-used)break;}out[cap-1]=0;return;}
 if(m->kind==AMIHASS_STATUS)snprintf(out,cap,"state=%s\nready=%ld\nsource=%s\nselected=%ld",m->item.state,(long)m->status.connected,m->status.server,(long)m->status.subscriptions);
 else if(m->kind==AMIHASS_QUERY||m->kind==AMIHASS_ACTION)snprintf(out,cap,"result=%ld\nid=%s\nname=%s\nstate=%s\nunit=%s",(long)m->result,m->item.id,m->item.name,m->item.state,m->item.unit);
 else{snprintf(out,cap,"total=%ld\noffset=%ld\ncount=%ld",(long)m->total,(long)m->offset,(long)m->count);for(i=0;i<m->count&&i<AMIHASS_PAGE;++i){used=strlen(out);if(used+2>=cap)break;n=m->kind==AMIHASS_HISTORY ? snprintf(out+used,cap-used,"\n%s\t%s",m->items[i].updated,m->items[i].state):m->kind==AMIHASS_ATTRIBUTES ? snprintf(out+used,cap-used,"\n%s\t%s\t%s",m->items[i].id,m->items[i].name,m->items[i].device_class):snprintf(out+used,cap-used,"\n%s\t%s\t%s",m->items[i].id,m->items[i].name,m->items[i].state);if(n<0||(unsigned int)n>=cap-used)break;}}if(m->kind==AMIHASS_QUERY){for(i=0;i<m->count&&i<AMIHASS_PAGE;++i){used=strlen(out);if(used+2>=cap)break;n=snprintf(out+used,cap-used,"\n%s=%s",m->items[i].id,m->items[i].state);if(n<0||(unsigned int)n>=cap-used)break;}}out[cap-1]=0;}
