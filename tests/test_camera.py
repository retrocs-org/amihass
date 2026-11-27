"""Bounded image headers, binary chunks and camera URL policy; no network."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "camera.h"
'''
sdk=(root/'SDK/amihass_client.c').read_text();a=sdk.index('const unsigned char *amihass_snapshot_data(');code+=sdk[a:]
code+=r'''
int main(void){struct AHCamera c;struct AmiHASSMessage m;struct AmiHASSConnection connection;unsigned int w,h;unsigned long n;char format[8],path[180],config[2000];unsigned char png[33]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,1,64,0,0,0,200,8};memset(&c,0,sizeof(c));assert(ah_camera_sample(&c)&&c.size==3908&&ah_camera_probe(c.data,c.size,&w,&h,format)&&w==160&&h==96&&!strcmp(format,"ILBM"));assert(ah_camera_probe(png,sizeof(png),&w,&h,format)&&w==320&&h==200);png[18]=255;assert(!ah_camera_probe(png,sizeof(png),&w,&h,format));{unsigned char bad[32];unsigned int z;memset(bad,255,sizeof(bad));bad[1]=216;bad[31]=192;assert(!ah_camera_probe(bad,sizeof(bad),&w,&h,format));for(z=0;z<32;++z){bad[z]=(unsigned char)z;assert(!ah_camera_probe(bad,z,&w,&h,format));}}assert(!ah_camera_probe(c.data,AH_CAMERA_LIMIT+1,&w,&h,format)&&!ah_camera_probe(c.data,10,&w,&h,format));assert(ah_camera_path("camera.test",path,sizeof(path))&&!strcmp(path,"/api/camera_proxy/camera.test?width=320&height=200"));assert(!ah_camera_path("camera.test?token=secret",path,sizeof(path))&&!ah_camera_path("light.test",path,sizeof(path)));
 memset(&connection,0,sizeof(connection));strcpy(connection.url,"https://fixture.invalid");connection.poll=30;assert(ah_camera_path("camera.test",path,sizeof(path))&&amihass_request(&connection,"amihass-fixture-token-for-testing",path,NULL,config,sizeof(config))&&strstr(config,"camera_proxy/camera.test?width=320&height=200"));assert(!amihass_request(&connection,"amihass-fixture-token-for-testing","/api/camera_proxy/camera.test?token=secret",NULL,config,sizeof(config)));assert(ah_camera_path_size("camera.test",1,path,sizeof(path))&&amihass_request(&connection,"amihass-fixture-token-for-testing",path,NULL,config,sizeof(config))&&strstr(config,"camera.test?width=96&height=60"));assert(!amihass_request(&connection,"amihass-fixture-token-for-testing","/api/camera_proxy/camera.test?width=4000&height=4000",NULL,config,sizeof(config)));assert(!amihass_request(&connection,"amihass-fixture-token-for-testing","/api/camera_proxy_stream/camera.test",NULL,config,sizeof(config)));
 strcpy(c.entity,"camera.amihass_sample_driveway");strcpy(c.version,"00000001-00000001");c.result=AMIHASS_OK;c.updated=100;memset(&m,0,sizeof(m));m.magic=AMIHASS_MAGIC;m.abi=AMIHASS_ABI;m.kind=AMIHASS_SNAPSHOT;strcpy(m.entity,c.entity);assert(ah_camera_reply(&c,&m,102)&&m.age==2&&m.refresh==2048&&m.count==0&&m.total==3908);assert(amihass_snapshot_data(&m,&n)&&n==2048&&!memcmp(m.items,c.data,n));m.offset=2048;strcpy(m.action,c.version);assert(ah_camera_reply(&c,&m,103)&&m.refresh==1860&&amihass_snapshot_data(&m,&n)&&!memcmp(m.items,c.data+2048,n));strcpy(m.action,"stale-version");ah_camera_reply(&c,&m,104);assert(m.result==AMIHASS_STALE);m.offset=-1;ah_camera_reply(&c,&m,104);assert(m.result==AMIHASS_UNSUPPORTED);m.offset=0;m.action[0]=0;c.pending=1;ah_camera_reply(&c,&m,104);assert(m.result==AMIHASS_WAIT);m.result=AMIHASS_OK;m.refresh=99999;assert(!amihass_snapshot_data(&m,&n));ah_camera_clear(&c);assert(!c.data&&!c.size);puts("Camera: bounded ILBM/PNG headers, dimensions, strict authenticated image path, versioned binary chunks and invalid replies passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir();(p/'exec/ports.h').write_text('typedef unsigned long ULONG;typedef long LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'test.c').write_text(code)
    cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),str(root/'src/camera.c'),str(root/'src/core.c'),'-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
