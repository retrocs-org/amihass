/* Bounded selected-entity cache. JSON attributes stay typed until the same
 * capability parser used by REST validates them. No actions are sent here. */
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "push_state.h"
static const char *space(const char *p){while(p&&isspace((unsigned char)*p))++p;return p;}
static int append(char *out,unsigned int cap,const char *p,unsigned int n)
{unsigned int used=(unsigned int)strlen(out);if(used+n>=cap)return 0;memcpy(out+used,p,n);out[used+n]=0;return 1;}
/* 1 when key is listed in a JSON string array, 0 when not, -1 when malformed. */
static int removed(const char *list,const char *key)
{char name[96];const char *end;if(!list)return 0;if(*list++!='[')return -1;list=space(list);while(*list!=']'){end=ah_json_end(list);if(!end)return -1;if(ah_json_text(list,name,sizeof(name),1)&&!strcmp(name,key))return 1;list=space(end);if(*list==']')break;if(*list++!=',')return -1;list=space(list);}return 0;}
/* Copy wanted members of old (minus removals and replacements) and of plus.
 * A member that does not fit is left out; only malformed JSON fails. */
static int merge(char out[AH_PUSH_ATTRS],const char *old,const char *plus,const char *minus)
{const char *p,*key,*end,*value;char name[64];int first=1,drop,phase;strcpy(out,"{");for(phase=0;phase<2;++phase){p=phase ? plus:old;if(!p)continue;if(*p++!='{')return 0;p=space(p);while(*p!='}'){key=p;end=ah_json_end(p);if(!end||*p!='"'||*space(end)!=':')return 0;value=space(space(end)+1);end=ah_json_end(value);if(!end)return 0;drop=!ah_json_text(key,name,sizeof(name),1)||!amihass_attribute_wanted(name);if(!drop&&!phase){int gone=removed(minus,name);if(gone<0)return 0;drop=gone||(plus&&ah_json_member(plus,name));}if(!drop){unsigned int used=(unsigned int)strlen(out),n=(unsigned int)(end-key);if(used+n+(first ? 0:1)+2<AH_PUSH_ATTRS){if(!first)append(out,AH_PUSH_ATTRS,",",1);append(out,AH_PUSH_ATTRS,key,n);first=0;}}p=space(end);if(*p=='}')break;if(*p++!=',')return 0;p=space(p);}}return append(out,AH_PUSH_ATTRS,"}",1);}
int ah_push_select(struct AHPushState *s,const char ids[][96],unsigned int count)
{unsigned int i;if(count>AH_PUSH_SELECTED)return 0;/* none: notifications only */memset(s,0,sizeof(*s));s->next_id=1;for(i=0;i<count;++i)if(ah_push_add(s,ids[i])<0){memset(s,0,sizeof(*s));return 0;}return 1;}
int ah_push_find(const struct AHPushState *s,const char *id)
{unsigned int i;for(i=0;i<s->count;++i)if(!strcmp(s->entities[i].id,id))return (int)i;return -1;}
int ah_push_add(struct AHPushState *s,const char *id)
{struct AHPushEntity *e;if(!id||!amihass_entity_id(id)||ah_push_find(s,id)>=0||s->count>=AH_PUSH_SELECTED)return -1;if(!s->next_id)s->next_id=1;e=&s->entities[s->count];memset(e,0,sizeof(*e));strcpy(e->id,id);strcpy(e->attrs,"{}");e->sub=s->next_id++;return (int)s->count++;}
int ah_push_remove(struct AHPushState *s,const char *id,unsigned long *sub)
{int i=ah_push_find(s,id);if(i<0)return 0;if(sub)*sub=s->entities[i].sub;--s->count;if((unsigned int)i<s->count)memmove(&s->entities[i],&s->entities[i+1],(s->count-(unsigned int)i)*sizeof(s->entities[0]));return 1;}
static int emit(struct AHPushEntity *e,AHPushUpdate update,void *user)
{char json[AH_PUSH_ATTRS+420],quoted[250];const unsigned char *p;unsigned int n=0;struct AmiHASSEntity value;struct AHControls controls;
 for(p=(const unsigned char*)e->state;*p;++p){if(n+7>=sizeof(quoted))return 0;if(*p=='"'||*p=='\\')quoted[n++]='\\';if(*p<32)return 0;quoted[n++]=*p;}quoted[n]=0;
 sprintf(json,"{\"entity_id\":\"%s\",\"state\":\"%s\",\"attributes\":%s}",e->id,quoted,e->attrs);if(!amihass_state(json,&value))return 0;memset(&controls,0,sizeof(controls));amihass_controls(json,&controls);return update(user,&value,&controls);}
