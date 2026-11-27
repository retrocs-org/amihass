#ifndef AMIHASS_PREFERENCES_H
#define AMIHASS_PREFERENCES_H
#include "service.h"
struct AmiHASSPreferences {
 struct AmiHASSConnection connection;
 char token[1025];
 int dirty,forget,boot,saved_boot,connection_result,reconnect,low_memory,sample_mode,push_updates;unsigned int retry_interval,background_budget;int remote_security;
};
int amihass_server_save(struct AmiHASSPreferences *prefs,const struct AmiHASSConnection *connection,const char *token,int forget);
int amihass_server_save_options(struct AmiHASSPreferences *prefs,const struct AmiHASSConnection *connection,const char *token,int forget,int reconnect,unsigned int retry_interval);
struct NDPanelField;
unsigned int amihass_about_fields(struct NDPanelField *fields);
void amihass_about_ui(void);
void amihass_diagnostics_ui(void);
int amihass_diagnostics_check(void);
int amihass_dependencies_check(void);
void amihass_health_ui(void);
void amihass_recovery_notice(const char *what);
void amihass_preferences_ui(int connection_page);
int amihass_connection_edit(struct AmiHASSPreferences *prefs);
int amihass_general_edit(struct AmiHASSPreferences *prefs);
#endif
