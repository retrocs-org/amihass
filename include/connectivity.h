#ifndef AMIHASS_CONNECTIVITY_H
#define AMIHASS_CONNECTIVITY_H
#include "service.h"
#include "connection_errors.h"
#define AH_SERVER_NOT_CONFIGURED 0
#define AH_SERVER_NOT_CONNECTED 1
#define AH_SERVER_CONNECTED 2
#define AH_SERVER_FAILURE 3
struct NDPanel;
struct AHConnectionTest {
 struct MsgPort *port;
 char name[80],directory[160],command[1800];
 unsigned long started;
 int transferred;
};
int amihass_network_running(void);
int amihass_connection_test_start(struct AHConnectionTest *test,const struct AmiHASSConnection *connection,const char *token);
int amihass_connection_test_poll(struct AHConnectionTest *test);
void amihass_connection_test_close(struct AHConnectionTest *test);
int amihass_connection_test_dialog(const struct AmiHASSConnection *connection,const char *token,struct NDPanel *panel,int cancel);
int amihass_probe_worker(const char *parent);
const char *amihass_server_status(int configured,int network,int result);
#endif
