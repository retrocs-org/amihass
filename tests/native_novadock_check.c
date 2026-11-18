/* Cross-project native client check. Only the shared sample source is allowed. */
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "nd_hass_bridge.h"
long __stack=65536;
static struct AmiHASSMessage *exchange(struct AmiHASSClient *c,int kind,const char *id)
{struct AmiHASSMessage *m;int i;if(!amihass_send(c,kind,"SDK.Interop",id,NULL,0,AMIHASS_QUERY_ONCE,NULL,NULL))return NULL;for(i=0;i<150;++i){m=amihass_reply(c);if(m)return m;Delay(2);}return NULL;}
int main(void)
{struct NDHassBridge tile;struct AmiHASSClient observer;struct AmiHASSMessage *m;struct AHControls c;const char *ids[]={"light.amihass_sample_living_room","light.amihass_sample_living_room","climate.amihass_sample_hallway"};const char *actions[]={"Brightness 37","Color #4080ff","Temperature 23"};int i,k,ok=0;memset(&tile,0,sizeof(tile));memset(&observer,0,sizeof(observer));
 m=exchange(&observer,AMIHASS_STATUS,NULL);if(!m)goto done;i=m->status.connected&&!strcmp(m->item.state,AMIHASS_SERVER_SAMPLE);FreeVec(m);if(!i){puts("FAIL: sample service required; no actions sent.");goto done;}
 for(k=0;k<3;++k){nd_hass_bridge_close(&tile);for(i=0;i<200;++i){nd_hass_bridge_poll(&tile,"HASS.Interop",ids[k],5);if(tile.result==AMIHASS_OK&&!tile.client.pending)break;Delay(2);}if(i==200)goto done;nd_hass_bridge_action(&tile,"HASS.Interop",ids[k],actions[k]);if(tile.result!=AMIHASS_WAIT)goto done;
 for(i=0;i<200;++i){nd_hass_bridge_poll(&tile,"HASS.Interop",ids[k],10);if(tile.result==AMIHASS_OK&&!tile.client.pending&&amihass_control_matches(&tile.controls,tile.entity.state,actions[k]))break;Delay(2);}if(i==200)goto done;
 m=exchange(&observer,AMIHASS_QUERY,ids[k]);if(!m)goto done;amihass_controls_items(m->items,m->count,&c);i=m->result==AMIHASS_OK&&amihass_control_matches(&c,m->item.state,actions[k]);FreeVec(m);if(!i)goto done;}
 puts("PASS: production NovaDock adapter controls shared sample brightness, RGB and climate; independent SDK client confirms all values.");ok=1;
done:nd_hass_bridge_close(&tile);amihass_client_close(&observer);return ok ? 0:10;}
