#ifndef AH_CONNECTION_ERRORS_H
#define AH_CONNECTION_ERRORS_H
#define AH_TEST_PENDING 0
#define AH_TEST_OK 1
#define AH_TEST_NO_NETWORK (-1)
#define AH_TEST_FAILED (-2)
#define AH_TEST_CANCELLED (-3)
#define AH_TEST_PREPARE (-4)
#define AH_TEST_NO_CURL (-5)
#define AH_TEST_BUSY (-6)
#define AH_TEST_TIMEOUT (-7)

#define AH_TEST_DNS (-8)
#define AH_TEST_AUTH (-9)
#define AH_TEST_FORBIDDEN (-10)
#define AH_TEST_ENDPOINT (-11)
#define AH_TEST_REDIRECT (-12)
#define AH_TEST_HTTP_ERROR (-13)
#define AH_TEST_RESPONSE (-14)
#define AH_TEST_CERTIFICATE (-15)
#define AH_TEST_TLS (-16)
#define AH_TEST_CA_FILE (-17)
#define AH_TEST_CONNECT (-18)
#define AH_TEST_STORAGE (-19)
#define AH_TEST_CURL_VERSION (-20)
/* Fixed text only. Never display downloader stderr or authenticated responses. */
static int ah_connection_result(int curl,int http,int valid)
{if(curl==0&&http==200&&valid)return 1;if(curl==5||curl==6)return AH_TEST_DNS;if(curl==7)return AH_TEST_CONNECT;if(curl==28)return -7;if(curl==60||curl==51)return AH_TEST_CERTIFICATE;if(curl==35)return AH_TEST_TLS;if(curl==77)return AH_TEST_CA_FILE;if(curl==23||curl==26)return AH_TEST_STORAGE;if(curl==1||curl==4||curl==48)return AH_TEST_CURL_VERSION;if(curl==0||curl==22){if(http==401)return AH_TEST_AUTH;if(http==403)return AH_TEST_FORBIDDEN;if(http==404)return AH_TEST_ENDPOINT;if(http>=300&&http<400)return AH_TEST_REDIRECT;if(http>=400)return AH_TEST_HTTP_ERROR;if(curl==0)return AH_TEST_RESPONSE;}return -2;}
static const char *ah_connection_text(int result)
{switch(result){case 1:return "Connected";case -1:return "Network down - start and configure TCP/IP.";case -3:return "Connection test cancelled.";case -4:return "Test launch failed - check temporary storage.";case -5:return "C:curl missing - install a compatible curl.";case -6:return "Previous test finishing - retry shortly.";case -7:return "Timed out - check server, network and firewall.";case AH_TEST_DNS:return "DNS failure - check hostname and DNS settings.";case AH_TEST_AUTH:return "Token rejected (401) - replace the token.";case AH_TEST_FORBIDDEN:return "Access denied (403) - check token permissions.";case AH_TEST_ENDPOINT:return "API not found (404) - check server URL.";case AH_TEST_REDIRECT:return "Redirect refused - use the final server URL.";case AH_TEST_HTTP_ERROR:return "Server HTTP error - check server/proxy logs.";case AH_TEST_RESPONSE:return "Unexpected reply - check Home Assistant URL.";case AH_TEST_CERTIFICATE:return "Certificate unverified - check CA file and clock.";case AH_TEST_TLS:return "TLS handshake failed - check curl/TLS support.";case AH_TEST_CA_FILE:return "CA file unreadable - check certificate path.";case AH_TEST_CONNECT:return "Cannot connect - check server, port and firewall.";case AH_TEST_STORAGE:return "Download I/O failed - check temporary storage.";case AH_TEST_CURL_VERSION:return "curl feature missing - install compatible curl.";default:return "Connection failed - see server logs for the code.";}}
#endif
