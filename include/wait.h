#include <exec/io.h>
struct AHWait {struct MsgPort *port;struct IORequest *request;int opened;};
void ah_wait(struct AHWait *w,unsigned long signals);
unsigned long ah_wait_ticks(struct AHWait *w,unsigned long signals,unsigned long ticks);
void ah_wait_close(struct AHWait *w);
unsigned long ah_elapsed100(unsigned long *stamp);
void ah_port_wait(struct MsgPort *port,unsigned long extra,unsigned long ticks);
