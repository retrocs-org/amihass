"""Exercise bounded diagnostic files and the viewer's read-only line wrapping."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
writer=(ROOT/'src/log.c').read_text();writer=writer[writer.index('static const char *event_text('):]
reader=(ROOT/'src/log_view.c').read_text();reader=reader[reader.index('static void add_log_line('):reader.index('static void publish_lines(')]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include "ah_log.h"
struct MsgPort {int unused;};static struct MsgPort guard;
static int denied,locks,unlocks;
static struct MsgPort *log_lock(void){++locks;return denied ? NULL:&guard;}
static void log_unlock(struct MsgPort *p){assert(p==&guard);++unlocks;}
static long log_size(void){struct stat s;return stat(AH_LOG_FILE,&s)==0 ? (long)s.st_size:0;}
static int log_rotate(void){remove(AH_LOG_PREVIOUS);return rename(AH_LOG_FILE,AH_LOG_PREVIOUS)==0;}
static void log_append(const char *line,unsigned long length){FILE *f=fopen(AH_LOG_FILE,"ab");if(f){fwrite(line,1,length,f);fclose(f);}}
static void log_timestamp(char stamp[40]){strcpy(stamp,"17-Nov-26 14:30:00");}
struct LogView {char lines[AH_LOG_LINES][160];int count,first,columns;};
'''+writer+reader+r'''
static long size(const char *path){struct stat s;return stat(path,&s)==0 ? (long)s.st_size:0;}
int main(void){struct LogView view;int i,j;char *all;FILE *file;long length;
 memset(&view,0,sizeof(view));view.columns=70;read_log(&view,AH_LOG_FILE);assert(!view.count);
 ah_log_event((enum AHLogEvent)999,0);assert(!size(AH_LOG_FILE)&&!locks);
 denied=1;ah_log_event(AH_LOG_TEST_OK,0);assert(!size(AH_LOG_FILE)&&!unlocks);denied=0;
 ah_log_event(AH_LOG_TEST_FAILED,60);ah_log_event(AH_LOG_SERVICE_STARTED,0);
 file=fopen(AH_LOG_FILE,"rb");assert(file);all=malloc(65536);length=(long)fread(all,1,65535,file);all[length]=0;fclose(file);
 assert(strstr(all,"Certificate unverified")&&strstr(all,"code 60")&&strstr(all,"Service started"));assert(!strstr(all,"Bearer ")&&!strstr(all,"https://")&&!strstr(all,"fixture-secret"));
 for(i=0;i<3000;++i)ah_log_event(AH_LOG_TEST_FAILED,i%70);
 assert(size(AH_LOG_FILE)>0&&size(AH_LOG_FILE)<=AH_LOG_LIMIT&&size(AH_LOG_PREVIOUS)>0&&size(AH_LOG_PREVIOUS)<=AH_LOG_LIMIT);
 ah_log_event(AH_LOG_SERVICE_STOPPED,0);
 memset(&view,0,sizeof(view));view.columns=70;read_log(&view,AH_LOG_PREVIOUS);read_log(&view,AH_LOG_FILE);
 assert(view.count==AH_LOG_LINES);for(i=0;i<view.count;++i){j=(view.first+i)%AH_LOG_LINES;assert(strlen(view.lines[j])<=70);}
 j=(view.first+view.count-1)%AH_LOG_LINES;assert(strstr(view.lines[j],"Service stopped"));
 /* Loading and wrapping never writes or clears the diagnostic files. */
 length=size(AH_LOG_FILE);memset(&view,0,sizeof(view));view.columns=24;read_log(&view,AH_LOG_FILE);assert(size(AH_LOG_FILE)==length);
 for(i=0;i<view.count;++i){j=(view.first+i)%AH_LOG_LINES;assert(strlen(view.lines[j])<=24);}
 assert(locks==unlocks+1);free(all);
 puts("Logs: fixed diagnostic events, error codes, rotation limits, read-only viewing and line wrapping passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'test.c').write_text(program)
    command=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(ROOT/'include'),str(p/'test.c'),'-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
    subprocess.run(command,check=True);subprocess.run([str(p/'test')],cwd=p,check=True)
