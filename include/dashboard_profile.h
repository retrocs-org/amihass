#ifndef AH_DASHBOARD_PROFILE_H
#define AH_DASHBOARD_PROFILE_H
/* Per-page refresh profiles and time-of-day pages, kept beside the layout in
 * their own small file so the Dashboard layout format is unchanged. Pages
 * are matched by name (names are unique within a Dashboard). */
#include "dashboard.h"
#define AH_PROFILE_FILE "ENVARC:AmiHASS/DashboardPages.cfg"
#define AH_PROFILE_NORMAL 0
#define AH_PROFILE_FAST 1
#define AH_PROFILE_SLOW 2
#define AH_PROFILE_VERY_SLOW 3
#define AH_PROFILE_NEVER 24 /* from-hour value: no time-of-day switching */
struct AHPageProfile {char name[33];unsigned int profile,from,until;};
struct AHPageProfiles {unsigned int count;struct AHPageProfile pages[AH_DASH_PAGES];};
int ah_profiles_load(struct AHPageProfiles *p,const char *path);
int ah_profiles_save(const struct AHPageProfiles *p,const char *path);
/* Settings of a page (defaults when it has none). */
struct AHPageProfile ah_profile_get(const struct AHPageProfiles *p,const char *name);
/* Store settings; a page at all defaults is dropped. Returns 0 if full. */
int ah_profile_set(struct AHPageProfiles *p,const struct AHPageProfile *settings);
void ah_profile_rename(struct AHPageProfiles *p,const char *from,const char *to);
/* Drop settings of pages that no longer exist. */
void ah_profile_prune(struct AHPageProfiles *p,const struct AHDashboardBook *book);
/* Refresh seconds for Automatic tiles on that page; 0 = service default. */
unsigned int ah_profile_seconds(unsigned int profile);
/* The first page whose window [from,until) contains hour (wrapping at
 * midnight; from==until is all day), or -1. */
int ah_profile_page_for_hour(const struct AHPageProfiles *p,const struct AHDashboardBook *book,unsigned int hour);
#endif
