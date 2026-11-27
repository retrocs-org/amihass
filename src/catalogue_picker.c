#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>
#include "amihass.h"
#include "nd_panel.h"
#include "catalogue_picker.h"
int ah_catalogue_choose(int kind,const char *current,char id[96],char name[64],int selftest)
{
 struct RoomPage {struct AmiHASSEntity items[AMIHASS_PAGE];const char *labels[AMIHASS_PAGE+2];} *page;
 struct AmiHASSClient client;struct AmiHASSMessage *m;struct NDPanelField fields[2];struct NDPanel *panel;
 static const char *buttons[]={"Previous","Next","Refresh","Select","Close"};
 int action,offset=0,count=0,total=0,loading=1,refresh=0,next=0,ticks=0,started=0,chosen=0,valid=0,i;
 unsigned int selected;char status[96];
 page=AllocVec(sizeof(*page),MEMF_PUBLIC|MEMF_CLEAR);if(!page)return 0;
 memset(&client,0,sizeof(client));strcpy(status,kind==AMIHASS_LABELS ? "Loading labels from the bridge...":"Loading rooms from the bridge...");
 fields[0].label=kind==AMIHASS_LABELS ? "Label":"Room";fields[0].value="";fields[0].limit=ND_PANEL_CHOICE;
 fields[1].label="Status";fields[1].value=status;fields[1].limit=0;
 panel=nd_panel_open(kind==AMIHASS_LABELS ? "AmiHASS - Select label":"AmiHASS - Select room",fields,2,buttons,5);if(!panel){FreeVec(page);return 0;}
 
 page->labels[0]=kind==AMIHASS_LABELS ? "All labels":"All rooms";nd_panel_choices(panel,0,page->labels,0);
 for(;;){
  ++ticks;
  if(loading && client.pending){
   m=amihass_reply(&client);
   if(m){
    if(m->kind!=kind){strcpy(status,"Unexpected room reply - Refresh to retry");loading=0;}
    else if(m->result==AMIHASS_OK && m->offset==offset && m->total<=100000 && m->count<=AMIHASS_PAGE){
     count=m->count;total=m->total;page->labels[0]=kind==AMIHASS_LABELS ? "All labels":"All rooms";
     for(i=0;i<count;++i){page->items[i]=m->items[i];page->items[i].id[95]=page->items[i].name[63]=0;page->labels[i+1]=page->items[i].name;}
     page->labels[count+1]=NULL;selected=0;for(i=0;i<count;++i)if(!strcmp(current,page->items[i].id))selected=i+1;
     nd_panel_choices(panel,0,page->labels,selected);valid=1;loading=0;
     if(count)sprintf(status,kind==AMIHASS_LABELS ? "Labels %d-%d of %d":"Rooms %d-%d of %d",offset+1,offset+count,total);else strcpy(status,kind==AMIHASS_LABELS ? "No labels configured - use All labels":"No rooms configured - use All rooms");
    }else if(m->result==AMIHASS_WAIT || m->result==AMIHASS_BUSY)next=ticks+5;
    else{loading=0;strncpy(status,m->result==AMIHASS_UNSUPPORTED ? "Update the bridge to browse rooms":amihass_problem(m->result),sizeof(status)-1);status[95]=0;}
    FreeVec(m);
   }else if(!client.pending){loading=0;strcpy(status,"Room request timed out - Refresh to retry");}
  }
  if(loading && ticks-started>600){amihass_client_close(&client);loading=0;strcpy(status,"Room request timed out - Refresh to retry");}
  if(loading && !client.pending && ticks>=next){if(!amihass_send(&client,kind,"AmiHASS.Rooms",NULL,NULL,offset,refresh,NULL,NULL)){loading=0;strcpy(status,"Start the bridge in Preferences; All rooms is available");}refresh=0;}
  nd_panel_set(panel,1,status);nd_panel_enable(panel,100,valid&&!loading&&offset>0);nd_panel_enable(panel,101,valid&&!loading&&offset+count<total);nd_panel_enable(panel,102,!loading);
  selected=nd_panel_choice(panel,0);nd_panel_enable(panel,103,selected==0 || (valid&&!loading&&selected<=(unsigned int)count));
  action=nd_panel_event(panel);
  if(selftest){if(action==-1||action==104)break;if(!loading){action=-1;if(valid)for(i=0;i<count;++i)if(!strcmp(page->items[i].id,kind==AMIHASS_LABELS ? "lighting":"study")){nd_panel_choices(panel,0,page->labels,i+1);action=103;break;}}else action=0;}
  if(action==-1 || action==104)break;
  if(action==103){selected=nd_panel_choice(panel,0);if(selected==0){id[0]=name[0]=0;chosen=1;break;}if(valid&&!loading&&selected<=(unsigned int)count){if(kind==AMIHASS_LABELS&&strlen(page->items[selected-1].id)>63)break;strcpy(id,page->items[selected-1].id);strcpy(name,page->items[selected-1].name);chosen=1;break;}}
  if((action==100 && offset>0) || (action==101 && valid && offset+count<total) || action==102){
   if(action==100)offset-=AMIHASS_PAGE;else if(action==101)offset+=AMIHASS_PAGE;
   amihass_client_close(&client);loading=1;valid=0;count=0;refresh=action==102;started=ticks;next=ticks;strcpy(status,kind==AMIHASS_LABELS ? "Loading labels from the bridge...":"Loading rooms from the bridge...");page->labels[1]=NULL;nd_panel_choices(panel,0,page->labels,0);
  }
  nd_panel_wait(panel,amihass_client_signal(&client),5);
 }
 amihass_client_close(&client);nd_panel_close(panel);FreeVec(page);return chosen;
}
