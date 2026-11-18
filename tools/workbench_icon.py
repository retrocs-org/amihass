#!/usr/bin/env python3
"""Generate NovaDock's palette-safe classic Workbench WBTOOL icons.

The output is the original planar DiskObject format understood by icon.library
on AmigaOS 3.x.  It deliberately avoids OS 3.5 colour-icon extensions so the
same artwork is used on both old and new Workbench installations.
"""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
CHECK_ONLY = False
STALE = []


def write_asset(path, payload):
    if CHECK_ONLY:
        if not path.is_file() or path.read_bytes() != payload:
            STALE.append(str(path.relative_to(ROOT)))
    else:
        path.write_bytes(payload)

WIDTH = 54
HEIGHT = 22


class Canvas:
    def __init__(self, width=WIDTH, height=HEIGHT):
        self.width = width
        self.height = height
        self.pixels = [[0 for _ in range(width)] for _ in range(height)]

    def pixel(self, x, y, colour):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.pixels[y][x] = colour

    def rect(self, x1, y1, x2, y2, colour):
        for y in range(y1, y2 + 1):
            for x in range(x1, x2 + 1):
                self.pixel(x, y, colour)

    def line(self, x1, y1, x2, y2, colour):
        dx = abs(x2 - x1)
        sx = 1 if x1 < x2 else -1
        dy = -abs(y2 - y1)
        sy = 1 if y1 < y2 else -1
        error = dx + dy
        while True:
            self.pixel(x1, y1, colour)
            if x1 == x2 and y1 == y2:
                return
            twice = 2 * error
            if twice >= dy:
                error += dy
                x1 += sx
            if twice <= dx:
                error += dx
                y1 += sy


def dock_art():
    c = Canvas()
    # Chamfered dock body and a bright lower accent.
    c.rect(2, 5, 51, 19, 1)
    c.rect(4, 6, 49, 17, 2)
    for x, y in ((2, 5), (3, 5), (50, 5), (51, 5),
                 (2, 19), (3, 19), (50, 19), (51, 19)):
        c.pixel(x, y, 0)
    c.line(4, 18, 49, 18, 3)
    # Four launch tiles; the N-shaped centre pair forms a small brand cue.
    for x1 in (7, 18, 29, 40):
        c.rect(x1, 8, x1 + 7, 15, 1)
        c.rect(x1 + 1, 9, x1 + 6, 14, 3)
    c.line(20, 13, 20, 10, 2)
    c.line(20, 10, 25, 14, 2)
    c.line(25, 14, 25, 10, 2)
    return c


def prefs_art():
    c = Canvas()
    # A compact preferences window with three clearly different sliders.
    c.rect(3, 2, 50, 20, 1)
    c.rect(5, 4, 48, 18, 3)
    c.rect(5, 4, 48, 7, 2)
    c.rect(44, 5, 47, 6, 1)
    for y in (10, 14, 18):
        c.line(9, y, 44, y, 1)
    c.rect(15, 8, 19, 12, 2)
    c.rect(34, 12, 38, 16, 2)
    c.rect(24, 16, 28, 19, 2)
    return c


