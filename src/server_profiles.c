/* Saved server profiles; see server_profiles.h. */
#include <stdio.h>
#include <string.h>
#include "server_profiles.h"
#include "amihass_core.h"
static int name_ok(const char *n){unsigned int i;if(!n||!n[0]||strlen(n)>32)return 0;for(i=0;n[i];++i)if((unsigned char)n[i]<32||n[i]=='\t'||(unsigned char)n[i]==127)return 0;return 1;}
void ah_servers_erase(struct AHServerProfiles *p){unsigned int i;for(i=0;i<AH_SERVERS;++i)amihass_erase(p->items[i].token,sizeof(p->items[i].token));}
int ah_servers_find(const struct AHServerProfiles *p,const char *name){unsigned int i;for(i=0;i<p->count;++i)if(!strcmp(p->items[i].name,name))return (int)i;return -1;}
int ah_servers_set(struct AHServerProfiles *p,const char *name,const char *url,const char *token)
{struct AHServerProfile e;int i;char clean[1025];if(!name_ok(name)||!url||strlen(url)>=sizeof(e.url))return 0;memset(&e,0,sizeof(e));strcpy(e.url,url);amihass_url_clean(e.url);
 if(!amihass_url(e.url,!strncmp(e.url,"http://",7))||!amihass_token(token,clean))return 0;strcpy(e.name,name);strcpy(e.token,clean);amihass_erase(clean,sizeof(clean));
 i=ah_servers_find(p,name);e.reconnect=i>=0 ? p->items[i].reconnect:1;e.retry_interval=i>=0 ? p->items[i].retry_interval:60;if(i<0){if(p->count>=AH_SERVERS){amihass_erase(&e,sizeof(e));return 0;}i=(int)p->count++;}
 amihass_erase(&p->items[i],sizeof(p->items[i]));p->items[i]=e;amihass_erase(&e,sizeof(e));return 1;}
int ah_servers_options(struct AHServerProfiles *p,const char *name,int reconnect,unsigned int retry_interval)
{int i=ah_servers_find(p,name);if(i<0||(reconnect!=0&&reconnect!=1)||(retry_interval!=60&&retry_interval!=600&&retry_interval!=900&&retry_interval!=1800))return 0;p->items[i].reconnect=reconnect;p->items[i].retry_interval=retry_interval;return 1;}
int ah_servers_remove(struct AHServerProfiles *p,const char *name)
{int i=ah_servers_find(p,name);if(i<0)return 0;amihass_erase(&p->items[i],sizeof(p->items[i]));memmove(&p->items[i],&p->items[i+1],(p->count-(unsigned int)i-1)*sizeof(p->items[0]));--p->count;memset(&p->items[p->count],0,sizeof(p->items[0]));return 1;}
/* Lines are name<TAB>value; a profile needs both its address and its token. */
int ah_servers_load(struct AHServerProfiles *p,const char *file,const char *tokens)
{FILE *f;char line[1200],*tab,*extra;unsigned int i,retry;int modern=0,reconnect;struct AHServerProfiles urls;memset(p,0,sizeof(*p));memset(&urls,0,sizeof(urls));
 f=fopen(file,"rb");if(!f)return 0;if(!fgets(line,sizeof(line),f)||(strcmp(line,"AMIHASSSERVERS1\n")&&strcmp(line,"AMIHASSSERVERS2\n"))){fclose(f);return 0;}modern=!strcmp(line,"AMIHASSSERVERS2\n");
 while(fgets(line,sizeof(line),f)&&urls.count<AH_SERVERS){line[strcspn(line,"\r\n")]=0;tab=strchr(line,'\t');if(!tab)continue;*tab++=0;reconnect=1;retry=60;extra=strchr(tab,'\t');if(modern){char tail;if(!extra)continue;*extra++=0;if(sscanf(extra,"%d\t%u%c",&reconnect,&retry,&tail)!=2||(reconnect!=0&&reconnect!=1)||(retry!=60&&retry!=600&&retry!=900&&retry!=1800))continue;}if(!name_ok(line)||strlen(tab)>=sizeof(urls.items[0].url)||ah_servers_find(&urls,line)>=0)continue;strcpy(urls.items[urls.count].name,line);strcpy(urls.items[urls.count].url,tab);urls.items[urls.count].reconnect=reconnect;urls.items[urls.count].retry_interval=retry;++urls.count;}
 fclose(f);f=fopen(tokens,"rb");if(!f)return 1;
 if(fgets(line,sizeof(line),f)&&!strcmp(line,"AMIHASSSERVERTOKENS1\n"))while(fgets(line,sizeof(line),f)){line[strcspn(line,"\r\n")]=0;tab=strchr(line,'\t');if(!tab)continue;*tab++=0;for(i=0;i<urls.count;++i)if(!strcmp(urls.items[i].name,line)){if(ah_servers_set(p,line,urls.items[i].url,tab))ah_servers_options(p,line,urls.items[i].reconnect,urls.items[i].retry_interval);}}
 amihass_erase(line,sizeof(line));fclose(f);return 1;}
static int write_file(const char *path,const char *head,const struct AHServerProfiles *p,int tokens)
{FILE *f;char fresh[300],old[300];unsigned int i;int ok,existed=0;if(strlen(path)>290)return 0;sprintf(fresh,"%s.new",path);sprintf(old,"%s.old",path);f=fopen(fresh,"wb");if(!f)return 0;
 ok=fputs(head,f)>=0;for(i=0;ok&&i<p->count;++i)ok=(tokens ? fprintf(f,"%s\t%s\n",p->items[i].name,p->items[i].token):fprintf(f,"%s\t%s\t%d\t%u\n",p->items[i].name,p->items[i].url,p->items[i].reconnect,p->items[i].retry_interval))>0;if(fclose(f))ok=0;
 if(ok){FILE *o=fopen(path,"rb");if(o){fclose(o);remove(old);if(rename(path,old))ok=0;else existed=1;}if(ok&&rename(fresh,path)){if(existed)rename(old,path);ok=0;}
  /* The previous token list is overwritten before it is deleted. */
  if(ok&&existed){if(tokens){FILE *w=fopen(old,"r+b");if(w){long n;fseek(w,0,SEEK_END);n=ftell(w);fseek(w,0,SEEK_SET);while(n-->0)fputc(0,w);fclose(w);}}remove(old);}}
 if(!ok)remove(fresh);return ok;}
int ah_servers_save(const struct AHServerProfiles *p,const char *file,const char *tokens)
{return write_file(tokens,"AMIHASSSERVERTOKENS1\n",p,1)&&write_file(file,"AMIHASSSERVERS2\n",p,0);}
