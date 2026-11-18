#ifndef AH_SAMPLE_H
#define AH_SAMPLE_H
#include "amihass.h"
#define AH_SAMPLE_COUNT 22
struct AHSample {struct AmiHASSEntity entities[AH_SAMPLE_COUNT];struct AHControls controls[AH_SAMPLE_COUNT];int remembered_level[AH_SAMPLE_COUNT];};
void ah_sample_init(struct AHSample *sample);
void ah_sample_request(struct AHSample *sample,struct AmiHASSMessage *message,unsigned long now);
#endif
