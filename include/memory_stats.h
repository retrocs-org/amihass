#ifndef AH_MEMORY_STATS_H
#define AH_MEMORY_STATS_H
/* System-wide free Fast RAM, sampled by the bridge; not process allocations. */
struct AHMemoryStats {unsigned long baseline,current,minimum,before,request_min,after,reclaimed,peak,requests,flushes;int active,initialized;};
static void ah_memory_sample(struct AHMemoryStats *s,unsigned long free_bytes)
{if(!s->initialized){s->baseline=s->minimum=free_bytes;s->initialized=1;}s->current=free_bytes;if(free_bytes<s->minimum)s->minimum=free_bytes;if(s->active&&free_bytes<s->request_min)s->request_min=free_bytes;}
static void ah_memory_begin(struct AHMemoryStats *s,unsigned long free_bytes)
{ah_memory_sample(s,free_bytes);s->before=s->request_min=free_bytes;s->active=1;}
static void ah_memory_end(struct AHMemoryStats *s,unsigned long free_bytes)
{unsigned long peak;if(!s->active)return;ah_memory_sample(s,free_bytes);peak=s->before>s->request_min ? s->before-s->request_min:0;if(peak>s->peak)s->peak=peak;s->after=free_bytes;s->active=0;++s->requests;}
static void ah_memory_reclaimed(struct AHMemoryStats *s,unsigned long before,unsigned long after)
{ah_memory_sample(s,after);s->reclaimed=after>before ? after-before:0;++s->flushes;}
#endif
