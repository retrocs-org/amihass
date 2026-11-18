/* A resident socket library alone does not mean an interface is configured. */
#define SocketBase AmiHASSNetworkSocketBase
#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/sockio.h>
#include <string.h>
#include "connectivity.h"
struct Library *SocketBase=NULL;
#if defined(__VBCC__) && !defined(__PPC__)
/* Classic bsdsocket.library ABI; these calls do not send network traffic. */
LONG __ah_network_socket(__reg("a6") struct Library *,__reg("d0") LONG,__reg("d1") LONG,__reg("d2") LONG)="\tjsr\t-30(a6)";
LONG __ah_network_ioctl(__reg("a6") struct Library *,__reg("d0") LONG,__reg("d1") ULONG,__reg("a0") void *)="\tjsr\t-114(a6)";
LONG __ah_network_close(__reg("a6") struct Library *,__reg("d0") LONG)="\tjsr\t-120(a6)";
#define socket(a,b,c) __ah_network_socket(SocketBase,(a),(b),(c))
#define IoctlSocket(a,b,c) __ah_network_ioctl(SocketBase,(a),(b),(c))
#define CloseSocket(a) __ah_network_close(SocketBase,(a))
#else
#include <proto/bsdsocket.h>
#endif
int amihass_network_running(void)
{
 union {ULONG alignment;char bytes[4096];} buffer;
 struct ifconf interfaces;struct ifreq request,entry;struct sockaddr_in address;
 unsigned int offset=0,length,stride,address_size;ULONG ip;LONG fd;int present,running=0;
 Forbid();present=FindName(&SysBase->LibList,"bsdsocket.library")!=NULL;Permit();
 if(!present)return 0;
 SocketBase=OpenLibrary("bsdsocket.library",4);if(!SocketBase)return 0;
 fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)goto done;
 memset(&interfaces,0,sizeof(interfaces));interfaces.ifc_len=sizeof(buffer.bytes);interfaces.ifc_buf=buffer.bytes;
 if(IoctlSocket(fd,SIOCGIFCONF,&interfaces)<0 || interfaces.ifc_len<0 || interfaces.ifc_len>(LONG)sizeof(buffer.bytes))goto close_socket;
 length=(unsigned int)interfaces.ifc_len;
 while(length-offset>=sizeof(struct ifreq)){
  memcpy(&entry,buffer.bytes+offset,sizeof(entry));
  address_size=entry.ifr_addr.sa_len;
  if(address_size<sizeof(struct sockaddr))address_size=sizeof(struct sockaddr);
  stride=sizeof(struct ifreq)-sizeof(struct sockaddr)+address_size;
  if(stride>length-offset)break;
  offset+=stride;
  if(entry.ifr_addr.sa_family!=AF_INET)continue;
  memset(&request,0,sizeof(request));memcpy(request.ifr_name,entry.ifr_name,IFNAMSIZ);request.ifr_name[IFNAMSIZ-1]=0;
  if(!request.ifr_name[0] || IoctlSocket(fd,SIOCGIFFLAGS,&request)<0 || !(request.ifr_flags&IFF_UP) || (request.ifr_flags&IFF_LOOPBACK))continue;
  if(IoctlSocket(fd,SIOCGIFADDR,&request)<0 || request.ifr_addr.sa_family!=AF_INET)continue;
  memcpy(&address,&request.ifr_addr,sizeof(address));ip=ntohl(address.sin_addr.s_addr);
  /* Ignore unspecified, loopback, multicast and broadcast addresses. */
  if(!(ip&0xff000000UL) || (ip&0xff000000UL)==0x7f000000UL || ip>=0xe0000000UL)continue;
  running=1;break;
 }
close_socket:CloseSocket(fd);
done:CloseLibrary(SocketBase);SocketBase=NULL;return running;
}
