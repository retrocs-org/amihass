"""Exercise the production paged label selector using scripted bridge replies."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1];source=(root/'tests/test_room_picker.py').read_text();scope={'__file__':str(root/'tests/test_room_picker.py')}
exec(compile(source[:source.index('with tempfile.TemporaryDirectory()')],str(root/'tests/test_room_picker.py'),'exec'),scope)
prefix=scope['prefix'].replace('AMIHASS_AREAS','AMIHASS_LABELS').replace('All rooms','All labels').replace('"study"','"lighting"').replace('"Study"','"Lighting"')
code=scope['code'].split('/* One 16-room page')[0]
production=(root/'src/catalogue_picker.c').read_text();production=production[production.index('int ah_catalogue_choose('):]
code+=production+'\nstatic int choose_room(struct Browser *b,int check){int ok=ah_catalogue_choose(AMIHASS_LABELS,b->room_id,b->room_id,b->room_name,check);if(ok)set_room(b,b->room_id,b->room_name);return ok;}\n'
main=scope['main'].replace('"study"','"lighting"').replace('"Study"','"Lighting"').replace('Room picker:','Label picker:')
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'exec').mkdir();(p/'exec/ports.h').write_text('typedef unsigned long ULONG;typedef long LONG;struct MsgPort;struct Message {unsigned short mn_Length;struct MsgPort *mn_ReplyPort;};\n');(p/'test.c').write_text(prefix+code+main)
    command=[os.environ.get('CC','cc'),'-std=c99','-Wno-pointer-to-int-cast','-I'+str(p),'-I'+str(root/'SDK'),'-I'+str(root/'include'),str(p/'test.c'),'-DAH_HOST_TEST','-include','ah_locale.h',str(root/'src/ah_locale.c'),'-o',str(p/'test')]
    if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
    subprocess.run(command,check=True);subprocess.run([str(p/'test')],check=True,timeout=10)
