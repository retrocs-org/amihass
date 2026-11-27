#ifndef AH_DASHBOARD_H
#define AH_DASHBOARD_H
#include "amihass_core.h"
#define AH_DASH_TILES 24
#define AH_DASH_FILE "ENVARC:AmiHASS/Dashboard.cfg"
/* Most actions a tile can offer for one entity (climate needs 9). */
#define AH_DASH_ACTIONS 12
struct AHDashboardTile {char label[33],entity[96],action[20];unsigned int refresh,display,timestamp;long gauge_min,gauge_max;};
struct AHDashboardLayout {char name[33];unsigned int columns,rows;struct AHDashboardTile tiles[AH_DASH_TILES];};
void ah_dashboard_defaults(struct AHDashboardLayout *layout);
void ah_dashboard_sample(struct AHDashboardLayout *layout);
unsigned int ah_dashboard_count(const struct AHDashboardLayout *layout);
int ah_dashboard_move(struct AHDashboardLayout *layout,unsigned int from,unsigned int to);
int ah_dashboard_resize(struct AHDashboardLayout *layout,unsigned int columns,unsigned int rows);
unsigned int ah_dashboard_editor_actions(const char *entity,const struct AHControls *controls,int known,const char *saved,const char **labels,unsigned int cap);
unsigned int ah_dashboard_supported_actions(const char *entity,const struct AHControls *controls,int known,const char **labels,unsigned int cap);
int ah_dashboard_confirm_required(const char *entity,const char *action,int sample);
unsigned int ah_dashboard_actions(const char *entity,const char **labels,unsigned int capacity);
int ah_dashboard_load_status(struct AHDashboardLayout *layout,const char *path,int *recovered);
int ah_dashboard_load(struct AHDashboardLayout *layout,const char *path);
int ah_dashboard_save(const struct AHDashboardLayout *layout,const char *path);
#define AH_DASH_PAGES 8
#define AH_DASH_PRESETS 8
struct AHDashboardPreset {char name[33];unsigned long rgb;};
struct AHDashboardBook {unsigned int count,current,preset_count,read_only;struct AHDashboardLayout pages[AH_DASH_PAGES];struct AHDashboardPreset presets[AH_DASH_PRESETS];};
void ah_dashboard_book_defaults(struct AHDashboardBook *book);
int ah_dashboard_book_load_status(struct AHDashboardBook *book,const char *path,int *recovered);
int ah_dashboard_book_load(struct AHDashboardBook *book,const char *path);
int ah_dashboard_book_save(const struct AHDashboardBook *book,const char *path);
int ah_dashboard_page_add(struct AHDashboardBook *book,const char *name);
int ah_dashboard_page_remove(struct AHDashboardBook *book,unsigned int index);
int ah_dashboard_preset(struct AHDashboardBook *book,const char *name,unsigned long rgb);
int ah_dashboard_export(const struct AHDashboardBook *book,const char *path);
int ah_dashboard_import(struct AHDashboardBook *book,const char *path);
int ah_dashboard_transfer(struct AHDashboardBook *book,unsigned int from_page,unsigned int from,unsigned int to_page,unsigned int to,int copy);
int ah_dashboard_source_matches(const char *entity,int sample);
int ah_dashboard_refresh_valid(unsigned int seconds);
void ah_dashboard_estimate(const struct AHDashboardLayout *layout,unsigned int first,unsigned int count,unsigned int default_poll,unsigned int *unique,unsigned long *milli_requests);
unsigned int ah_dashboard_poll(unsigned int seconds,int inactive);
int ah_dashboard_shift(struct AHDashboardLayout *l,unsigned int from,int direction);
int ah_dashboard_stale(unsigned long age,unsigned int refresh);
unsigned int ah_dashboard_refresh(unsigned int seconds);
int ah_dashboard_gauge(const struct AHDashboardTile *tile,const struct AmiHASSEntity *value,int result,int connected,unsigned int *percent);
void ah_dashboard_value(const struct AHDashboardTile *tile,const struct AmiHASSEntity *value,const struct AHControls *controls,char out[96]);
int ah_dashboard_template(struct AHDashboardLayout *layout,unsigned int kind);
int ah_dashboard_swap(struct AHDashboardBook *book,unsigned int from_page,unsigned int from,unsigned int to_page,unsigned int to);
#endif
