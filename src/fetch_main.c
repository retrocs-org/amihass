/* AmiHASSFetch: the service's small downloader helper. It exists so each HTTP
 * request loads a few kilobytes instead of the whole service executable. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "nd_version.h"
#include "nd_reply.h"
#include "nd_interaction.h"
#define FETCH_GUARD "AmiHASS.Fetch"
#define FETCH_PREFIX "T:AmiHASS-"
#define FETCH_PROGRESS_FILE "request.conf"
#include "fetch_worker.h"
const char fetch_version[]="$VER: AmiHASSFetch 0.3 (24.11.2026)";
long __stack=16384;
int main(int argc,char **argv)
{
 if(argc!=3 || strcmp(argv[1],"FETCH") || strncmp(argv[2],"AmiHASS.Job.",12) || strlen(argv[2])>70)return 20;
 return fetch_worker(argv[2]);
}
