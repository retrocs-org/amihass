/* Four small, manually refreshed previews; one bounded image transfer at a time. */
#define DataTypesBase AHThumbDataTypesBase
#define GfxBase AHThumbGfxBase
#define CyberGfxBase AHThumbCyberGfxBase
#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/datatypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ah_locale.h"
#include "camera.h"
#include "thumb_cybergraphics.h"
#include "camera_thumbs.h"
struct Library *CyberGfxBase=NULL;struct Library *DataTypesBase=NULL;struct GfxBase *GfxBase=NULL;
struct Thumb {Object *object;struct BitMap *bitmap;struct ColorMap *map;long pens[16];unsigned int pen_count;char file[96];int tile;time_t received;unsigned int width,height;};
struct AHCameraThumbs {struct Screen *screen;struct AmiHASSClient client;struct Thumb thumbs[4];int slots,slot,tile,loading,fresh;unsigned long total,offset,ticks,next;unsigned char *data;char entity[96],version[20],status[96];time_t started;};
static void dispose(struct Thumb *t){unsigned int i;if(t->bitmap)FreeBitMap(t->bitmap);if(t->map)for(i=0;i<t->pen_count;++i)if(t->pens[i]>=0)ReleasePen(t->map,(ULONG)t->pens[i]);if(t->object)DisposeDTObject(t->object);if(t->file[0])DeleteFile(t->file);memset(t,0,sizeof(*t));t->tile=-1;}
struct AHCameraThumbs *ah_thumbs_new(struct Screen *screen)
{struct AHCameraThumbs *t;int i;t=calloc(1,sizeof(*t));if(!t)return NULL;DataTypesBase=OpenLibrary("datatypes.library",39);GfxBase=(struct GfxBase*)OpenLibrary("graphics.library",39);if(!DataTypesBase||!GfxBase){if(DataTypesBase)CloseLibrary(DataTypesBase);if(GfxBase)CloseLibrary((struct Library*)GfxBase);free(t);DataTypesBase=NULL;GfxBase=NULL;return NULL;}t->slots=4;t->screen=screen;for(i=0;i<4;++i)t->thumbs[i].tile=-1;return t;}
void ah_thumbs_limit(struct AHCameraThumbs *t,int low){int i;if(!t)return;t->slots=low?1:4;for(i=t->slots;i<4;++i)dispose(&t->thumbs[i]);}
void ah_thumbs_free(struct AHCameraThumbs *t)
{int i;if(!t)return;amihass_client_close(&t->client);if(t->data){memset(t->data,0,t->total);free(t->data);}for(i=0;i<4;++i)dispose(&t->thumbs[i]);if(DataTypesBase)CloseLibrary(DataTypesBase);if(GfxBase)CloseLibrary((struct Library*)GfxBase);DataTypesBase=NULL;GfxBase=NULL;free(t);}
int ah_thumbs_request(struct AHCameraThumbs *t,int tile,const char *id)
{int i,slot=0;if(!t||t->loading||!id||strncmp(id,"camera.",7)||!amihass_entity_id(id))return 0;for(i=0;i<t->slots;++i){if(t->thumbs[i].tile==tile){slot=i;break;}if(t->thumbs[i].tile<0){slot=i;break;}if(t->thumbs[i].received<t->thumbs[slot].received)slot=i;}t->slot=slot;t->loading=t->fresh=1;t->offset=t->total=0;t->next=t->ticks;t->version[0]=0;t->started=time(NULL);strcpy(t->entity,id);/* Keep the previous preview until the replacement has decoded. */t->tile=tile;strcpy(t->status,"Fetching camera preview...");return 1;}
static int decode(struct AHCameraThumbs *t)
{struct Thumb next,*old=&t->thumbs[t->slot];Object *object=NULL;struct BitMapHeader *header=NULL;struct BitMap *source=NULL;struct ColorRegister *palette=NULL;struct RastPort src,dst;unsigned int width,height,x,y,sx,sy,i,index;char format[8];FILE *f;int ok;unsigned long dw,dh,before,after,colors=0;unsigned char pixel[3];long value;struct AmiHASSClient stats;struct AmiHASSMessage *m;
 memset(&next,0,sizeof(next));if(!ah_camera_probe(t->data,t->total,&width,&height,format))return 0;sprintf(next.file,"RAM:AmiHASS-Thumb-%08lx-%lu",(ULONG)t,t->ticks);f=fopen(next.file,"wb");if(!f)return 0;ok=fwrite(t->data,1,t->total,f)==t->total;if(fclose(f))ok=0;if(!ok){DeleteFile(next.file);return 0;}
 before=AvailMem(MEMF_FAST);object=NewDTObject(next.file,DTA_SourceType,DTST_FILE,DTA_GroupID,GID_PICTURE,PDTA_Remap,FALSE,TAG_DONE);if(!object)goto failed;GetDTAttrs(object,PDTA_BitMapHeader,(ULONG)&header,TAG_DONE);if(!header||header->bmh_Width!=width||header->bmh_Height!=height||!DoDTMethod(object,NULL,NULL,DTM_PROCLAYOUT,NULL,TRUE))goto failed;GetDTAttrs(object,PDTA_BitMap,(ULONG)&source,PDTA_ColorRegisters,(ULONG)&palette,PDTA_NumColors,(ULONG)&colors,TAG_DONE);if(!source)goto failed;if(source->Depth<=8&&(!palette||!colors||colors>256))goto failed;if(source->Depth>8){CyberGfxBase=OpenLibrary("cybergraphics.library",41);if(!CyberGfxBase||!nd_cgx_bitmap(source))goto failed;}
 dw=48;dh=(unsigned long)height*48/width;if(dh>32){dh=32;dw=(unsigned long)width*32/height;}if(!dw)dw=1;if(!dh)dh=1;next.width=dw;next.height=dh;next.bitmap=AllocBitMap(dw,dh,t->screen->RastPort.BitMap->Depth,BMF_CLEAR,t->screen->RastPort.BitMap);if(!next.bitmap)goto failed;next.map=t->screen->ViewPort.ColorMap;for(i=0;i<16;++i){unsigned long red=(i&8)?255:0,green=((i>>1)&3)*85,blue=(i&1)?255:0;next.pens[i]=ObtainBestPen(next.map,red*0x01010101UL,green*0x01010101UL,blue*0x01010101UL,OBP_Precision,PRECISION_IMAGE,OBP_FailIfBad,FALSE,TAG_DONE);++next.pen_count;}
 InitRastPort(&src);src.BitMap=source;InitRastPort(&dst);dst.BitMap=next.bitmap;for(y=0;y<dh;++y){sy=(unsigned int)((unsigned long)y*height/dh);for(x=0;x<dw;++x){sx=(unsigned int)((unsigned long)x*width/dw);if(source->Depth>8){if(nd_cgx_rgb_pixel(pixel,&src,sx,sy)!=1)goto failed;}else{value=ReadPixel(&src,sx,sy);if(value<0||(unsigned long)value>=colors)goto failed;pixel[0]=palette[value].red;pixel[1]=palette[value].green;pixel[2]=palette[value].blue;}index=(pixel[0]>=128?8:0)|((pixel[1]>>6)<<1)|(pixel[2]>=128?1:0);SetAPen(&dst,next.pens[index]>=0?(ULONG)next.pens[index]:0);WritePixel(&dst,x,y);}}
 after=AvailMem(MEMF_FAST);DisposeDTObject(object);object=NULL;if(CyberGfxBase)CloseLibrary(CyberGfxBase);CyberGfxBase=NULL;DeleteFile(next.file);next.file[0]=0;next.tile=t->tile;next.received=time(NULL);dispose(old);*old=next;
 memset(&stats,0,sizeof(stats));if(amihass_send(&stats,AMIHASS_CAMERA_STATS,"AmiHASS.Thumbnail",NULL,NULL,(int)t->total,(int)(before>after?before-after:0),NULL,NULL)){for(i=0;i<30;++i){m=amihass_reply(&stats);if(m){FreeVec(m);break;}Delay(1);}}amihass_client_close(&stats);return 1;
failed:if(object)DisposeDTObject(object);if(CyberGfxBase)CloseLibrary(CyberGfxBase);CyberGfxBase=NULL;dispose(&next);return 0;}
int ah_thumbs_busy(struct AHCameraThumbs *t){return t&&t->loading;}
int ah_thumbs_step(struct AHCameraThumbs *t)
{struct AmiHASSMessage *m;unsigned long n;const unsigned char *bytes;int changed=0;if(!t||!t->loading)return 0;++t->ticks;if(time(NULL)<t->started||time(NULL)-t->started>45){t->loading=0;amihass_client_close(&t->client);strcpy(t->status,"Camera preview timed out");changed=1;goto finished;}if(t->client.pending){m=amihass_reply(&t->client);if(!m){if(!t->client.pending){t->loading=0;strcpy(t->status,"Bridge timed out");changed=1;goto finished;}return 0;}if(m->result==AMIHASS_WAIT||m->result==AMIHASS_BUSY)t->next=t->ticks+5;else if(!(bytes=amihass_snapshot_data(m,&n))||m->offset!=(long)t->offset||(t->offset&&(t->total!=(unsigned long)m->total||strcmp(t->version,m->item.updated)))){t->loading=0;strcpy(t->status,"Camera preview unavailable; Refresh to retry");changed=1;}else{if(!t->offset){t->total=(unsigned long)m->total;strncpy(t->version,m->item.updated,19);t->version[19]=0;t->data=malloc(t->total);if(!t->data){t->loading=0;strcpy(t->status,"Not enough memory for preview");}}if(t->loading){memcpy(t->data+t->offset,bytes,n);t->offset+=n;if(t->offset==t->total){t->loading=0;if(decode(t))strcpy(t->status,"Camera preview updated");else strcpy(t->status,"Camera preview could not decode; check picture datatypes");changed=1;}}}FreeVec(m);}else if(t->ticks>=t->next){if(!amihass_snapshot_size(&t->client,"AmiHASS.Thumbnail",t->entity,(long)t->offset,t->offset?t->version:NULL,t->fresh,1)){t->loading=0;strcpy(t->status,"Start the bridge to refresh camera preview");changed=1;}else t->fresh=0;}
finished:
 if(!t->loading){if(t->data){memset(t->data,0,t->total);free(t->data);t->data=NULL;}amihass_client_close(&t->client);}return changed;}
int ah_thumbs_draw(struct AHCameraThumbs *t,int tile,struct RastPort *rp,int x,int y)
{int i;if(!t)return 0;for(i=0;i<4;++i)if(t->thumbs[i].tile==tile&&t->thumbs[i].bitmap){BltBitMapRastPort(t->thumbs[i].bitmap,0,0,rp,x,y,t->thumbs[i].width,t->thumbs[i].height,0xc0);return 1;}return 0;}
unsigned long ah_thumbs_age(struct AHCameraThumbs *t,int tile)
{int i;if(t)for(i=0;i<4;++i)if(t->thumbs[i].tile==tile&&t->thumbs[i].received)return time(NULL)>=t->thumbs[i].received?(unsigned long)(time(NULL)-t->thumbs[i].received):0;return 0;}
const char *ah_thumbs_status(struct AHCameraThumbs *t){return t?t->status:"Picture datatypes unavailable";}
