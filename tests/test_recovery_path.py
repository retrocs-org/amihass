"""The classic Amiga path uses IoErr rather than assuming POSIX fopen errno."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "recovery_io.h"
static long code;static int unlocks;
BPTR Lock(STRPTR path,long access){assert(access==ACCESS_READ);if(!strcmp(path,"present"))return 1;code=!strcmp(path,"missing")?ERROR_OBJECT_NOT_FOUND:!strcmp(path,"missing-parent")?ERROR_DIR_NOT_FOUND:214;return 0;}
void UnLock(BPTR lock){assert(lock==1);++unlocks;}
long IoErr(void){return code;}
int main(void){errno=EACCES;assert(ah_file_missing("missing")&&ah_file_missing("missing-parent"));errno=ENOENT;assert(!ah_file_missing("unreadable"));assert(!ah_file_missing("present")&&unlocks==1);puts("Recovery paths: AmigaDOS missing files distinguished from unreadable files independently of errno.");return 0;}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'dos').mkdir();(p/'proto').mkdir();(p/'dos/dos.h').write_text('typedef long BPTR;typedef char *STRPTR;\n#define ACCESS_READ -2\n#define ERROR_OBJECT_NOT_FOUND 205\n#define ERROR_DIR_NOT_FOUND 204\n');(p/'proto/dos.h').write_text('BPTR Lock(STRPTR,long);void UnLock(BPTR);long IoErr(void);\n');(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-D__VBCC__','-I'+str(p),'-I'+str(root/'include'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
