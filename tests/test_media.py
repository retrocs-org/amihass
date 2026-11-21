from pathlib import Path
import tempfile,subprocess,os
ROOT=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "amihass_core.h"
#include "sample.h"
int main(void){struct AHControls c,copy;struct AmiHASSEntity items[16];struct AHSample sample;struct AmiHASSMessage m;char path[96],body[180],updated[64];unsigned int n;
 amihass_controls("{\"entity_id\":\"media_player.radio\",\"state\":\"playing\",\"attributes\":{\"supported_features\":16437,\"volume_level\":0.35,\"media_title\":\"Amiga radio\",\"media_artist\":\"Demo artist\"}}",&c);assert(c.media_play&&c.media_pause&&c.media_previous&&c.media_next&&c.media_volume&&c.volume_known&&c.volume==35&&!strcmp(c.media_title,"Amiga radio"));n=amihass_control_items(&c,items,16);amihass_controls_items(items,n,&copy);assert(copy.media_play&&copy.media_previous&&copy.media_volume&&copy.volume==35&&!strcmp(copy.media_artist,"Demo artist"));
 assert(amihass_action("media_player.radio","Play",path,body)&&!strcmp(path,"/api/services/media_player/media_play"));assert(amihass_action("media_player.radio","Volume 43",path,body)&&!strcmp(body,"{\"entity_id\":\"media_player.radio\",\"volume_level\":0.43}"));assert(!amihass_action("media_player.radio","Volume 101",path,body));assert(!amihass_action("media_player.radio","Volume 2.5",path,body));assert(!amihass_action("light.desk","Play",path,body));assert(amihass_control_allowed(&c,"Next")&&amihass_control_matches(&c,"playing","Play")&&!amihass_control_matches(&c,"playing","Pause"));
 amihass_controls("{\"entity_id\":\"media_player.simple\",\"state\":\"idle\",\"attributes\":{\"volume_level\":0.4}}",&c);assert(!c.media_volume&&!amihass_control_allowed(&c,"Play")&&!amihass_control_allowed(&c,"Volume 40"));
 amihass_controls("{\"entity_id\":\"media_player.simple\",\"state\":\"idle\",\"attributes\":{\"supported_features\":9999999999999999999999999}}",&c);assert(!c.media_play&&!c.media_volume);
 ah_sample_init(&sample);memset(&m,0,sizeof(m));strcpy(m.owner,"MediaQA");strcpy(m.entity,"media_player.amihass_sample_living_room");m.kind=AMIHASS_ACTION;strcpy(m.action,"Pause");ah_sample_request(&sample,&m,100);assert(m.result==AMIHASS_WAIT&&!strcmp(m.item.state,"paused"));m.kind=AMIHASS_QUERY;ah_sample_request(&sample,&m,101);assert(m.result==AMIHASS_OK);amihass_controls_items(m.items,m.count,&c);assert(c.volume==35&&c.media_pause);
 m.kind=AMIHASS_ACTION;strcpy(m.action,"Volume 17");ah_sample_request(&sample,&m,102);assert(m.result==AMIHASS_WAIT);m.kind=AMIHASS_QUERY;ah_sample_request(&sample,&m,103);amihass_controls_items(m.items,m.count,&c);assert(c.volume==17&&amihass_control_matches(&c,m.item.state,"Volume 17"));m.kind=AMIHASS_ACTION;strcpy(m.action,"Next");ah_sample_request(&sample,&m,104);m.kind=AMIHASS_QUERY;ah_sample_request(&sample,&m,105);amihass_controls_items(m.items,m.count,&c);assert(!strcmp(c.media_title,"Next sample track"));strcpy(updated,m.item.updated);m.kind=AMIHASS_ACTION;strcpy(m.action,"Previous");ah_sample_request(&sample,&m,104);assert(strcmp(updated,m.item.updated));puts("Media: advertised capabilities, bounded metadata, service payloads, sample playback/volume and readback passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'exec').mkdir();(p/'exec/ports.h').write_text('typedef unsigned long ULONG;typedef long LONG;struct MsgPort;struct Message{unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'check.c').write_text(code);cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(ROOT/'SDK'),'-I'+str(ROOT/'include'),str(p/'check.c'),str(ROOT/'src/core.c'),str(ROOT/'src/sample.c'),str(ROOT/'src/browser_data.c'),'-o',str(p/'check')]
 if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
 subprocess.run(cmd,check=True);subprocess.run([str(p/'check')],check=True)