def music_art():
    """Launcher-sized retro face wearing headphones in four Workbench pens."""
    c = Canvas(48, 38)
    # Headphone band behind the head. Blocky stepped curves deliberately echo
    # classic late-1980s Amiga icon artwork rather than modern antialiasing.
    c.rect(9, 13, 11, 22, 1)
    c.rect(11, 8, 13, 14, 1)
    c.rect(14, 5, 17, 9, 1)
    c.rect(18, 3, 29, 5, 1)
    c.rect(30, 5, 33, 9, 1)
    c.rect(34, 8, 36, 14, 1)
    c.rect(36, 13, 38, 22, 1)
    c.rect(12, 9, 13, 15, 2)
    c.rect(15, 6, 18, 7, 2)
    c.rect(19, 4, 28, 4, 2)
    c.rect(29, 6, 32, 7, 2)
    c.rect(34, 9, 35, 15, 2)

    # Rounded pixel head: dark outline, warm mid-tone hair and a light face.
    c.rect(18, 6, 29, 6, 1)
    c.rect(15, 7, 32, 10, 1)
    c.rect(13, 11, 34, 22, 1)
    c.rect(15, 23, 32, 28, 1)
    c.rect(18, 29, 29, 31, 1)
    c.rect(17, 8, 30, 11, 2)
    c.rect(15, 12, 32, 14, 2)
    c.rect(14, 15, 33, 21, 3)
    c.rect(16, 22, 31, 26, 3)
    c.rect(18, 27, 29, 29, 3)
    # Asymmetric fringe gives the face a little character at low resolution.
    c.rect(15, 11, 30, 13, 2)
    c.rect(15, 13, 19, 16, 2)
    c.rect(28, 12, 32, 14, 2)

    # Ear cups sit in front of the head and include a bright bevel.
    c.rect(6, 15, 13, 27, 1)
    c.rect(8, 17, 12, 25, 2)
    c.rect(9, 18, 11, 22, 3)
    c.rect(34, 15, 41, 27, 1)
    c.rect(35, 17, 39, 25, 2)
    c.rect(36, 18, 38, 22, 3)

    # Eyes, nose and a friendly two-step smile.
    c.rect(20, 17, 21, 18, 1)
    c.rect(27, 17, 28, 18, 1)
    c.rect(24, 19, 25, 22, 2)
    c.pixel(23, 22, 1)
    c.line(20, 24, 22, 26, 1)
    c.line(22, 26, 27, 26, 1)
    c.line(27, 26, 29, 24, 1)

    # Small shoulders make the glyph read as a character rather than a mask.
    c.rect(14, 32, 33, 33, 1)
    c.rect(11, 34, 36, 36, 1)
    c.rect(15, 33, 32, 34, 2)
    c.rect(13, 35, 34, 35, 2)
    c.rect(20, 31, 27, 33, 3)
    return c


def running_apps_art():
    """Launcher-style 2x2 application grid in classic Workbench colours."""
    c = Canvas(48, 38)
    c.rect(5, 4, 42, 33, 1)
    c.rect(7, 6, 40, 31, 2)
    c.rect(8, 7, 39, 30, 3)
    for index, (x, y) in enumerate(((10, 9), (25, 9), (10, 20), (25, 20))):
        c.rect(x, y, x + 11, y + 8, 1)
        c.rect(x + 1, y + 1, x + 10, y + 7, 3)
        c.rect(x + 1, y + 1, x + 10, y + 2,
               2 if index % 2 == 0 else 1)
        c.rect(x + 2, y + 4, x + 4, y + 6, 2)
        c.rect(x + 6, y + 4, x + 9, y + 4, 2)
        c.rect(x + 6, y + 6, x + 9, y + 6, 2)
    c.line(8, 31, 39, 31, 3)
    return c


def installer_art():
    c = Canvas()
    # A package box with a downward arrow: "install NovaDock".
    c.rect(14, 9, 39, 20, 1)
    c.rect(16, 11, 37, 19, 2)
    c.line(14, 9, 19, 5, 1)
    c.line(39, 9, 34, 5, 1)
    c.line(19, 5, 34, 5, 1)
    c.rect(20, 6, 33, 8, 3)
    c.rect(25, 0, 28, 10, 3)
    for i in range(5):
        c.line(22 + i, 10 + i, 31 - i, 10 + i, 3)
    c.line(18, 15, 35, 15, 1)
    return c


def drawer_art():
    c = Canvas()
    # A classic drawer front with a handle and a small dock strip on top.
    c.rect(8, 6, 45, 20, 1)
    c.rect(10, 8, 43, 18, 2)
    c.rect(23, 11, 30, 13, 1)
    c.rect(12, 2, 41, 5, 1)
    for x1 in (14, 21, 28, 35):
        c.rect(x1, 3, x1 + 4, 4, 3)
    return c


