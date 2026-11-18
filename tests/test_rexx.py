from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1]
source=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "rexx_bridge.h"
int main(void){struct AmiHASSMessage m;char out[4096],longcmd[600];assert(ah_rexx_parse("status",&m)&&m.kind==AMIHASS_STATUS);assert(ah_rexx_parse("HELP",&m)&&!m.kind);assert(ah_rexx_parse("QUERY light.amihass_sample_desk",&m)&&m.kind==AMIHASS_QUERY&&m.refresh==AMIHASS_QUERY_ONCE);assert(ah_rexx_parse("ACTION light.desk \"Turn on\"",&m)&&!strcmp(m.action,"Turn on"));assert(ah_rexx_parse("LIST light 'desk lamp' 16",&m)&&m.offset==16&&!strcmp(m.filter,"desk lamp"));assert(ah_rexx_parse("DEVICES 'living room' 32",&m)&&m.kind==AMIHASS_DEVICES&&m.offset==32);assert(ah_rexx_parse("ENTITIES a11a0000000000000000000000000001 light",&m)&&m.kind==AMIHASS_LIST&&m.entity[0]);assert(!ah_rexx_parse("QUERY ../../bad",&m));assert(!ah_rexx_parse("ACTION light.desk 'Toggle",&m));assert(!ah_rexx_parse("LIST light lamp -1",&m));assert(!ah_rexx_parse("STATUS extra",&m));memset(longcmd,'x',599);longcmd[599]=0;assert(!ah_rexx_parse(longcmd,&m));memset(&m,0,sizeof(m));m.kind=AMIHASS_STATUS;strcpy(m.item.state,AMIHASS_SERVER_SAMPLE);m.status.connected=1;ah_rexx_format(&m,out,sizeof(out));assert(strstr(out,"state=sample")&&strstr(out,"ready=1"));ah_rexx_format(&m,out,8);assert(!out[7]);puts("ARexx: status, discovery, scoped entities, queries, quoted actions, invalid commands and bounded results passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'test.c').write_text(source);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),str(root/'src/rexx_commands.c'),str(root/'src/core.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
