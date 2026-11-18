"""Validate fixed, actionable error classifications without exposing responses."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "connection_errors.h"
int main(void){int i;int cases[][4]={{0,200,1,AH_TEST_OK},{6,0,0,AH_TEST_DNS},{7,0,0,AH_TEST_CONNECT},{28,0,0,AH_TEST_TIMEOUT},{60,0,0,AH_TEST_CERTIFICATE},{35,0,0,AH_TEST_TLS},{77,0,0,AH_TEST_CA_FILE},{22,401,0,AH_TEST_AUTH},{22,403,0,AH_TEST_FORBIDDEN},{22,404,0,AH_TEST_ENDPOINT},{0,302,0,AH_TEST_REDIRECT},{22,503,0,AH_TEST_HTTP_ERROR},{0,200,0,AH_TEST_RESPONSE},{23,0,0,AH_TEST_STORAGE},{48,0,0,AH_TEST_CURL_VERSION}};for(i=0;i<(int)(sizeof(cases)/sizeof(cases[0]));++i){const char *text;assert(ah_connection_result(cases[i][0],cases[i][1],cases[i][2])==cases[i][3]);text=ah_connection_text(cases[i][3]);assert(strlen(text)<64&&!strstr(text,"Bearer")&&!strstr(text,"https://"));}puts("Connection errors: DNS, HTTP/auth, TLS/CA, timeout, storage and API response classifications passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
