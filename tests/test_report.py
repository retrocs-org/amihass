from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "diagnostic_report.h"
int main(void){struct AmiHASSEntity e[3];FILE *f;char text[2048];size_t n;memset(e,0,sizeof(e));strcpy(e[0].id,"ipc.received");strcpy(e[0].state,"4294967295");strcpy(e[0].name,"secret-token");strcpy(e[1].id,"connection.error");strcpy(e[1].state,"-15");strcpy(e[2].id,"private.token");strcpy(e[2].state,"secret-token");f=tmpfile();assert(f&&ah_report_status(f,"https://private.invalid/secret-token")&&ah_report_items(f,e,3));rewind(f);n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);assert(strstr(text,"Server state: unknown")&&strstr(text,"ipc.received: 4294967295")&&strstr(text,"connection.error: -15")&&!strstr(text,"secret-token")&&!strstr(text,"https://")&&!strstr(text,"private.token"));strcpy(e[0].state,"123\nsecret-token");f=tmpfile();assert(f&&!ah_report_items(f,e,1)&&!ah_report_items(f,e,17));fclose(f);f=tmpfile();assert(f&&ah_report_status(f,"auth_rejected"));rewind(f);n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);assert(strstr(text,"Server state: auth_rejected"));puts("Reports: allowed states/counters only, full unsigned values, credential/URL rejection and malformed numeric rejection passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),'-I'+str(root/'SDK'),str(p/'test.c'),str(root/'src/diagnostic_report.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
