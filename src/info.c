/* App identity and purpose; no network access or settings changes. */
#include <proto/dos.h>
#include "preferences.h"
#include "nd_panel.h"
unsigned int amihass_about_fields(struct NDPanelField *fields)
{
 fields[0].label="App";fields[0].value="AmiHASS 0.3";fields[0].limit=0;
 fields[1].label="Purpose";fields[1].value="Home Assistant bridge for Amiga apps.";fields[1].limit=0;
 fields[2].label="Author";fields[2].value="NovaDock contributors";fields[2].limit=0;
 fields[3].label="Website";fields[3].value="https://amihass.retrocs.org";fields[3].limit=0;
 fields[4].label="GitHub";fields[4].value="https://github.com/retrocs-org/amihass";fields[4].limit=0;
 return 5;
}
void amihass_about_ui(void)
{
 struct NDPanelField fields[5];struct NDPanel *panel;int action;static const char *buttons[]={"Close"};
 amihass_about_fields(fields);
 panel=nd_panel_open("About AmiHASS",fields,5,buttons,1);if(!panel)return;
 for(;;){action=nd_panel_event(panel);if(action==-1 || action==100)break;nd_panel_wait(panel,0,50);}
 nd_panel_close(panel);
}
