/* See ah_locale.h. */
#include <stdlib.h>
#include <string.h>
#include "ah_locale.h"
#include "ah_texts.h"
#ifndef AH_HOST_TEST
#include <exec/types.h>
#include <libraries/locale.h>
#include <proto/exec.h>
#include <proto/locale.h>
struct Library *LocaleBase=NULL;
static struct Catalog *catalog;static int tried;
static void close_catalog(void){if(catalog)CloseCatalog(catalog);catalog=NULL;if(LocaleBase)CloseLibrary(LocaleBase);LocaleBase=NULL;}
static void open_catalog(void)
{struct TagItem tags[2];tried=1;LocaleBase=OpenLibrary("locale.library",38);if(!LocaleBase)return;
 tags[0].ti_Tag=OC_BuiltInLanguage;tags[0].ti_Data=(ULONG)"english";tags[1].ti_Tag=TAG_DONE;tags[1].ti_Data=0;
 catalog=OpenCatalogA(NULL,(STRPTR)"amihass.catalog",tags);atexit(close_catalog);}
#endif
/* Same sequence of % conversions (flags and widths may differ). */
int ah_same_format(const char *a,const char *b)
{for(;;){while(*a&&*a!='%')++a;while(*b&&*b!='%')++b;if(!*a||!*b)return !*a&&!*b;++a;++b;if(*a=='%'&&*b=='%'){++a;++b;continue;}
  while(*a&&strchr("-+ #0123456789.",*a))++a;while(*b&&strchr("-+ #0123456789.",*b))++b;
  if(*a=='l'||*a=='h'){if(*b!=*a)return 0;++a;++b;}else if(*b=='l'||*b=='h')return 0;if(*a!=*b||!*a)return 0;++a;++b;}}
const char *ah_tr(long id,const char *builtin)
{
#ifndef AH_HOST_TEST
 const char *s;if(!tried)open_catalog();if(!catalog)return builtin;s=(const char *)GetCatalogStr(catalog,id,(STRPTR)builtin);return s&&ah_same_format(s,builtin)?s:builtin;
#else
 (void)id;return builtin;
#endif
}
void ah_tr_list(const char **labels,const long *ids,const char *const *builtins,unsigned int count)
{unsigned int i;for(i=0;i<count;++i)labels[i]=ah_tr(ids[i],builtins[i]);labels[count]=NULL;}
/* English-keyed lookup for text that reaches the screen through the panel
 * and GadTools helpers: the generated table maps each English string to its
 * catalog ID (binary search). Unknown text (names, states, numbers) passes
 * through unchanged. */
const char *ah_tx(const char *english)
{int lo=0,hi=(int)(sizeof(ah_text_table)/sizeof(ah_text_table[0]))-1,mid,c;if(!english||!*english)return english;
 while(lo<=hi){mid=(lo+hi)/2;c=strcmp(english,ah_text_table[mid].text);if(!c)return ah_tr(ah_text_table[mid].id,ah_text_table[mid].text);if(c<0)hi=mid-1;else lo=mid+1;}return english;}
/* Cycle-gadget labels must outlive the gadget, so each source array keeps
 * one translated copy; when the pool is full the English array is used. */
#define LABEL_SETS 24
#define LABEL_MAX 32
static struct {const void *source;const char *labels[LABEL_MAX+1];} label_sets[LABEL_SETS];
const char **ah_tx_labels(const char **labels)
{unsigned int i,k;if(!labels)return labels;for(k=0;labels[k]&&k<LABEL_MAX;++k){}if(labels[k])return labels;
 for(i=0;i<LABEL_SETS;++i)if(label_sets[i].source==(const void *)labels||!label_sets[i].source)break;if(i==LABEL_SETS)return labels;
 label_sets[i].source=labels;for(k=0;labels[k];++k)label_sets[i].labels[k]=ah_tx(labels[k]);label_sets[i].labels[k]=NULL;return label_sets[i].labels;}
const char *ah_txf(const char *format){return ah_tx(format);}
