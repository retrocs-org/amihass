#ifndef AH_BROWSER_LIVE_H
#define AH_BROWSER_LIVE_H
#include "amihass.h"
struct AHBrowserLive {
 struct AmiHASSClient client;struct AHControls controls;
 struct AmiHASSEntity items[32],detail;
 int count,total,offset,selected,running,connected,loading,detail_loading,detail_result,dirty;
 int page_size,page_refresh,sample_mode;
 int wanted,sent_kind,refresh,terminal_error,devices;
 unsigned long ticks,next,age;
 char area[96],domain[24],filter[64],entity[96],device[96],server_state[40],error[96];
};
void ah_browser_live_init(struct AHBrowserLive *live);
void ah_browser_live_close(struct AHBrowserLive *live);
int ah_browser_live_page(struct AHBrowserLive *live,const char *domain,const char *filter,int offset,int refresh);
int ah_browser_live_devices_type(struct AHBrowserLive *live,const char *domain,const char *filter,int offset,int refresh);
int ah_browser_live_devices(struct AHBrowserLive *live,const char *filter,int offset,int refresh);
int ah_browser_live_device_page(struct AHBrowserLive *live,const char *device,const char *domain,const char *filter,int offset,int refresh);
void ah_browser_live_details(struct AHBrowserLive *live,int index);
void ah_browser_live_cancel_details(struct AHBrowserLive *live);
void ah_browser_live_step(struct AHBrowserLive *live);
#endif
