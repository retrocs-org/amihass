from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
int main(void){struct AHDashboardBook b,loaded;struct AmiHASSEntity value;struct AHControls c;char text[96];FILE *f;unsigned int kind,i;ah_dashboard_book_defaults(&b);b.read_only=1;ah_dashboard_sample(&b.pages[0]);b.pages[0].tiles[0].refresh=5;b.pages[0].tiles[0].display=2;b.pages[0].tiles[0].timestamp=1;assert(ah_dashboard_book_save(&b,"new.cfg")&&ah_dashboard_book_load(&loaded,"new.cfg")&&!memcmp(&b,&loaded,sizeof(b)));assert(ah_dashboard_export(&b,"new.ahdash")&&ah_dashboard_import(&loaded,"new.ahdash")&&loaded.read_only&&loaded.pages[0].tiles[0].refresh==5);b.pages[0].tiles[0].refresh=1;assert(!ah_dashboard_book_save(&b,"invalid.cfg"));b.pages[0].tiles[0].refresh=900;b.pages[0].tiles[0].display=4;assert(!ah_dashboard_book_save(&b,"invalid.cfg"));b.pages[0].tiles[0].display=0;b.read_only=2;assert(!ah_dashboard_book_save(&b,"invalid.cfg"));
 f=fopen("old.cfg","wb");fputs("AMIHASSDASH3\n1 0 0\nLegacy\n1 1\nLamp\nlight.desk\nToggle\n",f);fclose(f);assert(ah_dashboard_book_load(&loaded,"old.cfg")&&!loaded.read_only&&!loaded.pages[0].tiles[0].refresh&&!loaded.pages[0].tiles[0].display&&!loaded.pages[0].tiles[0].timestamp);
 memset(&value,0,sizeof(value));memset(&c,0,sizeof(c));strcpy(value.state,"on");c.brightness=c.level_known=1;c.level=42;loaded.pages[0].tiles[0].display=2;ah_dashboard_value(&loaded.pages[0].tiles[0],&value,&c,text);assert(!strcmp(text,"42%"));c.level_known=0;ah_dashboard_value(&loaded.pages[0].tiles[0],&value,&c,text);assert(strstr(text,"unavailable"));loaded.pages[0].tiles[0].display=3;c.temperature=1;c.target=2250;strcpy(c.unit,"C");ah_dashboard_value(&loaded.pages[0].tiles[0],&value,&c,text);assert(!strcmp(text,"22.50 C"));loaded.pages[0].tiles[0].display=1;ah_dashboard_value(&loaded.pages[0].tiles[0],&value,&c,text);assert(!strncmp(text,"on",2));
 for(kind=0;kind<3;++kind){assert(ah_dashboard_template(&loaded.pages[0],kind));for(i=0;i<ah_dashboard_count(&loaded.pages[0]);++i)if(loaded.pages[0].tiles[i].entity[0])assert(amihass_entity_id(loaded.pages[0].tiles[i].entity)&&ah_dashboard_source_matches(loaded.pages[0].tiles[i].entity,1));loaded.read_only=1;assert(ah_dashboard_book_save(&loaded,"template.cfg"));}assert(!ah_dashboard_template(&loaded.pages[0],3));puts("Dashboard options: read-only/refresh/display/timestamp persistence, v3 migration, bounds, display fallbacks and all sample templates passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),'-I'+str(root/'SDK'),str(p/'test.c'),str(root/'src/dashboard_layout.c'),str(root/'src/core.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],cwd=p,check=True)
