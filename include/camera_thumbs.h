#ifndef AH_CAMERA_THUMBS_H
#define AH_CAMERA_THUMBS_H
struct AHCameraThumbs;struct Screen;struct RastPort;
struct AHCameraThumbs *ah_thumbs_new(struct Screen*);
void ah_thumbs_limit(struct AHCameraThumbs*,int);
void ah_thumbs_free(struct AHCameraThumbs*);
int ah_thumbs_request(struct AHCameraThumbs*,int,const char*);
int ah_thumbs_step(struct AHCameraThumbs*);
int ah_thumbs_busy(struct AHCameraThumbs*);
int ah_thumbs_draw(struct AHCameraThumbs*,int,struct RastPort*,int,int);
unsigned long ah_thumbs_age(struct AHCameraThumbs*,int);
const char *ah_thumbs_status(struct AHCameraThumbs*);
#endif
