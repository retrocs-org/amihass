from pathlib import Path
import struct,zlib
import palette_art as art
import workbench_icon as wb
ROOT=Path(__file__).resolve().parents[1]
c=art.Canvas();b=c.box;l=c.line;f=c.frame
l(3,13,16,2,3);l(16,2,29,13,3);f(7,12,19,13,2);b(10,15,5,5,4);b(18,15,5,8,1);b(19,16,3,6,5)
l(4,27,28,27,4);l(6,26,6,30,2);l(26,26,26,30,2);b(2,25,5,4,3);b(25,25,5,4,5);b(14,25,5,4,6)
(ROOT/'Icons/AmiHASS.png').write_bytes(art.png(c.p));(ROOT/'Icons/AmiHASS.iff').write_bytes(art.ilbm(c.p))
# Reuse AmiRocket's installer artwork; retain AmiHASS launcher metadata.
(ROOT/'Install.info').write_bytes((ROOT/'Icons/installer/Install.info').read_bytes())
(ROOT/'AmiHASS.drawer.info').write_bytes((ROOT/'Icons/installer/Drawer.info').read_bytes())

# Matching native four-pen icons. Shared home badge identifies AmiHASS.
def window_icon():
    c=wb.Canvas(56,40)
    c.rect(5,7,50,34,1)
    c.rect(2,3,48,32,1)
    c.rect(3,4,47,31,0)
    c.line(3,4,47,4,2);c.line(3,4,3,31,2)
    c.rect(5,6,45,11,3)
    c.line(6,6,44,6,2)
    c.rect(6,7,9,9,1);c.rect(7,7,8,8,2)
    c.line(12,7,12,10,1)
    c.rect(41,7,44,9,1);c.line(42,7,43,7,2)
    return c

def home_badge(c):
    c.rect(7,15,25,28,1);c.rect(8,16,24,27,3)
    c.line(10,21,16,15,1);c.line(16,15,22,21,1)
    c.line(10,20,16,14,2);c.line(16,14,22,20,2)
    c.rect(12,20,20,26,2);c.rect(15,22,17,26,1)
    c.pixel(19,21,3)

prefs=window_icon();home_badge(prefs)
for y,knob in [(17,32),(22,40),(27,35)]:
    prefs.line(29,y,44,y,1);prefs.line(29,y+1,44,y+1,2)
    prefs.rect(knob-2,y-2,knob+2,y+2,1)
    prefs.rect(knob-1,y-1,knob+1,y+1,3);prefs.line(knob-1,y-1,knob+1,y-1,2)

browser=window_icon();home_badge(browser)
for y in (16,21,26):
    browser.rect(29,y,31,y+2,3);browser.line(34,y,43,y,1);browser.line(34,y+1,41,y+1,2)
# Bold stepped lens and bevelled handle remain legible at native size.
for y,x1,x2 in [(20,39,44),(21,37,46),(22,36,47),(23,35,48),(24,35,48),(25,35,48),(26,35,48),(27,36,47),(28,37,46),(29,39,44)]:
    browser.line(x1,y,x2,y,1)
for y,x1,x2 in [(22,39,44),(23,37,46),(24,37,46),(25,37,46),(26,38,45),(27,39,44)]:
    browser.line(x1,y,x2,y,3)
browser.line(39,22,44,22,2);browser.line(37,23,37,25,2)
browser.line(43,24,45,26,0)
for d in range(5):browser.line(46+d,28+d,48+d,30+d,1)
browser.line(47,29,52,34,2);browser.line(48,30,53,35,3)

dashboard=window_icon()
for x in (8,27):
    for y in (15,23):
        dashboard.rect(x,y,x+14,y+5,1);dashboard.rect(x+1,y+1,x+13,y+4,3);dashboard.line(x+2,y+1,x+12,y+1,2)
for name,canvas in [('AmiHASSPrefs',prefs),('AmiHASSBrowser',browser),('AmiHASSDashboard',dashboard)]:
    (ROOT/(name+'.info')).write_bytes(wb.disk_object(canvas))
# PNG previews use the same pixels as the classic Workbench .info files.
def preview_png(path,width,height,rows):
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    raw=b''.join(b'\0'+bytes(value for pixel in row for value in pixel) for row in rows)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

pens=[(170,170,170,255),(0,0,0,255),(255,255,255,255),(105,145,185,255)]
board=[[(205,205,205,255)]*512 for _ in range(192)]
for index,(name,canvas) in enumerate([('AmiHASSPrefs',prefs),('AmiHASSBrowser',browser)]):
    rows=[[pens[value] for value in row] for row in canvas.pixels]
    preview_png(ROOT/'Icons'/(name+'.png'),canvas.width,canvas.height,rows)
    for y,row in enumerate(rows):
        for x,pixel in enumerate(row):
            for dy in range(4):
                for dx in range(4):board[16+y*4+dy][16+index*256+x*4+dx]=pixel
preview_png(ROOT/'Icons/AmiHASS-Workbench-icons.png',512,192,board)
