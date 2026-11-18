"""Regression coverage for resident stacks with unconfigured interfaces."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'src/network.c').read_text();source=source[source.index('int amihass_network_running('):]
program=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef int32_t LONG;
struct sockaddr {unsigned char sa_len,sa_family;unsigned char data[14];};
struct in_addr {uint32_t s_addr;};
struct sockaddr_in {unsigned char sin_len,sin_family;unsigned short sin_port;struct in_addr sin_addr;char zero[8];};
#define IFNAMSIZ 16
struct ifreq {char ifr_name[IFNAMSIZ];union {struct sockaddr addr;short flags;} data;};
#define ifr_addr data.addr
#define ifr_flags data.flags
struct ifconf {LONG ifc_len;char *ifc_buf;};
#define AF_INET 2
#define SOCK_DGRAM 2
#define IFF_UP 1
#define IFF_LOOPBACK 8
#define SIOCGIFCONF 1
#define SIOCGIFFLAGS 2
#define SIOCGIFADDR 3
#define ntohl(x) (x)
struct Library {int unused;};struct {int LibList;} execbase,*SysBase=&execbase;struct Library *SocketBase;
struct Interface {char name[16];int flags;uint32_t address;};
static struct Interface entries[8];static int count,resident,opens,closes,sockets,closed_sockets,open_fail,socket_fail,enum_fail,flags_fail,addr_fail,variable_first,truncated;
static void Forbid(void){}static void Permit(void){}
static void *FindName(void *list,const char *name){(void)list;assert(!strcmp(name,"bsdsocket.library"));return resident ? (void *)1:NULL;}
static struct Library *OpenLibrary(const char *name,int version){assert(!strcmp(name,"bsdsocket.library")&&version==4);++opens;return open_fail ? NULL:(struct Library *)1;}
static void CloseLibrary(struct Library *lib){assert(lib);++closes;}
static LONG socket(int family,int kind,int protocol){assert(family==AF_INET&&kind==SOCK_DGRAM&&protocol==0);++sockets;return socket_fail ? -1:7;}
static void CloseSocket(LONG fd){assert(fd==7);++closed_sockets;}
static int IoctlSocket(LONG fd,ULONG command,void *arg){int i;struct ifreq *r=arg;assert(fd==7);
 if(command==SIOCGIFCONF){struct ifconf *c=arg;int at=0;assert(c->ifc_len>=512);if(enum_fail)return -1;
  if(variable_first){struct ifreq entry;memset(&entry,0,sizeof(entry));entry.ifr_addr.sa_family=18;entry.ifr_addr.sa_len=24;memcpy(c->ifc_buf,&entry,sizeof(entry));at=sizeof(entry)+8;}
  for(i=0;i<count;++i){struct ifreq entry;memset(&entry,0,sizeof(entry));strcpy(entry.ifr_name,entries[i].name);entry.ifr_addr.sa_family=AF_INET;entry.ifr_addr.sa_len=16;memcpy(c->ifc_buf+at,&entry,sizeof(entry));at+=sizeof(entry);}
  c->ifc_len=truncated ? 1:at;return 0;}
 for(i=0;i<count&&strcmp(r->ifr_name,entries[i].name);++i){}assert(i<count);
 if(command==SIOCGIFFLAGS){if(flags_fail)return -1;r->ifr_flags=entries[i].flags;return 0;}
 assert(command==SIOCGIFADDR);if(addr_fail)return -1;
 {struct sockaddr_in address;memset(&address,0,sizeof(address));address.sin_family=AF_INET;address.sin_len=16;address.sin_addr.s_addr=entries[i].address;memcpy(&r->ifr_addr,&address,sizeof(address));}return 0;
}
'''+source+r'''
static void reset(void){memset(entries,0,sizeof(entries));count=opens=closes=sockets=closed_sockets=open_fail=socket_fail=enum_fail=flags_fail=addr_fail=variable_first=truncated=0;resident=1;}
static void add(const char *name,int flags,uint32_t ip){strcpy(entries[count].name,name);entries[count].flags=flags;entries[count++].address=ip;}
static void check(int expected){assert(amihass_network_running()==expected);assert(!SocketBase);assert(closes==opens-(open_fail?1:0));assert(closed_sockets==sockets-(socket_fail?1:0));}
int main(void){
 reset();resident=0;check(0);assert(!opens&&!sockets);
 reset();open_fail=1;check(0);assert(!sockets);
 reset();socket_fail=1;check(0);
 reset();check(0);
 reset();add("lo0",IFF_UP|IFF_LOOPBACK,0x7f000001);check(0);
 reset();add("lo0",IFF_UP|IFF_LOOPBACK,0x7f000001);add("A2065",IFF_UP,0);check(0);
 reset();add("A2065",0,0xc0a80102);check(0);
 reset();add("A2065",IFF_UP,0x7f000001);check(0);
 reset();add("A2065",IFF_UP,0xffffffff);check(0);
 reset();add("A2065",IFF_UP,0xe0000001);check(0);
 reset();add("A2065",IFF_UP,0xc0a80102);check(1);
 reset();add("A2065",IFF_UP,0xc0a80102);check(1);entries[0].address=0;check(0);entries[0].address=0xc0a80102;check(1);
 reset();add("lo0",IFF_UP|IFF_LOOPBACK,0x7f000001);add("offline",IFF_UP,0);add("ppp0",IFF_UP,0x0a000002);variable_first=1;check(1);
 reset();add("A2065",IFF_UP,0xc0a80102);enum_fail=1;check(0);
 reset();add("A2065",IFF_UP,0xc0a80102);flags_fail=1;check(0);
 reset();add("A2065",IFF_UP,0xc0a80102);addr_fail=1;check(0);
 reset();add("A2065",IFF_UP,0xc0a80102);truncated=1;check(0);
 puts("Network readiness: resident-only, loopback, unconfigured/down interfaces, assigned IPs, reconnects and query failures passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as temporary:
    p=Path(temporary);(p/'test.c').write_text(program)
    command=[os.environ.get('CC','cc'),'-std=c99',str(p/'test.c'),'-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
    subprocess.run(command,check=True);subprocess.run([str(p/'test')],check=True)
