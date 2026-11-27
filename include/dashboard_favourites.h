#ifndef AH_DASHBOARD_FAVOURITES_H
#define AH_DASHBOARD_FAVOURITES_H
#include <stdio.h>
#include <string.h>
#include "amihass_core.h"
#include "recovery_io.h"
#define AH_FAVOURITES 32
#define AH_FAVOURITES_FILE "ENVARC:AmiHASS/Favourites.cfg"
struct AHFavourite {char source[160],id[96],name[64];};
struct AHFavourites {unsigned int count;struct AHFavourite items[AH_FAVOURITES];};
static int ah_favourite_valid(const struct AHFavourite *e)
{return e->source[0]&&!strpbrk(e->source,"\r\n\t")&&amihass_entity_id(e->id)&&e->name[0]&&!strpbrk(e->name,"\r\n\t");}
static int ah_favourite_set(struct AHFavourites *f,const struct AHFavourite *e)
{unsigned int i;if(!ah_favourite_valid(e))return 0;for(i=0;i<f->count;++i)if(!strcmp(f->items[i].source,e->source)&&!strcmp(f->items[i].id,e->id))break;if(i==f->count){if(i==AH_FAVOURITES)return 0;++f->count;}f->items[i]=*e;return 1;}
static void ah_favourite_remove(struct AHFavourites *f,unsigned int i)
{if(i<f->count){memmove(&f->items[i],&f->items[i+1],(f->count-i-1)*sizeof(f->items[0]));--f->count;}}
static int ah_favourites_line(FILE *f,char *out,unsigned int cap)
{char *n;if(!fgets(out,cap,f))return 0;n=strchr(out,'\n');if(!n)return 0;*n=0;return !strchr(out,'\r');}
static int ah_favourites_load(struct AHFavourites *out,const char *path)
{FILE *f;char header[32],backup[300];struct AHFavourites draft;struct AHFavourite e;int ok=1;memset(out,0,sizeof(*out));memset(&draft,0,sizeof(draft));f=fopen(path,"rb");if(!f){if(!ah_file_missing(path)||strlen(path)>290)return 0;sprintf(backup,"%s.old",path);f=fopen(backup,"rb");if(!f)return ah_file_missing(backup);}if(!ah_favourites_line(f,header,sizeof(header))||strcmp(header,"AMIHASSFAVOURITES1"))ok=0;
 while(ok&&fgetc(f)!=EOF){fseek(f,-1,SEEK_CUR);memset(&e,0,sizeof(e));if(!ah_favourites_line(f,e.source,sizeof(e.source))||!ah_favourites_line(f,e.id,sizeof(e.id))||!ah_favourites_line(f,e.name,sizeof(e.name))||!ah_favourite_set(&draft,&e))ok=0;}
 if(ferror(f))ok=0;fclose(f);if(ok)*out=draft;return ok;}
static int ah_favourites_save(const struct AHFavourites *f,const char *path)
{FILE *fp;char fresh[300],old[300];unsigned int i;int ok=1,existed=0;if(f->count>AH_FAVOURITES||strlen(path)>290)return 0;for(i=0;i<f->count;++i)if(!ah_favourite_valid(&f->items[i]))return 0;sprintf(fresh,"%s.new",path);sprintf(old,"%s.old",path);fp=fopen(fresh,"wb");if(!fp)return 0;ok=fputs("AMIHASSFAVOURITES1\n",fp)>=0;for(i=0;ok&&i<f->count;++i)ok=fprintf(fp,"%s\n%s\n%s\n",f->items[i].source,f->items[i].id,f->items[i].name)>0;if(fclose(fp))ok=0;if(!ok){remove(fresh);return 0;}fp=fopen(path,"rb");if(fp){fclose(fp);if(remove(old)&&!ah_file_missing(old)){remove(fresh);return 0;}if(rename(path,old)){remove(fresh);return 0;}existed=1;}else if(!ah_file_missing(path)){remove(fresh);return 0;}if(rename(fresh,path)){if(existed)rename(old,path);remove(fresh);return 0;}remove(old);return 1;}
#endif
