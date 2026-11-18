#include <exec/io.h>
struct AHWait {struct MsgPort *port;struct IORequest *request;int opened;};
void ah_wait(struct AHWait *w,unsigned long signals);
void ah_wait_close(struct AHWait *w);