static int subscription_id(const char *p,unsigned long *id)
{unsigned long v=0;if(!p||!isdigit((unsigned char)*p))return 0;while(isdigit((unsigned char)*p)){if(v>99999999UL)return 0;v=v*10UL+(unsigned long)(*p++-'0');}if(!strchr(",} \r\n\t",*p))return 0;*id=v;return 1;}
/* Returns 0 only for malformed JSON or a failed client update. Events for
 * a subscription that was just released are ignored. */
int ah_push_event(struct AHPushState *s,const char *json,AHPushUpdate update,void *user)
{const char *event,*group,*entry,*plus,*minus,*state,*attrs,*end;unsigned int i;unsigned long id;char type[24],next[AH_PUSH_ATTRS];struct AHPushEntity *e=NULL;
 end=ah_json_end(json);if(!end||*space(end)||!ah_json_text(ah_json_member(json,"type"),type,sizeof(type),1)||strcmp(type,"event"))return 0;if(!subscription_id(ah_json_member(json,"id"),&id))return 0;event=ah_json_member(json,"event");if(!event||*event!='{')return 0;
 for(i=0;i<s->count;++i)if(s->entities[i].sub==id){e=&s->entities[i];break;}if(!e)return 1;
 group=ah_json_member(event,"a");entry=group ? ah_json_member(group,e->id):NULL;
 if(entry){state=ah_json_member(entry,"s");attrs=ah_json_member(entry,"a");if(!state||!attrs||*attrs!='{')return 0;e->valid=0;if(!ah_json_text_prefix(state,e->state,sizeof(e->state),1)||!merge(next,attrs,NULL,NULL))return 0;strcpy(e->attrs,next);e->valid=1;if(!emit(e,update,user))return 0;}
 group=ah_json_member(event,"c");entry=group ? ah_json_member(group,e->id):NULL;
 /* A change without a held full state (e.g. it was too large) stays on REST. */
 if(entry&&e->valid){plus=ah_json_member(entry,"+");minus=ah_json_member(entry,"-");state=plus ? ah_json_member(plus,"s"):NULL;if(state&&!ah_json_text_prefix(state,e->state,sizeof(e->state),1))return 0;attrs=plus ? ah_json_member(plus,"a"):NULL;minus=minus ? ah_json_member(minus,"a"):NULL;if(attrs||minus){if(!merge(next,e->attrs,attrs,minus))return 0;strcpy(e->attrs,next);}if(!emit(e,update,user))return 0;}
 group=ah_json_member(event,"r");if(group){int gone=removed(group,e->id);if(gone<0)return 0;if(gone){strcpy(e->state,"unavailable");strcpy(e->attrs,"{}");e->valid=0;if(!emit(e,update,user))return 0;}}
 return 1;}
/* event = the "event" member: {"type":"current|added|updated|removed","notifications":{id:{...},...}} */
int ah_push_notifications(const char *event,AHNoteUpdate update,void *user)
{
 const char *map,*p,*end,*value;char type[16];struct AHNote n;int op;
 if(!event||*event!='{'||!ah_json_text(ah_json_member(event,"type"),type,sizeof(type),1))return 0;
 op=!strcmp(type,"removed")?AH_NOTE_REMOVE:(!strcmp(type,"added")||!strcmp(type,"updated")||!strcmp(type,"current"))?AH_NOTE_SET:0;if(!op)return 1;/* a future update type is ignored */
 map=ah_json_member(event,"notifications");if(!map||*map!='{')return 0;
 if(!strcmp(type,"current")&&!update(user,AH_NOTE_CLEAR,NULL))return 0;
 p=space(map+1);while(*p!='}'){end=ah_json_end(p);if(!end||*p!='"'||*space(end)!=':')return 0;value=space(space(end)+1);end=ah_json_end(value);if(!end)return 0;
  memset(&n,0,sizeof(n));if(!ah_json_text_prefix(p,n.id,sizeof(n.id),0))return 0;
  if(*value=='{'){ah_json_text_prefix(ah_json_member(value,"title"),n.title,sizeof(n.title),0);ah_json_text_prefix(ah_json_member(value,"message"),n.message,sizeof(n.message),0);ah_json_text_prefix(ah_json_member(value,"created_at"),n.created,sizeof(n.created),1);}
  if(n.id[0]&&!update(user,op,&n))return 0;
  p=space(end);if(*p=='}')break;if(*p++!=',')return 0;p=space(p);}
 return 1;
}
