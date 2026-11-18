#ifndef AH_BROWSER_DATA_H
#define AH_BROWSER_DATA_H
#define AH_BROWSER_MAX 32
struct AHBrowserEntity {const char *id,*name,*state,*attributes;};
unsigned int ah_browser_count(void);
const struct AHBrowserEntity *ah_browser_entity(unsigned int index);
int ah_browser_domain(const struct AHBrowserEntity *entity,char *out,unsigned int capacity);
unsigned int ah_browser_filter(const char *domain,const char *search,unsigned int *indices,unsigned int capacity);
#endif
