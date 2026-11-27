/* Correlate delivery with the originating client/entity; another client's
 * successful press must never confirm this client's failed request. */
static int press_reserve(struct Backend *b,struct AmiHASSMessage *m,unsigned long now)
{int i,slot=-1;for(i=0;i<32;++i){struct PressReceipt *p=&b->press_receipts[i];if(p->port==m->msg.mn_ReplyPort&&!strcmp(p->entity,m->entity)){slot=i;break;}if(!p->ticket||now<p->seen||now-p->seen>120)slot=i;}if(slot<0)return -1;{struct PressReceipt *p=&b->press_receipts[slot];memset(p,0,sizeof(*p));p->port=m->msg.mn_ReplyPort;strcpy(p->entity,m->entity);p->seen=now;p->ticket=++b->press_serial;if(!p->ticket)p->ticket=++b->press_serial;return slot;}}
static void press_finish(struct Backend *b,unsigned long ticket,int success)
{int i;for(i=0;i<32;++i)if(b->press_receipts[i].ticket==ticket){b->press_receipts[i].done=success?1:-1;return;}}
static unsigned long press_value(struct Backend *b,struct MsgPort *port,const char *entity)
{int i;for(i=0;i<32;++i)if(b->press_receipts[i].port==port&&!strcmp(b->press_receipts[i].entity,entity)&&b->press_receipts[i].done==1)return b->press_receipts[i].ticket;return 0;}
static void press_release(struct Backend *b,struct MsgPort *port)
{int i;for(i=0;i<32;++i)if(b->press_receipts[i].port==port)memset(&b->press_receipts[i],0,sizeof(b->press_receipts[i]));}
