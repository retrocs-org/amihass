#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "amihass_core.h"



static const char *state="{\"entity_id\":\"light.desk\",\"state\":\"on\",\"attributes\":{\"friendly_name\":\"Desk \\u00e5\",\"unit_of_measurement\":\"K\"},\"last_updated\":\"2026-11-17T12:00:00Z\"}";
int main(void)
{
 {struct AmiHASSEntity e,rooms[16];char request[8192];int total;assert(amihass_state("{\"entity_id\":\"sensor.name\",\"state\":\"1\",\"attributes\":{\"friendly_name\":\"Räksmörgås \\u20ac \\u2014 \\ud83d\\udd12\"}}",&e));assert(strstr(e.name,"EUR - ?"));assert(amihass_areas_page("{\"total\":1,\"items\":[{\"id\":\"räum\",\"name\":\"Räum\"}]}",rooms,16,&total)==1);assert(!strcmp(rooms[0].id,"räum"));assert(amihass_device_scope_page_request("",rooms[0].id,"light's", "light","",0,request,sizeof(request)));assert(strstr(request,"in_label")&&strstr(request,"label_entities"));assert(!amihass_devices_scope_request("","bad\nlabel","","",0,request,sizeof(request)));}

 assert(amihass_api_response("{\"message\":\"API running.\"}"));
 assert(!amihass_api_response("{\"message\":\"unauthorized\"}"));
 assert(!amihass_api_response("{\"message\":\"API running.\",\"message\":\"API running.\"}"));
 assert(!amihass_api_response("{\"message\":\"API running.\"} trailing"));
 struct AmiHASSEntity e,items[2];struct AmiHASSConnection c;
 char json[1200],copy[1200],token[1025],path[96],body[180],request[1700];unsigned int i;int index=-1;
 assert(amihass_url("https://homeassistant.local:8123",0));assert(!amihass_url("http://homeassistant.local:8123",0));assert(amihass_url("http://127.0.0.1:8123",1));
 assert(!amihass_url("https://name:secret@home",1));assert(!amihass_url("https://home/api/",1));assert(!amihass_url("https://home:65536",1));assert(!amihass_url("https://home\nheader=x",1));assert(!amihass_url("https://home?token=x",1));
 /* Domains added in 0.3: service mapping, capability gating and readback. */
 {char path[96],body[180];struct AHControls c;
  assert(amihass_action("climate.lr","HVAC heat",path,body)&&!strcmp(path,"/api/services/climate/set_hvac_mode")&&strstr(body,"\"hvac_mode\":\"heat\""));assert(!amihass_action("climate.lr","HVAC warp",path,body));
  assert(amihass_action("climate.lr","Turn off",path,body)&&!strcmp(path,"/api/services/climate/turn_off"));assert(!amihass_action("climate.lr","Toggle",path,body));
  assert(amihass_action("automation.night","Trigger",path,body)&&!strcmp(path,"/api/services/automation/trigger"));assert(amihass_action("automation.night","Turn off",path,body)&&!strcmp(path,"/api/services/homeassistant/turn_off"));
  assert(amihass_action("input_select.mode","Next option",path,body)&&!strcmp(path,"/api/services/input_select/select_next"));assert(amihass_action("select.mode","Option Eco",path,body)&&!strcmp(path,"/api/services/select/select_option")&&strstr(body,"\"option\":\"Eco\""));assert(!amihass_action("select.mode","Option a\"b",path,body));
  assert(amihass_action("input_number.level","Value 21.5",path,body)&&strstr(body,"\"value\":21.50"));assert(amihass_action("number.level","Value -3",path,body));
  assert(amihass_action("vacuum.robo","Return home",path,body)&&!strcmp(path,"/api/services/vacuum/return_to_base"));assert(!amihass_action("vacuum.robo","Pause",path,body));
  assert(amihass_action("valve.main","Close valve",path,body)&&!strcmp(path,"/api/services/valve/close_valve"));assert(amihass_action("valve.main","Position 40",path,body)&&!strcmp(path,"/api/services/valve/set_valve_position"));
  assert(amihass_action("humidifier.h","Humidity 45",path,body)&&strstr(body,"\"humidity\":45"));assert(amihass_action("siren.s","Toggle",path,body));
  assert(amihass_action("alarm_control_panel.home","Arm away",path,body)&&!strcmp(path,"/api/services/alarm_control_panel/alarm_arm_away"));assert(amihass_action_sensitive("alarm_control_panel.home","Disarm")&&!amihass_action_sensitive("alarm_control_panel.home","Arm away"));
  assert(amihass_action("scene.evening","Run",path,body)&&amihass_action("script.x","Confirm and run",path,body)&&!strcmp(path,"/api/services/script/turn_on"));
  amihass_controls("{\"entity_id\":\"climate.lr\",\"state\":\"heat\",\"attributes\":{\"hvac_modes\":[\"off\",\"heat\"]}}",&c);assert(amihass_control_allowed(&c,"HVAC heat")&&amihass_control_allowed(&c,"HVAC off")&&!amihass_control_allowed(&c,"HVAC cool")&&amihass_control_matches(&c,"heat","HVAC heat"));
  {struct AmiHASSEntity items[16];struct AHControls back;unsigned int n=amihass_control_items(&c,items,16);amihass_controls_items(items,n,&back);assert(back.hvac_modes==c.hvac_modes&&back.hvac_modes==3);}
  amihass_controls("{\"entity_id\":\"alarm_control_panel.home\",\"state\":\"disarmed\",\"attributes\":{\"code_format\":null}}",&c);assert(c.alarm&&amihass_control_allowed(&c,"Arm home")&&amihass_control_matches(&c,"armed_home","Arm home"));
  amihass_controls("{\"entity_id\":\"alarm_control_panel.home\",\"state\":\"disarmed\",\"attributes\":{\"code_format\":\"number\"}}",&c);assert(c.alarm==2&&amihass_control_allowed(&c,"Disarm"));}
 /* Reverse-proxy prefixes are accepted; API paths, empty or dot segments are not. */
 assert(amihass_url("https://example.org/ha",0));assert(amihass_url("https://example.org:8443/home/assistant",0));assert(!amihass_url("https://example.org/ha/",0));assert(!amihass_url("https://example.org//ha",0));assert(!amihass_url("https://example.org/../ha",0));assert(!amihass_url("https://example.org/ha/api",0));assert(!amihass_url("https://example.org/h a",0));
 {char u[160];strcpy(u,"  http://ha.local:8123///  ");amihass_url_clean(u);assert(!strcmp(u,"http://ha.local:8123")&&amihass_url(u,1));strcpy(u,"https://ha/x/");amihass_url_clean(u);assert(!strcmp(u,"https://ha/x"));strcpy(u,"https://");amihass_url_clean(u);assert(!strcmp(u,"https://"));}
 /* Long states and names truncate on whole glyphs; the entity stays usable. */
 {struct AmiHASSEntity t;assert(amihass_state("{\"entity_id\":\"sensor.note\",\"state\":\"0123456789012345678901234567890123456789ABC\",\"attributes\":{\"friendly_name\":\"\\u00e4\"}}",&t)&&strlen(t.state)==39&&!strncmp(t.state,"012345678901234567890123456789012345678",39));}

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
  assert(amihass_devices_domain_request("light","",0,payload,sizeof(payload))&&strstr(payload,"states['light']")&&strstr(payload,"|unique"));assert(!amihass_devices_domain_request("light' or true","",0,payload,sizeof(payload)));assert(amihass_devices_request("Desk's",0,payload,sizeof(payload)));assert(strstr(payload,"name_by_user")&&strstr(payload,"first.name or first.entity_id"));
  assert(amihass_devices_page("{\"total\":1,\"items\":[{\"id\":\"0123456789abcdef0123456789abcdef\",\"name\":\"Desk\",\"state\":\"Maker\",\"model\":\"Lamp\"}]}",items,2,&total)==1&&total==1&&!strcmp(items[0].name,"Desk"));
  assert(amihass_devices_page("{\"total\":1,\"items\":[{\"id\":\"invalid\"}]}",items,2,&total)==-1);
  assert(amihass_device_page_request("0123456789abcdef0123456789abcdef","light","",0,payload,sizeof(payload))&&strstr(payload,"device_entities"));
  assert(!amihass_device_page_request("bad'","","",0,payload,sizeof(payload)));
  assert(amihass_page_request("All","",0,payload,sizeof(payload))&&strstr(payload,"selectattr")&&!strstr(payload,"for e in states"));
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
 {/* Full attributes, history window and history thinning. */
  struct AmiHASSEntity a[64];int t,n,i;char hp[220],st[24],big[20000],tb[200],rq[2048];
  n=amihass_attributes_page("{\"entity_id\":\"sensor.x\",\"state\":\"1\",\"attributes\":{\"friendly_name\":\"Hall \\u00e5\",\"options\":[\"a\", \"b\"],\"on\":true,\"n\":null,\"v\":-2.5,\"o\":{\"k\": \"v w\"}}}",a,64,&t);
  assert(n==6&&t==6&&!strcmp(a[0].id,"friendly_name")&&!strcmp(a[0].name,"Hall \345")&&!strcmp(a[0].device_class,"string"));
  assert(!strcmp(a[1].name,"[\"a\",\"b\"]")&&!strcmp(a[1].device_class,"list")&&!strcmp(a[2].device_class,"boolean")&&!strcmp(a[3].device_class,"null")&&!strcmp(a[4].name,"-2.5")&&!strcmp(a[5].name,"{\"k\":\"v w\"}"));
  assert(amihass_attributes_page("{\"attributes\":{\"a\":1,\"b\":2,\"c\":3}}",a,2,&t)==2&&t==3);
  assert(amihass_attributes_page("{\"attributes\":{\"a\":1,}}",a,2,&t)<0&&amihass_attributes_page("{\"state\":\"1\"}",a,2,&t)<0);
  assert(amihass_history_start_request(6,tb,sizeof(tb))&&strstr(tb,"hours=6")&&!amihass_history_start_request(0,tb,sizeof(tb))&&!amihass_history_start_request(49,tb,sizeof(tb)));
  assert(amihass_history_start(" 2026-11-24T08:00:05Z\n",st)&&!strcmp(st,"2026-11-24T08:00:05Z")&&!amihass_history_start("2026-11-24T08:00:05+00:00",st)&&!amihass_history_start("x",st));
  assert(amihass_history_path("sensor.hall",st,hp,sizeof(hp))&&!strcmp(hp,"/api/history/period/2026-11-24T08:00:05Z?filter_entity_id=sensor.hall&minimal_response&no_attributes"));
  assert(amihass_request(&c,"test-only-token-for-local-fixture",hp,NULL,rq,sizeof(rq)));
  assert(amihass_history_path("sensor.hall","",hp,sizeof(hp))&&amihass_request(&c,"test-only-token-for-local-fixture",hp,NULL,rq,sizeof(rq)));
  assert(!amihass_request(&c,"test-only-token-for-local-fixture","/api/history/period?filter_entity_id=sensor.hall&minimal_response",NULL,rq,sizeof(rq)));
  assert(!amihass_request(&c,"test-only-token-for-local-fixture","/api/history/period/2026-11-24T08:00:05Z?filter_entity_id=sensor.hall&minimal_response&no_attributes&x=1",NULL,rq,sizeof(rq)));
  assert(!amihass_history_path("sensor.x\"",NULL,hp,sizeof(hp)));
  strcpy(big,"[[{\"entity_id\":\"sensor.t\",\"state\":\"unavailable\",\"last_changed\":\"2026-11-24T00:00:00+00:00\"}");
  for(i=0;i<200;++i)sprintf(big+strlen(big),",{\"state\":\"%d.5\",\"last_changed\":\"2026-11-24T%02d:%02d:00+00:00\"}",i,i/60,i%60);strcat(big,"]]");
  n=amihass_history_points(big,(unsigned long)strlen(big),a,64);assert(n==64&&!a[0].state[0]&&!strcmp(a[63].state,"199.5")&&!strcmp(a[0].updated,"2026-11-24T00:00:00+00:00"));/* the leading unavailable state is a gap */
  for(i=2;i<64;++i)assert(atof(a[i].state)>atof(a[i-1].state));
  assert(amihass_history_points("[]",2,a,64)==0&&amihass_history_points("[[{\"state\":\"12345.25\"},{\"state\":\"unavailable\"}]]",48,a,64)==2&&!strcmp(a[0].state,"12345.25")&&!a[1].state[0]);/* gaps kept, empty */
  {char bounds[13][24],body[400],rp[240];assert(amihass_history_bounds_request(24,4,body,sizeof(body))&&strstr(body,"range(5)")&&strstr(body,"21600*i")&&!amihass_history_bounds_request(24,13,body,sizeof(body)));
   assert(amihass_history_bounds("2026-11-24T00:00:00Z 2026-11-24T06:00:00Z 2026-11-24T12:00:00Z \n",bounds,3)&&!strcmp(bounds[2],"2026-11-24T12:00:00Z"));
   assert(!amihass_history_bounds("2026-11-24T06:00:00Z 2026-11-24T00:00:00Z",bounds,2)&&!amihass_history_bounds("2026-11-24T00:00:00Z",bounds,2)&&!amihass_history_bounds("2026-11-24T00:00:00Z 2026-11-24T06:00:00Z x",bounds,2));
   assert(amihass_history_range_path("sensor.p","2026-11-24T00:00:00Z","2026-11-24T06:00:00Z",rp,sizeof(rp))&&!strcmp(rp,"/api/history/period/2026-11-24T00:00:00Z?filter_entity_id=sensor.p&end_time=2026-11-24T06:00:00Z&minimal_response&no_attributes")&&amihass_request(&c,"test-only-token-for-local-fixture",rp,NULL,rq,sizeof(rq)));
   assert(!amihass_history_range_path("sensor.p","","2026-11-24T06:00:00Z",rp,sizeof(rp))&&!amihass_request(&c,"test-only-token-for-local-fixture","/api/history/period/2026-11-24T00:00:00Z?filter_entity_id=sensor.p&end_time=2026-11-24T06:00:00X&minimal_response&no_attributes",NULL,rq,sizeof(rq)));}
  assert(amihass_history_points("[[{\"state\":\"1\"}",15,a,64)<0);
 }
 {/* Alarm panels that need a code: capability 2, code in the body only. */
  struct AHControls k;char rq2[4096];struct AmiHASSEntity ci[24];int ncap;
  amihass_controls("{\"entity_id\":\"alarm_control_panel.home\",\"state\":\"armed_away\",\"attributes\":{\"code_format\":\"number\"}}",&k);assert(k.alarm==2&&amihass_control_allowed(&k,"Disarm"));
  amihass_controls("{\"entity_id\":\"alarm_control_panel.home\",\"state\":\"disarmed\",\"attributes\":{\"code_format\":null}}",&k);assert(k.alarm==1);
  k.alarm=2;ncap=amihass_control_items(&k,ci,24);{int q,f=0;struct AHControls back;memset(&back,0,sizeof(back));for(q=0;q<ncap;++q)if(!strcmp(ci[q].id,"control.alarm")&&!strcmp(ci[q].state,"code"))f=1;assert(f);}
  assert(amihass_alarm_code("1234")&&amihass_alarm_code("abC9")&&!amihass_alarm_code("")&&!amihass_alarm_code("12 34")&&!amihass_alarm_code("12\"34")&&!amihass_alarm_code("12345678901234567"));
  assert(amihass_control_request_code(&c,"test-only-token-for-local-fixture","alarm_control_panel.home","Disarm","4711",rq2,sizeof(rq2))&&strstr(rq2,"\\\"code\\\": \\\"4711\\\"")==NULL&&strstr(rq2,"4711")&&strstr(rq2,"alarm_disarm"));
  assert(!amihass_control_request_code(&c,"test-only-token-for-local-fixture","light.desk","Toggle","4711",rq2,sizeof(rq2))&&!amihass_control_request_code(&c,"test-only-token-for-local-fixture","alarm_control_panel.home","Disarm","47 11",rq2,sizeof(rq2)));
  assert(amihass_control_request(&c,"test-only-token-for-local-fixture","alarm_control_panel.home","Disarm",rq2,sizeof(rq2))&&!strstr(rq2,"code"));
 }
 puts("AmiHASS core policies passed.");return 0;
}
