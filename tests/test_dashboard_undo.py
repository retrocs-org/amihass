"""Drive production undo and page switching with lightweight UI adapters."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/dashboard.c').read_text();a=s.index('static void change_page(');b=s.index('static int self_check(',a)
code=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
struct Dash {struct AHDashboardLayout layout;struct AHDashboardBook book,*undo;int undo_dirty,dirty,selected,round;unsigned int row_offset;int results[24],received[24];struct AHControls controls[24];unsigned long status_due;char status[160];};
static int mode;static void *AllocVec(size_t n,int flags){(void)flags;return calloc(1,n);}static void FreeVec(void *p){free(p);}static void pause_client(struct Dash *d){(void)d;}static void ui_close(struct Dash *d){(void)d;}static int ui_open(struct Dash *d){(void)d;return 1;}static void update(struct Dash *d){(void)d;}
static void edit_tile(struct Dash *d){if(mode){strcpy(d->layout.tiles[0].label,"Changed");d->dirty=1;}}
static void edit_layout(struct Dash *d){edit_tile(d);}static void control_dialog(struct Dash *d){(void)d;}static void edit_pages(struct Dash *d){d->book.current=1;d->layout=d->book.pages[1];}
'''+s[a:b]+r'''
int main(void){struct Dash d;struct AHDashboardBook *saved;memset(&d,0,sizeof(d));ah_dashboard_book_defaults(&d.book);ah_dashboard_sample(&d.book.pages[0]);assert(ah_dashboard_page_add(&d.book,"Kitchen"));d.book.current=0;d.layout=d.book.pages[0];mode=1;edit_with_undo(&d,1);assert(d.undo&&d.dirty&&!strcmp(d.layout.tiles[0].label,"Changed"));saved=d.undo;mode=0;edit_with_undo(&d,1);assert(d.undo==saved);edit_with_undo(&d,3);assert(d.undo==saved&&d.book.current==1);undo_edit(&d);assert(!d.undo&&!d.dirty&&d.book.current==0&&!strcmp(d.layout.tiles[0].label,"Living room"));mode=1;edit_with_undo(&d,1);d.dirty=0;d.undo_dirty=1;undo_edit(&d);assert(d.dirty&&!strcmp(d.layout.tiles[0].label,"Living room"));change_page(&d,1);assert(d.book.current==1&&!d.layout.tiles[0].entity[0]);change_page(&d,0);assert(!strcmp(d.layout.tiles[0].label,"Living room"));assert(!ah_dashboard_source_matches("light.amihass_sample_desk",0)&&ah_dashboard_source_matches("light.amihass_sample_desk",1));assert(ah_dashboard_source_matches("light.real",0)&&!ah_dashboard_source_matches("light.real",1));puts("Dashboard undo: committed/cancelled edits, navigation, saved-state dirty handling, page cache reset and source checks passed.");return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run([os.environ.get('CC','cc'),'-std=c99','-I'+str(root/'include'),'-I'+str(root/'SDK'),str(p/'test.c'),str(root/'src/dashboard_layout.c'),str(root/'src/core.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
