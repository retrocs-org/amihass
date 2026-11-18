from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "control_summary.h"
int main(void){struct AHControls c;char text[96];memset(&c,0,sizeof(c));ah_control_summary("light.desk",&c,0,text);assert(strstr(text,"Checking"));c.brightness=c.color=1;ah_control_summary("light.desk",&c,1,text);assert(strstr(text,"On/off")&&strstr(text,"brightness")&&strstr(text,"RGB color"));memset(&c,0,sizeof(c));c.temperature=1;c.minimum=800;c.maximum=3000;strcpy(c.unit,"C");ah_control_summary("climate.hall",&c,1,text);assert(!strcmp(text,"Temperature 8 to 30 C"));memset(&c,0,sizeof(c));ah_control_summary("sensor.room",&c,1,text);assert(strstr(text,"Read only"));puts("Capabilities: unknown, on/off, brightness/RGB, climate range/unit and read-only summaries passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),'-I'+str(root/'SDK'),str(p/'test.c'),str(root/'src/core.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
