#ifndef AH_DIAGNOSTIC_REPORT_H
#define AH_DIAGNOSTIC_REPORT_H
#include <stdio.h>
#include "amihass_core.h"
int ah_report_status(FILE *file,const char *state);
int ah_report_items(FILE *file,const struct AmiHASSEntity *items,unsigned int count);
#endif
