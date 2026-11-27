/* Shared, volatile simulation. No transport or configuration access. */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "sample.h"
#include "browser_data.h"
static const unsigned char groups[AH_SAMPLE_COUNT]={0,1,2,0,3,3,3,3,3,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3};
static const char *names[]={"Sample living room","Sample desk","Sample kitchen","Sample house"};
static int area_match(const char *area,int group)
{static const char *labels[]={"Living room","Study","Kitchen","House"};static const char *ids[]={"living_room","study","kitchen","house"};const char *p,*q;int kind;if(!*area)return 1;for(kind=0;kind<2;++kind){p=area;q=kind ? ids[group]:labels[group];while(*p&&*q&&tolower((unsigned char)*p)==tolower((unsigned char)*q)){++p;++q;}if(!*p&&!*q)return 1;}return 0;}
static void device_id(int group,char id[96]){sprintf(id,"a11a00000000000000000000%08x",group+1);}
void ah_sample_init(struct AHSample *s)
{
 unsigned int i;char json[700],id[96];const struct AHBrowserEntity *e;const char *dot;
 memset(s,0,sizeof(*s));for(i=0;i<AH_SAMPLE_COUNT;++i){e=ah_browser_entity(i);if(!e)break;dot=strchr(e->id,'.');sprintf(id,"%.*s.amihass_sample_%s",(int)(dot-e->id),e->id,dot+1);
 sprintf(json,"{\"entity_id\":\"%s\",\"state\":\"%s\",\"attributes\":%s}",id,e->state,e->attributes);amihass_state(json,&s->entities[i]);amihass_controls(json,&s->controls[i]);amihass_control_unit(&s->controls[i],NULL);s->remembered_level[i]=s->controls[i].level>0 ? s->controls[i].level:100;strcpy(s->entities[i].name,e->name);strcpy(s->entities[i].updated,"Sample initial state");}
}
static int sample_label(const char *label,const struct AmiHASSEntity *e)
{if(!*label||!strcmp(label,"amiga"))return 1;if(!strcmp(label,"lighting"))return !strncmp(e->id,"light.",6);if(!strcmp(label,"climate"))return !strncmp(e->id,"climate.",8)||!strncmp(e->id,"sensor.",7);if(!strcmp(label,"media"))return !strncmp(e->id,"media_player.",13);return 0;}
void ah_sample_request(struct AHSample *s,struct AmiHASSMessage *m,unsigned long now)
{
 int i,j,k,match,n=0,matched[AH_SAMPLE_COUNT],group=-1;char id[96],path[96],body[180],area[96],label[64];struct AmiHASSEntity *e;
 if(m->kind==AMIHASS_LABELS){static const char *ids[]={"amiga","climate","lighting","media"};static const char *names[]={"Amiga samples","Climate","Lighting","Media"};int j;m->total=4;m->count=0;if(m->offset<0)m->offset=0;for(j=m->offset;j<4&&m->count<AMIHASS_PAGE;++j){e=&m->items[m->count++];memset(e,0,sizeof(*e));strcpy(e->id,ids[j]);strcpy(e->name,names[j]);}m->result=AMIHASS_OK;return;}
 if(m->kind==AMIHASS_AREAS){
  static const char *ids[]={"house","kitchen","living_room","study"};
  static const char *labels[]={"House","Kitchen","Living room","Study"};
  int offset=m->offset;if(offset<0 || offset>100000)offset=0;m->count=0;m->total=4;m->age=0;
  for(i=offset;i<4 && m->count<AMIHASS_PAGE;++i){e=&m->items[m->count++];memset(e,0,sizeof(*e));strcpy(e->id,ids[i]);strcpy(e->name,labels[i]);}
  m->result=AMIHASS_OK;return;
 }
 strncpy(label,m->item.name,63);label[63]=0;strncpy(area,m->item.id,95);area[95]=0;m->count=m->total=0;m->age=0;if(m->kind==AMIHASS_DEVICES){
  for(i=0;i<4;++i){struct AmiHASSEntity d;memset(&d,0,sizeof(d));device_id(i,d.id);strcpy(d.name,names[i]);strcpy(d.state,"AmiHASS sample");match=0;for(k=0;k<AH_SAMPLE_COUNT;++k)if(groups[k]==i&&sample_label(label,&s->entities[k])&&amihass_matches(&s->entities[k],m->domain,NULL))match=1;if(match&&area_match(area,i)&&amihass_matches(&d,NULL,m->filter))matched[n++]=i;}
 }else if(m->kind==AMIHASS_LIST){
  if(m->entity[0]){for(i=0;i<4;++i){device_id(i,id);if(!strcmp(id,m->entity))group=i;}if(group<0){m->result=AMIHASS_FAILED;return;}}
  for(i=0;i<AH_SAMPLE_COUNT;++i)if(sample_label(label,&s->entities[i])&&area_match(area,groups[i])&&(group<0 || groups[i]==group) && amihass_matches(&s->entities[i],m->domain,m->filter))matched[n++]=i;
 }else if(m->kind==AMIHASS_QUERY || m->kind==AMIHASS_ACTION){
  if(!m->owner[0]){m->result=AMIHASS_UNCONFIGURED;return;}
  for(i=0;i<AH_SAMPLE_COUNT;++i)if(!strcmp(s->entities[i].id,m->entity))break;
  if(i==AH_SAMPLE_COUNT){m->result=AMIHASS_UNCONFIGURED;return;}e=&s->entities[i];
  if(m->kind==AMIHASS_ACTION){
   if(!amihass_action(e->id,m->action,path,body)||!amihass_control_allowed(&s->controls[i],m->action)){m->result=AMIHASS_UNSUPPORTED;return;}
   if(!amihass_available(e) && strncmp(e->id,"scene.",6)){m->result=AMIHASS_UNAVAILABLE;return;}
   if(!strcmp(m->action,"Press")){++s->controls[i].delivery;}
   else if(!strcmp(m->action,"Lock"))strcpy(e->state,"locked");
   else if(!strcmp(m->action,"Unlock"))strcpy(e->state,"unlocked");
   else if(!strcmp(m->action,"Open lock"))strcpy(e->state,"open");
   else if(!strcmp(m->action,"Play"))strcpy(e->state,"playing");
   else if(!strcmp(m->action,"Pause"))strcpy(e->state,"paused");
   else if(!strcmp(m->action,"Next")||!strcmp(m->action,"Previous")){strcpy(s->controls[i].media_title,!strcmp(m->action,"Next") ? "Next sample track":"Previous sample track");}
   else if(!strncmp(m->action,"Volume ",7)){s->controls[i].volume_known=1;s->controls[i].volume=atoi(m->action+7);}
   else if(!strncmp(m->action,"Brightness ",11)){s->controls[i].level_known=1;s->controls[i].level=atoi(m->action+11);strcpy(e->state,s->controls[i].level ? "on":"off");}
   else if(!strncmp(m->action,"Color ",6)){s->controls[i].color_known=1;s->controls[i].rgb=strtoul(m->action+7,NULL,16);strcpy(e->state,"on");if(!s->controls[i].level)s->controls[i].level=100;}
   else if(!strncmp(m->action,"Fan speed ",10)){s->controls[i].speed_known=1;s->controls[i].speed=atoi(m->action+10);strcpy(e->state,s->controls[i].speed ? "on":"off");}
   else if(!strncmp(m->action,"Position ",9)){s->controls[i].position_known=1;s->controls[i].position_value=atoi(m->action+9);strcpy(e->state,s->controls[i].position_value ? "open":"closed");}
   else if(!strcmp(m->action,"Open cover") || !strcmp(m->action,"Close cover")){s->controls[i].position_known=1;s->controls[i].position_value=!strcmp(m->action,"Open cover") ? 100:0;strcpy(e->state,s->controls[i].position_value ? "open":"closed");}
   else if(!strcmp(m->action,"Stop cover")){strcpy(e->state,s->controls[i].position_value ? "open":"closed");}
   else if(!strncmp(m->action,"Temperature ",12)){amihass_number(m->action+12,&s->controls[i].target);}
   else if(!strcmp(m->action,"Turn off"))strcpy(e->state,"off");
   else if(!strcmp(m->action,"Toggle"))strcpy(e->state,!strcmp(e->state,"off") ? "on":"off");
   else strcpy(e->state,"on");
   if(s->controls[i].brightness&&!strncmp(e->id,"light.",6)){
    if(s->controls[i].level>0)s->remembered_level[i]=s->controls[i].level;
    if(!strcmp(e->state,"on")&&s->controls[i].level==0){s->controls[i].level=s->remembered_level[i];s->controls[i].level_known=1;}
   }
   sprintf(e->updated,"Sample change %lu.%lu",now,++s->revision);m->result=AMIHASS_WAIT;
  }else m->result=amihass_available(e) ? AMIHASS_OK:AMIHASS_UNAVAILABLE;
  m->item=*e;{struct AHControls controls=s->controls[i];if(controls.brightness&&!strcmp(e->state,"off")){controls.level=0;controls.level_known=1;}if(controls.fan_speed&&!strcmp(e->state,"off")){controls.speed=0;controls.speed_known=1;}m->count=amihass_control_items(&controls,m->items,AMIHASS_PAGE);}return;
 }else{m->result=AMIHASS_UNSUPPORTED;return;}
 if(m->offset<0 || m->offset>100000)m->offset=0;m->total=n;
 for(i=(int)m->offset;i<n && m->count<AMIHASS_PAGE;++i){j=matched[i];e=&m->items[m->count++];if(m->kind==AMIHASS_DEVICES){memset(e,0,sizeof(*e));device_id(j,e->id);strcpy(e->name,names[j]);strcpy(e->state,"AmiHASS sample");}else *e=s->entities[j];}
 m->result=AMIHASS_OK;
}
