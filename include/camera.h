#ifndef AH_CAMERA_H
#define AH_CAMERA_H
#include "amihass.h"
#define AH_CAMERA_LIMIT 262144UL
#define AH_CAMERA_WIDTH 640
#define AH_CAMERA_HEIGHT 480
struct AHCamera {unsigned char *data;unsigned long size,updated,touched,serial,requested;unsigned int width,height;int result,pending,discard,small;char entity[96],version[20],format[8];};
int ah_camera_probe(const unsigned char*,unsigned long,unsigned int*,unsigned int*,char format[8]);
int ah_camera_sample(struct AHCamera*);
void ah_camera_clear(struct AHCamera*);
int ah_camera_reply(struct AHCamera*,struct AmiHASSMessage*,unsigned long now);
int ah_camera_path(const char*,char*,unsigned int);
/* small: 96x60 for Dashboard tiles (drawn at 48x30) instead of 320x200. */
int ah_camera_path_size(const char*,int small,char*,unsigned int);
int ah_camera_view(const char *entity,int selftest);
#endif
