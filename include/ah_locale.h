#ifndef AH_LOCALE_H
#define AH_LOCALE_H
/* Translated UI text from LOCALE:Catalogs/<language>/amihass.catalog (or
 * PROGDIR:Catalogs/...), falling back to the built-in English. The catalog
 * opens on first use and closes at program exit. A translation whose %
 * format codes differ from the English text is ignored, so a damaged
 * catalog cannot break formatted output. */
#include "ah_strings.h"
const char *ah_tr(long id,const char *builtin);
/* Fills labels[0..count-1] with translations and ends the list with NULL. */
void ah_tr_list(const char **labels,const long *ids,const char *const *builtins,unsigned int count);
int ah_same_format(const char *a,const char *b);
/* Translation keyed by the English text itself (see ah_texts.h); a format
 * string keeps its % codes, so it is safe for sprintf. */
const char *ah_tx(const char *english);
const char **ah_tx_labels(const char **labels);
const char *ah_txf(const char *format);
#endif
