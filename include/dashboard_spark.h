#ifndef AH_DASHBOARD_SPARK_H
#define AH_DASHBOARD_SPARK_H
/* History-graph tiles (display 5): 24 hours of numeric history per tile,
 * fetched one tile at a time on a separate SDK client and kept for five
 * minutes. Values are hundredths; no floating point. */
#include <time.h>
#include "amihass.h"
#include "amihass_core.h"
#include "dashboard.h"
#define AH_SPARK_POINTS 64
#define AH_SPARK_DISPLAY 5 /* 24 hours; 6 = 6 hours, 7 = 1 hour */
#define AH_SPARK_IS(display) ((display)>=5&&(display)<=7)
#define AH_SPARK_HOURS(display) ((display)==7 ? 1:(display)==6 ? 6:24)
#define AH_SPARK_GAP (-2147483647L-1L) /* unavailable or non-numeric */
#define AH_SPARK_SECONDS 300
struct AHSparks {
 struct AmiHASSClient client;int tile,offset,tries,again;time_t asked;
 char entity[AH_DASH_TILES][96];int hours[AH_DASH_TILES];time_t at[AH_DASH_TILES];unsigned long serial[AH_DASH_TILES];int count[AH_DASH_TILES];long value[AH_DASH_TILES][AH_SPARK_POINTS];
 int fill;long fresh[AH_SPARK_POINTS];char source[160];/* data source the graphs came from */
};
void ah_sparks_init(struct AHSparks *s);
/* Drives the fetches; returns 1 when some tile has new points to draw. */
int ah_sparks_step(struct AHSparks *s,const struct AHDashboardLayout *layout,time_t now);
/* Forget every graph (page change, data source change). */
void ah_sparks_reset(struct AHSparks *s);
void ah_sparks_close(struct AHSparks *s);
/* Point i of tile as a y offset 0..height-1 (0 = top), -1 for a gap. */
int ah_sparks_shape(const struct AHSparks *s,unsigned int tile,int height,int *ys,int cap);
/* Lowest and highest value of a tile (hundredths); 0 when it has none. */
int ah_sparks_range(const struct AHSparks *s,unsigned int tile,long *low,long *high);
/* A value as short text: one decimal below 100, whole numbers above. */
void ah_sparks_text(long hundredths,char *out);
unsigned long ah_sparks_signal(const struct AHSparks *s);
int ah_sparks_waiting(const struct AHSparks *s);
#endif
