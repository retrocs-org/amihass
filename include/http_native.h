#ifndef AH_HTTP_NATIVE_H
#define AH_HTTP_NATIVE_H
/* Protocol half of the native HTTP worker: reads the same request.conf the
 * service writes for curl, builds a keep-alive HTTP/1.1 request and parses
 * the response incrementally (Content-Length, chunked or until close).
 * No sockets here, so it is tested on the host. */
#define AH_HTTP_GUARD "AmiHASS.HTTP"
#define AH_HTTP_HEADERS 4
/* Match the service request.conf and SDK template-body limits. */
#define AH_HTTP_CONFIG_BYTES 8192
#define AH_HTTP_BODY_BYTES 4096
#define AH_HTTP_REQUEST_BYTES 12288
struct AHHttpRequest {int tls,port,post;char host[128],path[700],header[AH_HTTP_HEADERS][1100],body[AH_HTTP_BODY_BYTES];unsigned int headers;unsigned long body_len;};
/* 0 when the config is not one the service writes (unknown keys, bad URL). */
int ah_http_conf_parse(const char *conf,struct AHHttpRequest *r);
/* Request text; returns its length, 0 when it does not fit. */
unsigned long ah_http_build(const struct AHHttpRequest *r,char *out,unsigned long cap);
#define AH_HTTP_MORE 0
#define AH_HTTP_DONE 1
#define AH_HTTP_BAD (-1)
#define AH_HTTP_TOO_BIG (-2)
#define AH_HTTP_SINK (-3)
typedef int (*AHHttpSink)(void *user,const char *data,unsigned long n);
struct AHHttpReader {int stage,status,chunked,close,until_close;unsigned long length,left,body,limit,head;char line[512];unsigned int used;};
void ah_http_reader_init(struct AHHttpReader *r,unsigned long limit);
/* Feeds received bytes; body bytes go to sink. Returns AH_HTTP_MORE,
 * AH_HTTP_DONE or an error. *used tells how many bytes were consumed. */
int ah_http_feed(struct AHHttpReader *r,const char *data,unsigned long n,unsigned long *used,AHHttpSink sink,void *user);
/* The connection closed: DONE for a body that runs to close, else BAD. */
int ah_http_eof(struct AHHttpReader *r);
/* curl exit code equivalent for a finished or failed exchange. */
int ah_http_curl_code(int reader_result,int status);
#endif
