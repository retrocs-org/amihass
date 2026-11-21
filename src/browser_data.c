/* Offline fixtures only. This provider never reads settings or performs I/O. */
#include <string.h>
#include <ctype.h>
#include "browser_data.h"
static const struct AHBrowserEntity entities[]={
 {"light.living_room","Living room light","on","{\"brightness\":180,\"supported_color_modes\":[\"rgb\",\"color_temp\"],\"rgb_color\":[255,180,100],\"color_mode\":\"rgb\",\"color_temp_kelvin\":3000}"},
 {"light.desk","Desk lamp","off","{\"brightness\":0,\"supported_color_modes\":[\"brightness\"]}"},
 {"switch.coffee_machine","Coffee machine","off","{\"friendly_name\":\"Coffee machine\"}"},
 {"sensor.living_room_temperature","Living room temperature","21.5","{\"unit_of_measurement\":\"C\",\"device_class\":\"temperature\"}"},
 {"sensor.outdoor_humidity","Outdoor humidity","68","{\"unit_of_measurement\":\"%\",\"device_class\":\"humidity\"}"},
 {"sensor.house_power","House power","430","{\"unit_of_measurement\":\"W\",\"device_class\":\"power\"}"},
 {"binary_sensor.front_door","Front door","off","{\"device_class\":\"door\"}"},
 {"binary_sensor.hall_motion","Hall motion","on","{\"device_class\":\"motion\"}"},
 {"climate.hallway","Hallway heating","heat","{\"temperature\":21,\"min_temp\":7,\"max_temp\":35,\"target_temp_step\":0.5,\"temperature_unit\":\"C\",\"current_temperature\":20.5,\"hvac_modes\":[\"off\",\"heat\",\"auto\"]}"},
 {"media_player.living_room","Living room player","playing","{\"volume_level\":0.35,\"supported_features\":16437,\"media_title\":\"Amiga radio\",\"media_artist\":\"AmiHASS sample\",\"is_volume_muted\":false}"},
 {"cover.garage","Garage door","closed","{\"current_position\":0,\"supported_features\":15}"},
 {"fan.bedroom","Bedroom fan","on","{\"percentage\":40,\"supported_features\":1,\"preset_modes\":[\"sleep\",\"normal\"]}"},
 {"lock.front_door","Front door lock","locked","{\"supported_features\":1,\"code_format\":null}"},
 {"scene.evening","Evening scene","unknown","{\"friendly_name\":\"Evening scene\"}"},
 {"script.good_night","Good night","off","{\"last_triggered\":null}"},
 {"input_boolean.guest_mode","Guest mode","off","{\"editable\":true}"},
 {"number.heating_limit","Heating limit","23","{\"min\":16,\"max\":28,\"step\":0.5}"},
 {"select.house_mode","House mode","Home","{\"options\":[\"Home\",\"Away\",\"Holiday\"]}"},
 {"weather.home","Home weather","cloudy","{\"temperature\":12,\"humidity\":75}"},
 {"camera.driveway","Driveway camera","idle","{\"stream_available\":true}"},
 {"sensor.garden_battery","Garden battery","unavailable","{\"unit_of_measurement\":\"%\",\"device_class\":\"battery\"}"},
 {"retro_bridge.demo_device","Custom integration device","ready","{\"firmware\":\"0.1\",\"channels\":[1,2],\"details\":{\"mode\":\"demo\"}}"},
};
unsigned int ah_browser_count(void){return sizeof(entities)/sizeof(entities[0]);}
const struct AHBrowserEntity *ah_browser_entity(unsigned int i){return i<ah_browser_count() ? &entities[i]:0;}
int ah_browser_domain(const struct AHBrowserEntity *e,char *out,unsigned int cap)
{const char *dot;unsigned int n;if(!e || !out || !cap || !(dot=strchr(e->id,'.')))return 0;n=(unsigned int)(dot-e->id);if(n+1>cap)return 0;memcpy(out,e->id,n);out[n]=0;return 1;}
static int contains(const char *text,const char *query)
{unsigned int i;if(!*query)return 1;for(;*text;++text){for(i=0;query[i] && text[i] && tolower((unsigned char)query[i])==tolower((unsigned char)text[i]);++i){}if(!query[i])return 1;}return 0;}
unsigned int ah_browser_filter(const char *domain,const char *search,unsigned int *indices,unsigned int cap)
{unsigned int i,n=0;char type[96];if(!domain)domain="";if(!search)search="";for(i=0;i<ah_browser_count();++i){if(*domain && (!ah_browser_domain(&entities[i],type,sizeof(type)) || strcmp(type,domain)))continue;if(!contains(entities[i].id,search) && !contains(entities[i].name,search) && !contains(entities[i].state,search))continue;if(n<cap && indices)indices[n]=i;++n;}return n;}