def guide_art():
    c = Canvas()
    # An open book: two pages, a spine and lines of text.
    c.rect(9, 3, 26, 19, 1)
    c.rect(27, 3, 44, 19, 1)
    c.rect(10, 4, 25, 18, 2)
    c.rect(28, 4, 43, 18, 2)
    c.line(26, 3, 26, 20, 3)
    c.line(27, 3, 27, 20, 3)
    for y in (7, 10, 13, 16):
        c.line(12, y, 23, y, 1)
        c.line(30, y, 41, y, 1)
    c.rect(30, 6, 35, 8, 3)
    return c


def amiga_string(text):
    raw = text.encode('latin-1') + b'\0'
    return struct.pack('>I', len(raw)) + raw


def drawer_data():
    # struct OldDrawerData: the drawer window (a NewWindow) and its scroll
    # position; Workbench sizes the window itself on first open.
    return struct.pack('>hhhhBBIIIIIIIhhHHHii',
                       40, 30, 420, 160,      # LeftEdge, TopEdge, Width, Height
                       0xFF, 0xFF,            # DetailPen, BlockPen
                       0, 0, 0, 0, 0, 0, 0,   # IDCMP, Flags, pointers (unused)
                       90, 40, 0xFFFF, 0xFFFF,  # min/max size
                       1,                     # WBENCHSCREEN
                       0, 0)                  # dd_CurrentX, dd_CurrentY


def disk_object(canvas, kind=3, default_tool=None, tool_types=()):
    """KIND 3 is WBTOOL, 4 WBPROJECT, 2 WBDRAWER.  A project names the
    program that opens it (DEFAULT_TOOL) and may carry TOOL_TYPES; both are
    stored as length-prefixed strings after the image data.  A drawer
    carries its window (OldDrawerData) right after the DiskObject and the
    OS 2.x view settings (flags, view modes) at the very end."""
    # Serialized DiskObject, followed by one planar Image and its two planes.
    data = bytearray(78)
    struct.pack_into('>HH', data, 0, 0xE310, 1)  # magic, version
    struct.pack_into('>I', data, 4, 0)           # NextGadget
    struct.pack_into('>hhhh', data, 8, 0, 0, canvas.width, canvas.height)
    struct.pack_into('>HHH', data, 16, 0x0004, 0x0001, 0x0001)
    struct.pack_into('>I', data, 22, 1)           # GadgetRender is present
    struct.pack_into('>I', data, 26, 0)           # algorithmic selected image
    struct.pack_into('>I', data, 44, 1)           # disk revision marker
    data[48] = kind                               # WBTOOL / WBPROJECT
    if default_tool:
        struct.pack_into('>I', data, 50, 1)       # do_DefaultTool present
    if tool_types:
        struct.pack_into('>I', data, 54, 1)       # do_ToolTypes present
    struct.pack_into('>II', data, 58, 0x80000000, 0x80000000)
    struct.pack_into('>I', data, 74, 16384)
    if kind == 2:
        struct.pack_into('>I', data, 66, 1)       # do_DrawerData present
        struct.pack_into('>I', data, 74, 0)       # drawers have no stack
        data.extend(drawer_data())
    data.extend(struct.pack('>hhhhhIBBI', 0, 0, canvas.width, canvas.height, 2,
                            1, 3, 0, 0))
    row_words = (canvas.width + 15) // 16
    for plane in range(2):
        for y in range(canvas.height):
            for word_index in range(row_words):
                word = 0
                for bit in range(16):
                    x = word_index * 16 + bit
                    if x < canvas.width and canvas.pixels[y][x] & (1 << plane):
                        word |= 1 << (15 - bit)
                data.extend(struct.pack('>H', word))
    if default_tool:
        data.extend(amiga_string(default_tool))
    if tool_types:
        data.extend(struct.pack('>I', (len(tool_types) + 1) * 4))
        for entry in tool_types:
            data.extend(amiga_string(entry))
    if kind == 2:
        data.extend(struct.pack('>IH', 0, 0))    # DDFLAGS_SHOWDEFAULT, DDVM_BYDEFAULT
    return bytes(data)


