#include <string.h>
#include "service.h"
#include "amihass_config.h"
int amihass_connection_load(struct AmiHASSConnection *connection,char token[1025])
{struct AmiHASSConfig config;int loaded=amihass_config_load(&config);*connection=config.connection;token[0]=0;if(loaded && config.connection.url[0] && config.token[0])strcpy(token,config.token);amihass_erase(&config,sizeof(config));return loaded && token[0]!=0;}
int amihass_connection_save(const struct AmiHASSConnection *connection,const char *token)
{struct AmiHASSConfig config;int ok;if(!amihass_url(connection->url,!strncmp(connection->url,"http://",7)) || !amihass_token(token,config.token))return 0;amihass_config_load(&config);config.connection=*connection;if(!amihass_token(token,config.token))return 0;ok=amihass_config_save(&config);amihass_erase(&config,sizeof(config));return ok;}
