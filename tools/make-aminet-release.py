#!/usr/bin/env python3
"""Prepare an ASCII Aminet description for the exact verified runtime LHA."""
import argparse,re,textwrap
from pathlib import Path
from release_support import ReleaseError,version
ROOT=Path(__file__).resolve().parents[1]

def readme(author,uploader,archive_name):
    value=version(ROOT)
    if not re.fullmatch(r'[^\s@<>]+@[^\s@<>]+\.[^\s@<>]+',uploader) or any(c in author for c in '\r\n\0'):raise ReleaseError('Aminet needs a single-line author and uploader email')
    text=(ROOT/'CHANGELOG.MD').read_text();match=re.search(r'^## AmiHASS '+re.escape(value)+r'\n(.*?)(?=^## |\Z)',text,re.M|re.S)
    if not match:raise ReleaseError('Missing matching changelog section')
    changes=match.group(1).strip().replace('### ','').replace('`','')
    result=f'''Short:        Home Assistant bridge for classic AmigaOS
Author:       {author}
Uploader:     {uploader}
Type:         util/wb
Version:      {value}
Architecture: m68k-amigaos >= 3.0.0
Requires:     AmigaOS 3.x, 68000+, Installer 42+, 640x256
Distribution: Aminet
URL:          https://amihass.retrocs.org/

AmiHASS shares one Home Assistant connection between Amiga applications.
The package includes Preferences, Device Browser, Dashboard and ARexx examples.
Server data needs TCP/IP, curl with HTTPS, CA certificates and a valid token.
Shared sample devices work offline. The separate SDK ZIP contains C client
source, headers and a working compiled example.

Unpack {archive_name}, open AmiHASS and run Install-AmiHASS.
Read the final installer page, then open the installed AmiHASSPrefs manually.
Updates preserve settings and restore the previous service running state.
Uninstall can keep or remove settings. See README.txt and Docs/USAGE.md.

Source: https://github.com/retrocs-org/amihass
Licence: MIT; see LICENSE and NOTICE.

{changes}
'''
    for a,b in [('—','-'),('–','-'),('’',"'"),('“','"'),('”','"')]:result=result.replace(a,b)
    return result.encode('ascii','strict')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--author',default='NovaDock contributors');p.add_argument('--uploader',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.write_bytes(readme(a.author,a.uploader,'AmiHASS.lha'))
