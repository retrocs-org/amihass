#ifndef AMIHASS_SERVICE_H
#define AMIHASS_SERVICE_H
#include "amihass.h"
int amihass_backend(const char *program);
int amihass_backend_restore(const char *program);
int amihass_worker(const char *parent);
int amihass_connection_load(struct AmiHASSConnection *connection,char token[1025]);
int amihass_connection_save(const struct AmiHASSConnection *connection,const char *token);
void amihass_connection_ui(void);
int amihass_choose(const char *domain,char entity[96]);
#endif
