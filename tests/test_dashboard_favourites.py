"""Bounded source-specific favourites: persistence, damage and failed saves."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "dashboard_favourites.h"
int main(void){struct AHFavourites f,g;struct AHFavourite e;FILE *fp;int i;memset(&f,0,sizeof(f));memset(&e,0,sizeof(e));assert(ah_favourites_load(&g,"favs"));strcpy(e.source,"sample://amihass");strcpy(e.id,"light.amihass_sample_living_room");strcpy(e.name,"Living room");assert(ah_favourite_set(&f,&e));strcpy(e.name,"Renamed");assert(ah_favourite_set(&f,&e)&&f.count==1);strcpy(e.source,"https://local.fixture");assert(ah_favourite_set(&f,&e)&&f.count==2);assert(ah_favourites_save(&f,"favs")&&ah_favourites_load(&g,"favs")&&g.count==2&&!strcmp(g.items[0].name,"Renamed"));assert(strcmp(g.items[0].source,g.items[1].source));assert(!rename("favs","favs.old"));assert(ah_favourites_load(&g,"favs")&&g.count==2);assert(ah_favourites_save(&f,"favs"));mkdir("favs.new",0700);ah_favourite_remove(&f,0);assert(f.count==1&&!ah_favourites_save(&f,"favs"));assert(ah_favourites_load(&g,"favs")&&g.count==2);rmdir("favs.new");assert(ah_favourites_save(&f,"favs")&&ah_favourites_load(&g,"favs")&&g.count==1);for(i=1;i<32;++i){sprintf(e.id,"sensor.test_%d",i);assert(ah_favourite_set(&f,&e));}strcpy(e.id,"sensor.overflow");assert(!ah_favourite_set(&f,&e)&&f.count==32);strcpy(e.id,"bad entity");assert(!ah_favourite_set(&f,&e));fp=fopen("bad","wb");fputs("AMIHASSFAVOURITES1\nsample://amihass\nlight.bad\n",fp);fclose(fp);assert(!ah_favourites_load(&g,"bad")&&!g.count);fp=fopen("bad","wb");fputs("WRONG\n",fp);fclose(fp);assert(!ah_favourites_load(&g,"bad"));puts("Dashboard favourites: duplicate replacement, server/sample isolation, bounded list, add/remove persistence, recovery, failed save and invalid files passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text('#include <unistd.h>\n'+code)
 command=[os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),'-I'+str(root/'SDK'),str(p/'t.c'),str(root/'src/core.c'),'-o',str(p/'t')]
 if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
 subprocess.run(command,check=True);subprocess.run([str(p/'t')],cwd=p,check=True)
