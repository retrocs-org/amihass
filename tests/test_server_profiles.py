"""Saved server profiles: validation, replace by name, separate token file, damaged files and erasure."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "server_profiles.h"
#include "amihass.h"
void amihass_erase(void *p,unsigned long n){volatile char *c=p;while(n--)*c++=0;}
static const char *tok="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.test-only-fixture-token.abc";
int main(void){static struct AHServerProfiles p,q;FILE *f;char text[4000];size_t n;memset(&p,0,sizeof(p));
 assert(ah_servers_set(&p,"Home","https://ha.example:8123/",tok)&&p.count==1&&!strcmp(p.items[0].url,"https://ha.example:8123"));
 assert(ah_servers_set(&p,"Cabin","http://192.168.1.5:8123",tok)&&p.count==2);assert(ah_servers_set(&p,"Home","https://other.example",tok)&&p.count==2&&!strcmp(p.items[0].url,"https://other.example"));
 assert(!ah_servers_set(&p,"","https://x.example",tok)&&!ah_servers_set(&p,"Bad\tname","https://x.example",tok)&&!ah_servers_set(&p,"X","ftp://x",tok)&&!ah_servers_set(&p,"X","https://x.example","")&&p.count==2);
 assert(ah_servers_options(&p,"Home",0,900)&&!ah_servers_options(&p,"Home",1,15));
 assert(ah_servers_save(&p,"servers.cfg","servers.token")&&ah_servers_load(&q,"servers.cfg","servers.token")&&q.count==2&&!strcmp(q.items[1].name,"Cabin")&&!strcmp(q.items[1].token,tok)&&!q.items[0].reconnect&&q.items[0].retry_interval==900&&q.items[1].reconnect&&q.items[1].retry_interval==60);
 f=fopen("servers.cfg","rb");n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);assert(!strstr(text,"test-only-fixture-token"));/* the address file holds no token */
 assert(ah_servers_remove(&p,"Home")&&p.count==1&&!strcmp(p.items[0].name,"Cabin")&&!p.items[1].token[0]&&!ah_servers_remove(&p,"Home"));
 assert(ah_servers_save(&p,"servers.cfg","servers.token")&&ah_servers_load(&q,"servers.cfg","servers.token")&&q.count==1);
 remove("servers.token");assert(ah_servers_load(&q,"servers.cfg","servers.token")&&q.count==0);/* no token: not usable */
 f=fopen("servers.cfg","wb");fputs("OTHER\n",f);fclose(f);assert(!ah_servers_load(&q,"servers.cfg","servers.token"));
 f=fopen("servers.cfg","wb");fputs("AMIHASSSERVERS1\nLegacy\thttps://legacy.example\n",f);fclose(f);f=fopen("servers.token","wb");fprintf(f,"AMIHASSSERVERTOKENS1\nLegacy\t%s\n",tok);fclose(f);assert(ah_servers_load(&q,"servers.cfg","servers.token")&&q.count==1&&q.items[0].reconnect&&q.items[0].retry_interval==60);
 ah_servers_erase(&p);assert(!p.items[0].token[0]);
 puts("Server profiles: validation, replace by name, tokens kept apart, removal, damaged files and erasure passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'t.c').write_text(program)
    cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'t.c'),str(root/'src/server_profiles.c'),str(root/'src/core.c'),'-o',str(p/'t')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'t')],cwd=p,check=True)
