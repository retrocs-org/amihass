#ifndef AH_PICKER_MEMORY_H
#define AH_PICKER_MEMORY_H
#include <string.h>
struct AHPickerMemory {char area[64],query[64],label[64];unsigned int category;int known,sample;};
static void ah_picker_remember(struct AHPickerMemory *m,unsigned int category,const char *area,const char *query,int entity_stage)
{m->category=category<19 ? category:0;strncpy(m->area,area,63);m->area[63]=0;if(!entity_stage){strncpy(m->query,query,63);m->query[63]=0;}}
static void ah_picker_provider(struct AHPickerMemory *m,int sample)
{if(m->known&&m->sample!=sample){m->label[0]=0;m->area[0]=0;m->query[0]=0;}m->known=1;m->sample=sample;}
#endif
