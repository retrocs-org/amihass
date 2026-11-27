"""Per-page refresh profiles and time-of-day pages: storage, defaults, rename/prune and hour windows."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard_profile.h"
int main(void){static struct AHPageProfiles p,q;static struct AHDashboardBook b;struct AHPageProfile e;FILE *f;memset(&p,0,sizeof(p));memset(&b,0,sizeof(b));
 b.count=3;strcpy(b.pages[0].name,"Home");strcpy(b.pages[1].name,"Night");strcpy(b.pages[2].name,"Work");
 e=ah_profile_get(&p,"Home");assert(e.profile==AH_PROFILE_NORMAL&&e.from==AH_PROFILE_NEVER&&!strcmp(e.name,"Home"));
 e.profile=AH_PROFILE_SLOW;assert(ah_profile_set(&p,&e)&&p.count==1);
 strcpy(e.name,"Night");e.profile=AH_PROFILE_VERY_SLOW;e.from=22;e.until=7;assert(ah_profile_set(&p,&e));strcpy(e.name,"Work");e.profile=AH_PROFILE_FAST;e.from=9;e.until=17;assert(ah_profile_set(&p,&e)&&p.count==3);
 assert(ah_profile_page_for_hour(&p,&b,23)==1&&ah_profile_page_for_hour(&p,&b,3)==1&&ah_profile_page_for_hour(&p,&b,7)==-1&&ah_profile_page_for_hour(&p,&b,9)==2&&ah_profile_page_for_hour(&p,&b,17)==-1&&ah_profile_page_for_hour(&p,&b,24)==-1);
 assert(ah_profile_seconds(AH_PROFILE_FAST)==5&&ah_profile_seconds(AH_PROFILE_SLOW)==60&&ah_profile_seconds(AH_PROFILE_VERY_SLOW)==300&&!ah_profile_seconds(AH_PROFILE_NORMAL));
 assert(ah_profiles_save(&p,"pages.cfg")&&ah_profiles_load(&q,"pages.cfg")&&q.count==3&&!memcmp(&p,&q,sizeof(p)));assert(ah_profiles_save(&p,"pages.cfg")&&ah_profiles_load(&q,"pages.cfg")&&q.count==3);
 ah_profile_rename(&p,"Work","Office");assert(ah_profile_get(&p,"Office").from==9&&ah_profile_page_for_hour(&p,&b,10)==-1);strcpy(b.pages[2].name,"Office");assert(ah_profile_page_for_hour(&p,&b,10)==2);
 b.count=2;ah_profile_prune(&p,&b);assert(p.count==2&&ah_profile_get(&p,"Office").from==AH_PROFILE_NEVER);
 e=ah_profile_get(&p,"Home");e.profile=AH_PROFILE_NORMAL;assert(ah_profile_set(&p,&e)&&p.count==1);/* back at defaults: dropped */
 e.from=25;assert(!ah_profile_set(&p,&e));e.from=3;e.until=3;strcpy(e.name,"Home");assert(ah_profile_set(&p,&e)&&ah_profile_page_for_hour(&p,&b,12)==0);/* all day */
 f=fopen("bad.cfg","wb");fputs("AMIHASSPAGES1\n9 1 2 Home\n1 30 2 Night\n1 2 3 \n2 5 6 Night\n",f);fclose(f);assert(ah_profiles_load(&q,"bad.cfg")&&q.count==1&&!strcmp(q.pages[0].name,"Night")&&q.pages[0].profile==2);
 f=fopen("bad.cfg","wb");fputs("OTHER\n",f);fclose(f);assert(!ah_profiles_load(&q,"bad.cfg")&&!q.count);assert(!ah_profiles_load(&q,"missing.cfg"));
 puts("Dashboard page profiles: defaults, save/load, rename, prune, hour windows across midnight and damaged files passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir();(p/'exec/ports.h').write_text('#include <stdint.h>\ntypedef uint32_t ULONG;typedef int32_t LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'t.c').write_text(program)
    cmd=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'t.c'),str(root/'src/dashboard_profile.c'),'-o',str(p/'t')]
    if os.environ.get('SANITIZE')=='1':cmd+=['-fsanitize=address,undefined']
    subprocess.run(cmd,check=True);subprocess.run([str(p/'t')],cwd=p,check=True)
