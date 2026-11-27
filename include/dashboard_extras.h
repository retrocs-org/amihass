#ifndef AH_DASHBOARD_EXTRAS_H
#define AH_DASHBOARD_EXTRAS_H
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
#include "recovery_io.h"
#define AH_CAMERA_POLICY_FILE "ENVARC:AmiHASS/CameraRefresh.cfg"
static int ah_camera_interval_valid(unsigned int n){return !n||n==60||n==300||n==900;}
static unsigned int ah_camera_policy_load(const char *path)
{FILE *f;unsigned int n=0;char header[32],line[32],extra;f=fopen(path,"rb");if(!f)return 0;if(!fgets(header,sizeof(header),f)||strcmp(header,"AMIHASSCAMERA1\n")||!fgets(line,sizeof(line),f)||sscanf(line,"%u %c",&n,&extra)!=1||fgetc(f)!=EOF||!ah_camera_interval_valid(n))n=0;fclose(f);return n;}
static int ah_camera_policy_save(const char *path,unsigned int n)
{FILE *f;char fresh[300],old[300];int ok,existed=0;if(!ah_camera_interval_valid(n)||strlen(path)>290)return 0;sprintf(fresh,"%s.new",path);sprintf(old,"%s.old",path);f=fopen(fresh,"wb");if(!f)return 0;ok=fprintf(f,"AMIHASSCAMERA1\n%u\n",n)>0;if(fclose(f))ok=0;if(!ok){remove(fresh);return 0;}f=fopen(path,"rb");if(f){fclose(f);if(remove(old)&&!ah_file_missing(old)){remove(fresh);return 0;}if(rename(path,old)){remove(fresh);return 0;}existed=1;}else if(!ah_file_missing(path)){remove(fresh);return 0;}if(rename(fresh,path)){if(existed)rename(old,path);remove(fresh);return 0;}remove(old);return 1;}
static int ah_camera_auto_due(unsigned int interval,unsigned long now,unsigned long last,int connected,int visible,int active,int busy)
{return interval&&ah_camera_interval_valid(interval)&&now&&connected&&visible&&active&&!busy&&(!last||(now>=last&&now-last>=interval));}
/* Last values remain visible, but never look current while offline/unavailable. */
static const char *ah_dashboard_availability(int result,int connected,int received,int available,unsigned long age,unsigned int refresh)
{if(!connected)return "Offline";if(result==8||!available)return "Unavailable";if(received&&(result!=1||ah_dashboard_stale(age,refresh)))return "Cached";if(!received)return result==2 ? "Updating":"No value";return "Available";}
static void ah_room_label(char out[33],const char *in){unsigned int i;for(i=0;i<32&&in[i];++i)out[i]=(unsigned char)in[i]<32||in[i]==127 ? ' ':in[i];out[i]=0;}
static unsigned int ah_dashboard_room_layout(struct AHDashboardLayout *out,const char *name,const struct AmiHASSEntity *items,unsigned int count)
{unsigned int i,j,n=0;ah_dashboard_defaults(out);memset(out->tiles,0,sizeof(out->tiles));ah_room_label(out->name,name[0] ? name:"Room");for(i=0;i<count&&n<AH_DASH_TILES;++i){if(!amihass_entity_id(items[i].id))continue;for(j=0;j<n;++j)if(!strcmp(out->tiles[j].entity,items[i].id))break;if(j<n)continue;strcpy(out->tiles[n].entity,items[i].id);ah_room_label(out->tiles[n].label,items[i].name[0] ? items[i].name:items[i].id);++n;}out->columns=n>12 ? 4:2;out->rows=n ? (n+out->columns-1)/out->columns:1;return n;}
#endif
