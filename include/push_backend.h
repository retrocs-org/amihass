/* Included after the scheduler's helpers; owns no sockets in this process. */
/* Persistent notifications arrive over the live-update connection; it is kept
 * open for them while a client asked within the last two minutes. */
static int notes_wanted(struct Backend *b,unsigned long now){return b->notes_asked&&now>=b->notes_asked&&now-b->notes_asked<120UL;}
static void notes_apply(struct Backend *b,int op,const struct AHNote *n)
{int i;b->notes_known=1;++b->notes_serial;if(op==AH_NOTE_CLEAR){b->notes_count=0;return;}
 for(i=0;i<b->notes_count;++i)if(!strcmp(b->notes[i].id,n->id))break;
 if(op==AH_NOTE_REMOVE){if(i<b->notes_count){memmove(&b->notes[i],&b->notes[i+1],(unsigned int)(b->notes_count-i-1)*sizeof(b->notes[0]));--b->notes_count;}return;}
 if(i==b->notes_count){if(b->notes_count==AH_NOTES){memmove(&b->notes[0],&b->notes[1],(AH_NOTES-1)*sizeof(b->notes[0]));--i;}else ++b->notes_count;}/* the oldest makes room */
 b->notes[i]=*n;}
static void push_fallback(struct Backend *b,unsigned long now)
{int i;b->push_ready=0;for(i=0;i<SELECTED;++i)if(b->subscriptions[i].pushed){b->subscriptions[i].pushed=0;b->subscriptions[i].due=now;if(b->subscriptions[i].result==AMIHASS_OK)b->subscriptions[i].result=AMIHASS_STALE;}}
static void push_events(struct Backend *b)
{struct AHPushMessage *m;int i;unsigned long now=seconds();if(!b->push_port)return;while((m=(struct AHPushMessage*)GetMsg(b->push_port))!=NULL){if(m->msg.mn_Length!=sizeof(*m)||m->magic!=AH_PUSH_MAGIC){nd_reply_finish((struct Message*)m);continue;}m->stop=b->manual_paused||b->stopping||!b->push_enabled||b->low_memory||b->sample_mode||b->auth_blocked||!b->network||!b->configured||(m->generation&&m->generation!=b->generation);if(!m->stop){b->push_seen=now;if(m->kind==1){b->push_ready=m->ready;m->generation=b->generation;m->count=0;for(i=0;i<SELECTED;++i){if(b->subscriptions[i].watching&&active_subscription(&b->subscriptions[i],now)&&m->count<AH_PUSH_SELECTED){strcpy(m->ids[m->count++],b->subscriptions[i].entity);if(b->push_ready&&b->subscriptions[i].pushed)b->subscriptions[i].updated=now;}else if(b->subscriptions[i].pushed){b->subscriptions[i].pushed=0;b->subscriptions[i].due=now;} /* released from push: REST owns it again */}if(!m->count&&!notes_wanted(b,now))m->stop=1;if(b->push_ready){b->connected=1;b->last_success=now;}}
 else if(m->kind==2){m->entity.id[95]=0;for(i=0;i<SELECTED;++i)if(b->subscriptions[i].watching&&!strcmp(b->subscriptions[i].entity,m->entity.id)){struct Subscription *s=&b->subscriptions[i];s->value=m->entity;s->controls=m->controls;s->updated=now;s->result=amihass_available(&s->value)?AMIHASS_OK:AMIHASS_UNAVAILABLE;s->pushed=1;++b->push_updates;b->push_last_update=now;break;}}
 else if(m->kind==4){m->note.id[63]=m->note.title[63]=m->note.message[255]=m->note.created[39]=0;notes_apply(b,m->note_op,&m->note);}
 else if(m->kind==3){push_fallback(b,now);b->push_error=m->error ? 1:2;++b->push_failures;b->push_retry=now+(b->retry_interval<60?60:b->retry_interval);if(m->error){b->last_error=AH_TEST_AUTH;b->auth_blocked=1;connection_failed(b,now);}}}nd_reply_finish((struct Message*)m);}}
static void push_step(struct Backend *b,const char *program,unsigned long now)
{char quoted[1100],command[1200];BPTR in,out;int i,selected=0;LONG rc;if(!b->push_port)return;
 if(b->push_ready&&(!present(AH_PUSH_GUARD)||now<b->push_seen||now-b->push_seen>5))push_fallback(b,now);
 if(b->manual_paused||!b->push_enabled||b->sample_mode||b->low_memory||b->auth_blocked||!b->configured||!b->network||b->stopping){if(b->push_ready)push_fallback(b,now);return;}
 for(i=0;i<SELECTED;++i)if(b->subscriptions[i].watching&&active_subscription(&b->subscriptions[i],now))++selected;
 if((!selected&&!notes_wanted(b,now))||(b->push_retry&&!b->reconnect)||now<b->push_retry||present(AH_PUSH_GUARD)||now<b->push_launch+5)return;
 ++b->push_attempts;b->push_launch=now;if(!nd_quote_path(quoted,sizeof(quoted),program))return;sprintf(command,"%s PUSH %s",quoted,b->push_name);in=Open("NIL:",MODE_OLDFILE);out=Open("NIL:",MODE_NEWFILE);if(!in||!out){if(in)Close(in);if(out)Close(out);return;}rc=SystemTags(command,SYS_Input,in,SYS_Output,out,SYS_Asynch,TRUE,NP_StackSize,65536UL,TAG_DONE);if(rc<0){Close(in);Close(out);b->push_retry=now+60;}}
