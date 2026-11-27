/* Bounded snapshot cache and binary ABI 1 chunks; no camera credentials/URLs. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "camera.h"
static unsigned long be32(const unsigned char *p){return ((unsigned long)p[0]<<24)|((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|p[3];}
static unsigned int be16(const unsigned char *p){return ((unsigned int)p[0]<<8)|p[1];}
static int dimensions(unsigned long w,unsigned long h,unsigned int *width,unsigned int *height)
{if(!w||!h||w>AH_CAMERA_WIDTH||h>AH_CAMERA_HEIGHT)return 0;*width=(unsigned int)w;*height=(unsigned int)h;return 1;}
int ah_camera_probe(const unsigned char *p,unsigned long n,unsigned int *w,unsigned int *h,char format[8])
{unsigned long i,len,end;if(!p||n<24||n>AH_CAMERA_LIMIT)return 0;
 if(!memcmp(p,"\211PNG\r\n\032\n",8)){if(n<33||be32(p+8)!=13||memcmp(p+12,"IHDR",4)||p[24]>8)return 0;strcpy(format,"PNG");return dimensions(be32(p+16),be32(p+20),w,h);}
 if(p[0]==255&&p[1]==216){i=2;while(i+4<=n){if(p[i++]!=255)return 0;while(i<n&&p[i]==255)++i;if(i>=n)return 0;{unsigned int marker=p[i++];if(marker==217||marker==218)return 0;if(marker==1||(marker>=208&&marker<=215))continue;if(i+2>n)return 0;len=be16(p+i);if(len<2||len>n-i)return 0;if((marker>=192&&marker<=195)||(marker>=197&&marker<=199)||(marker>=201&&marker<=203)||(marker>=205&&marker<=207)){if(len<8||p[i+2]!=8)return 0;strcpy(format,"JPEG");return dimensions(be16(p+i+5),be16(p+i+3),w,h);}i+=len;}}return 0;}
 if(!memcmp(p,"FORM",4)&&!memcmp(p+8,"ILBM",4)){end=be32(p+4);if(end>n-8)return 0;end+=8;for(i=12;i+8<=end;){len=be32(p+i+4);if(len>end-i-8)return 0;if(!memcmp(p+i,"BMHD",4)){if(len<20||!p[i+16]||p[i+16]>8||p[i+18]>1)return 0;strcpy(format,"ILBM");return dimensions(be16(p+i+8),be16(p+i+10),w,h);}i+=8+len+(len&1);}}return 0;}
void ah_camera_clear(struct AHCamera *c){if(c->data){memset(c->data,0,c->size);free(c->data);}memset(c,0,sizeof(*c));}
int ah_camera_path_size(const char *id,int small,char *out,unsigned int cap)
{if(!id||strncmp(id,"camera.",7)||!amihass_entity_id(id)||strlen(id)+48>=cap)return 0;sprintf(out,"/api/camera_proxy/%s?width=%s",id,small ? "96&height=60":"320&height=200");return 1;}
int ah_camera_path(const char *id,char *out,unsigned int cap){return ah_camera_path_size(id,0,out,cap);}
static void put32(unsigned char *p,unsigned long n){p[0]=(unsigned char)(n>>24);p[1]=(unsigned char)(n>>16);p[2]=(unsigned char)(n>>8);p[3]=(unsigned char)n;}
int ah_camera_sample(struct AHCamera *c)
{unsigned char *p,*body;unsigned int x,y,plane,bit;unsigned long size=3908,at;/* 160x96, two planar bitplanes, four colours. */
 p=calloc(1,size);if(!p)return 0;memcpy(p,"FORM",4);put32(p+4,size-8);memcpy(p+8,"ILBMBMHD",8);put32(p+16,20);p[20]=0;p[21]=160;p[22]=0;p[23]=96;p[28]=2;p[34]=p[35]=1;p[36]=0;p[37]=160;p[38]=0;p[39]=96;memcpy(p+40,"CMAP",4);put32(p+44,12);p[48]=20;p[49]=25;p[50]=35;p[51]=60;p[52]=100;p[53]=140;p[54]=140;p[55]=180;p[56]=130;p[57]=230;p[58]=225;p[59]=190;memcpy(p+60,"BODY",4);put32(p+64,3840);body=p+68;
 for(y=0;y<96;++y)for(x=0;x<160;++x){unsigned int color=y<55?1:2;if((x>44&&x<116&&y>38&&y<80)||(y>20&&y<=38&&x>80-(y-20)*2&&x<80+(y-20)*2))color=3;if(x>70&&x<89&&y>55&&y<80)color=0;if((x<5||x>154||y<5||y>90))color=0;for(plane=0;plane<2;++plane){at=(unsigned long)y*40+plane*20+x/8;bit=7-x%8;if(color&(1<<plane))body[at]|=(unsigned char)(1<<bit);}}
 if(c->data){memset(c->data,0,c->size);free(c->data);}c->data=p;c->size=size;c->width=160;c->height=96;strcpy(c->format,"ILBM");return 1;}
int ah_camera_reply(struct AHCamera *c,struct AmiHASSMessage *m,unsigned long now)
{unsigned long n,offset;if(strncmp(m->entity,"camera.",7)||!amihass_entity_id(m->entity)||m->offset<0||m->offset>(long)AH_CAMERA_LIMIT){m->result=AMIHASS_UNSUPPORTED;return 1;}if(strcmp(c->entity,m->entity)){m->result=AMIHASS_BUSY;return 1;}if(c->pending){m->result=AMIHASS_WAIT;return 1;}if(c->result!=AMIHASS_OK||!c->data){m->result=c->result?c->result:AMIHASS_FAILED;return 1;}if((m->offset||m->action[0])&&strcmp(m->action,c->version)){m->result=AMIHASS_STALE;return 1;}offset=(unsigned long)m->offset;if(offset>c->size){m->result=AMIHASS_FAILED;return 1;}n=c->size-offset;if(n>AMIHASS_SNAPSHOT_BYTES)n=AMIHASS_SNAPSHOT_BYTES;memset(&m->item,0,sizeof(m->item));strcpy(m->item.id,c->entity);strcpy(m->item.updated,c->version);strcpy(m->item.state,c->format);sprintf(m->item.name,"%u x %u",c->width,c->height);memset(m->items,0,sizeof(m->items));memcpy(m->items,c->data+offset,n);m->count=0;m->refresh=(int)n;m->total=(long)c->size;m->age=now>=c->updated?now-c->updated:0;m->result=AMIHASS_OK;c->touched=now;return 1;}
