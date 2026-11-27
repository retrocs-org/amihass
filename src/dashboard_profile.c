/* Per-page refresh profiles and time-of-day pages; see dashboard_profile.h. */
#include <stdio.h>
#include <string.h>
#include "dashboard_profile.h"
static int valid(const struct AHPageProfile *e)
{return e->name[0]&&strlen(e->name)<33&&!strpbrk(e->name,"\r\n")&&e->profile<=AH_PROFILE_VERY_SLOW&&e->from<=AH_PROFILE_NEVER&&e->until<24;}
static int is_default(const struct AHPageProfile *e){return e->profile==AH_PROFILE_NORMAL&&e->from==AH_PROFILE_NEVER;}
struct AHPageProfile ah_profile_get(const struct AHPageProfiles *p,const char *name)
{struct AHPageProfile e;unsigned int i;memset(&e,0,sizeof(e));strncpy(e.name,name?name:"",32);e.from=AH_PROFILE_NEVER;for(i=0;p&&i<p->count;++i)if(!strcmp(p->pages[i].name,e.name))return p->pages[i];return e;}
int ah_profile_set(struct AHPageProfiles *p,const struct AHPageProfile *s)
{unsigned int i;if(!valid(s))return 0;for(i=0;i<p->count;++i)if(!strcmp(p->pages[i].name,s->name))break;
 if(is_default(s)){if(i<p->count){memmove(&p->pages[i],&p->pages[i+1],(p->count-i-1)*sizeof(p->pages[0]));--p->count;}return 1;}
 if(i==p->count){if(p->count>=AH_DASH_PAGES)return 0;++p->count;}p->pages[i]=*s;return 1;}
void ah_profile_rename(struct AHPageProfiles *p,const char *from,const char *to)
{unsigned int i;for(i=0;i<p->count;++i)if(!strcmp(p->pages[i].name,from)){memset(p->pages[i].name,0,sizeof(p->pages[i].name));strncpy(p->pages[i].name,to,32);}}
void ah_profile_prune(struct AHPageProfiles *p,const struct AHDashboardBook *b)
{unsigned int i,j;for(i=0;i<p->count;){for(j=0;j<b->count;++j)if(!strcmp(b->pages[j].name,p->pages[i].name))break;if(j==b->count){memmove(&p->pages[i],&p->pages[i+1],(p->count-i-1)*sizeof(p->pages[0]));--p->count;}else ++i;}}
unsigned int ah_profile_seconds(unsigned int profile){return profile==AH_PROFILE_FAST?5U:profile==AH_PROFILE_SLOW?60U:profile==AH_PROFILE_VERY_SLOW?300U:0U;}
int ah_profile_page_for_hour(const struct AHPageProfiles *p,const struct AHDashboardBook *b,unsigned int hour)
{unsigned int i,j;const struct AHPageProfile *e;if(hour>23)return -1;
 for(j=0;j<b->count;++j)for(i=0;i<p->count;++i){e=&p->pages[i];if(strcmp(e->name,b->pages[j].name)||e->from>=24)continue;
  if(e->from==e->until||(e->from<e->until?hour>=e->from&&hour<e->until:hour>=e->from||hour<e->until))return (int)j;}
 return -1;}
int ah_profiles_load(struct AHPageProfiles *p,const char *path)
{FILE *f;char line[80];struct AHPageProfile e;int n;memset(p,0,sizeof(*p));f=fopen(path,"rb");if(!f)return 0;
 if(!fgets(line,sizeof(line),f)||strcmp(line,"AMIHASSPAGES1\n")){fclose(f);return 0;}
 while(fgets(line,sizeof(line),f)&&p->count<AH_DASH_PAGES){memset(&e,0,sizeof(e));n=0;line[strcspn(line,"\r\n")]=0;
  if(sscanf(line,"%u %u %u %n",&e.profile,&e.from,&e.until,&n)!=3||!n||strlen(line+n)>32)continue;strcpy(e.name,line+n);if(valid(&e)&&!is_default(&e))ah_profile_set(p,&e);}
 fclose(f);return 1;}
int ah_profiles_save(const struct AHPageProfiles *p,const char *path)
{FILE *f;char fresh[300];unsigned int i;int ok;if(strlen(path)>280)return 0;sprintf(fresh,"%s.new",path);f=fopen(fresh,"wb");if(!f)return 0;
 ok=fputs("AMIHASSPAGES1\n",f)>=0;for(i=0;ok&&i<p->count;++i)ok=fprintf(f,"%u %u %u %s\n",p->pages[i].profile,p->pages[i].from,p->pages[i].until,p->pages[i].name)>0;
 /* Keep the old file until the new one is in place (AmigaDOS rename does not replace). */
 if(fclose(f))ok=0;if(ok){char old[300];FILE *o;int existed=0;sprintf(old,"%s.old",path);o=fopen(path,"rb");if(o){fclose(o);remove(old);if(rename(path,old))ok=0;else existed=1;}
  if(ok&&rename(fresh,path)){if(existed)rename(old,path);ok=0;}if(ok&&existed)remove(old);}
 if(!ok)remove(fresh);return ok;}
