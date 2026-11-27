#ifndef AH_SERVER_PROFILES_H
#define AH_SERVER_PROFILES_H
/* Saved Home Assistant servers for quick switching in Server Settings. The
 * service still talks to one server at a time: loading a profile fills in
 * Server Settings, and Save there makes it the active server. Addresses and
 * tokens are kept apart, like the active settings and token. */
#define AH_SERVERS_FILE "ENVARC:AmiHASS/Servers.cfg"
#define AH_SERVERS_TOKENS "ENVARC:AmiHASS/Servers.token"
#define AH_SERVERS 8
struct AHServerProfile {char name[33],url[160],token[1025];int reconnect;unsigned int retry_interval;};
struct AHServerProfiles {unsigned int count;struct AHServerProfile items[AH_SERVERS];};
int ah_servers_load(struct AHServerProfiles *p,const char *file,const char *tokens);
int ah_servers_save(const struct AHServerProfiles *p,const char *file,const char *tokens);
/* Adds or replaces by name; 0 when the name, URL or token is unusable or full. */
int ah_servers_set(struct AHServerProfiles *p,const char *name,const char *url,const char *token);
int ah_servers_options(struct AHServerProfiles *p,const char *name,int reconnect,unsigned int retry_interval);
int ah_servers_remove(struct AHServerProfiles *p,const char *name);
int ah_servers_find(const struct AHServerProfiles *p,const char *name);
/* Overwrites every token in memory. */
void ah_servers_erase(struct AHServerProfiles *p);
#endif
