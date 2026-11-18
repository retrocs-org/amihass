#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "amihass_core.h"



static const char *state="{\"entity_id\":\"light.desk\",\"state\":\"on\",\"attributes\":{\"friendly_name\":\"Desk \\u00e5\",\"unit_of_measurement\":\"K\"},\"last_updated\":\"2026-11-17T12:00:00Z\"}";
int main(void)
{
 assert(amihass_api_response("{\"message\":\"API running.\"}"));
 assert(!amihass_api_response("{\"message\":\"unauthorized\"}"));
 assert(!amihass_api_response("{\"message\":\"API running.\",\"message\":\"API running.\"}"));
 assert(!amihass_api_response("{\"message\":\"API running.\"} trailing"));
 struct AmiHASSEntity e,items[2];struct AmiHASSConnection c;
 char json[1200],copy[1200],token[1025],path[96],body[180],request[1700];unsigned int i;int index=-1;
 assert(amihass_url("https://homeassistant.local:8123",0));assert(!amihass_url("http://homeassistant.local:8123",0));assert(amihass_url("http://127.0.0.1:8123",1));
 assert(!amihass_url("https://name:secret@home",1));assert(!amihass_url("https://home/api/",1));assert(!amihass_url("https://home:65536",1));assert(!amihass_url("https://home\nheader=x",1));assert(!amihass_url("https://home?token=x",1));
 assert(amihass_entity_id("light.my_light"));assert(!amihass_entity_id("light.a\"}"));assert(!amihass_entity_id("light.a/b"));assert(!amihass_entity_id("light."));assert(!amihass_entity_id("light.a.b"));
 assert(amihass_token("test-only-token-for-local-fixture\n",token));assert(!strcmp(token,"test-only-token-for-local-fixture"));
 assert(!amihass_token("bad\nAuthorization: example",token));assert(!amihass_token("not enough",token));
 assert(amihass_state(state,&e));assert(!strcmp(e.id,"light.desk") && !strcmp(e.state,"on"));assert(!strcmp(e.name,"Desk \345"));assert(amihass_available(&e));assert(!strcmp(amihass_icon(&e),"Bulb"));
 assert(amihass_matches(&e,"light","DESK"));assert(!amihass_matches(&e,"switch","desk"));assert(amihass_matches(&e,"All","desk"));
 sprintf(json,"[%s,%s]",state,state);assert(amihass_catalog(json,items,2)==2);assert(amihass_catalog(json,items,1)==-1);
 assert(amihass_catalog("[]",items,2)==0);assert(amihass_catalog("[1]",items,2)==-1);
 for(i=0;i<strlen(state);++i){memcpy(copy,state,i);copy[i]=0;assert(!amihass_state(copy,&e));}
 for(i=0;i<strlen(state);++i){strcpy(copy,state);copy[i]='\\';(void)amihass_state(copy,&e);}
 assert(!amihass_state("{\"entity_id\":\"light.desk\",\"entity_id\":\"light.other\",\"state\":\"on\"}",&e));
 assert(!amihass_state("{\"entity_id\":\"light.desk\",\"entity_\\u0069d\":\"light.other\",\"state\":\"on\"}",&e));
 assert(amihass_action("light.desk","Toggle",path,body));assert(!strcmp(path,"/api/services/homeassistant/toggle"));assert(!strcmp(body,"{\"entity_id\":\"light.desk\"}"));
 assert(amihass_action("scene.amiga","Activate",path,body));assert(!strcmp(path,"/api/services/scene/turn_on"));assert(amihass_action("scene.amiga","Toggle",path,body));
 assert(amihass_action("group.desk","Turn off",path,body));assert(!amihass_action("sensor.room","Toggle",path,body));assert(!amihass_action("lock.front","Turn on",path,body));assert(!amihass_action("cover.garage","Toggle",path,body));
 strcpy(c.url,"https://homeassistant.local:8123");c.allow_http=0;c.poll=30;
 assert(amihass_request(&c,"test-only-token-for-local-fixture",path,"group.desk",request,sizeof(request)));assert(strstr(request,"Authorization: Bearer test-only-token-for-local-fixture\"\n"));assert(strstr(request,"data = \"{\\\"entity_id\\\":\\\"group.desk\\\"}\"\n"));
 assert(!amihass_request(&c,"test-only-token-for-local-fixture","/api/states\nheader",NULL,request,sizeof(request)));assert(!amihass_request(&c,"test-only-token-for-local-fixture","/api/states/../../",NULL,request,sizeof(request)));
 assert(!amihass_request(&c,"test-only-token-for-local-fixture",path,"group.desk",request,10));
 {int total;char page[2400],payload[4096];char *args[]={"HASS","HASS_2","CONNECTION"};
  assert(amihass_areas_request(16,payload,sizeof(payload))&&strstr(payload,"areas()")&&strstr(payload,"area_name(a)")&&strstr(payload,"entries[16:32]"));
  assert(!amihass_areas_request(-1,payload,sizeof(payload))&&!amihass_areas_request(100001,payload,sizeof(payload))&&!amihass_areas_request(0,payload,12));
  assert(amihass_areas_page("{\"total\":2,\"items\":[{\"id\":\"living_room\",\"name\":\"Living room\"},{\"id\":\"study\",\"name\":\"Study\"}]}",items,2,&total)==2&&total==2&&!strcmp(items[1].id,"study"));
  assert(amihass_areas_page("{\"total\":0,\"items\":[]}",items,2,&total)==0&&!total);
  assert(amihass_areas_page("{\"total\":20,\"items\":[{\"id\":\"study\",\"name\":\"Study\"}]}",items,2,&total)==1&&total==20);
  assert(amihass_areas_page("{\"total\":1,\"items\":[{\"id\":\"\"}]}",items,2,&total)==-1);
  assert(amihass_areas_page("{\"total\":1,\"items\":[{\"id\":\"bad\\nroom\"}]}",items,2,&total)==-1);
  assert(amihass_areas_page("{\"total\":1,\"items\":[{\"id\":\"study\"}]}",items,0,&total)==-1);
  assert(amihass_areas_page("{\"total\":0,\"items\":[{\"id\":\"study\"}]}",items,2,&total)==-1);
  assert(amihass_areas_page("{\"total\":1,\"items\":[{\"id\":\"study\"}]} trailing",items,2,&total)==-1);
  sprintf(page,"{\"total\":1,\"items\":[{\"id\":\"%090d\"}]}",1);assert(amihass_areas_page(page,items,2,&total)==1&&strlen(items[0].name)==63);
  assert(amihass_device_id("0123456789abcdef0123456789abcdef"));assert(!amihass_device_id("bad"));
  assert(amihass_devices_domain_request("light","",0,payload,sizeof(payload))&&strstr(payload,"e.domain=='light'"));assert(!amihass_devices_domain_request("light' or true","",0,payload,sizeof(payload)));assert(amihass_devices_request("Desk's",0,payload,sizeof(payload)));assert(strstr(payload,"name_by_user")&&strstr(payload,"or e.name or e.entity_id"));
  assert(amihass_devices_page("{\"total\":1,\"items\":[{\"id\":\"0123456789abcdef0123456789abcdef\",\"name\":\"Desk\",\"state\":\"Maker\",\"model\":\"Lamp\"}]}",items,2,&total)==1&&total==1&&!strcmp(items[0].name,"Desk"));
  assert(amihass_devices_page("{\"total\":1,\"items\":[{\"id\":\"invalid\"}]}",items,2,&total)==-1);
  assert(amihass_device_page_request("0123456789abcdef0123456789abcdef","light","",0,payload,sizeof(payload))&&strstr(payload,"device_entities"));
  assert(!amihass_device_page_request("bad'","","",0,payload,sizeof(payload)));
  assert(amihass_page_request("All","",0,payload,sizeof(payload)));
  assert(!strstr(payload,"{%%") && !strstr(payload,"%%}"));
  assert(amihass_devices_request("",0,payload,sizeof(payload)));
  assert(!strstr(payload,"{%%") && !strstr(payload,"%%}"));
  assert(amihass_command(3,args)==3);
  assert(amihass_page_request("light","Desk's lamp",0,payload,sizeof(payload)));
  assert(strstr(payload,"/api/")==NULL);
  assert(amihass_page_request("All","\345",16,payload,sizeof(payload)));
  sprintf(page,"{\"total\":3040,\"items\":[%s]}",state);
  assert(amihass_page(page,items,2,&total)==1 && total==3040);
  assert(amihass_page_request("custom_type2","",0,payload,sizeof(payload)));
  assert(!amihass_page_request("light' or True","",0,payload,sizeof(payload)));
  assert(!amihass_page_request("light","",-1,payload,sizeof(payload)));
 }
 puts("AmiHASS core policies passed.");return 0;
}
