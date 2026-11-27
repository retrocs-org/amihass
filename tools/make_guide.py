#!/usr/bin/env python3
"""Build docs/AmiHASS.guide (AmigaGuide) from docs/USAGE.md and ARexx/README.MD.

Each "## " section becomes a node linked from the contents page. --check only
verifies that the guide is current."""
import re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
SOURCES = (('docs/USAGE.md', 'Using AmiHASS'), ('ARexx/README.MD', 'ARexx scripts'))

def latin1(text):
    for a, b in (('—', '-'), ('–', '-'), ('’', "'"), ('‘', "'"), ('“', '"'), ('”', '"'), ('…', '...'), ('→', '->'), ('×', 'x')):
        text = text.replace(a, b)
    return text.encode('latin-1', 'replace').decode('latin-1')

def inline(text):
    text = text.replace('\\', '\\\\').replace('@', '\\@')
    text = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', text)          # links: keep the text
    text = re.sub(r'\*\*([^*]+)\*\*', r'@{b}\1@{ub}', text)         # bold
    text = re.sub(r'`([^`]+)`', r'@{i}\1@{ui}', text)               # code as italics
    return text

def node_name(title, used):
    base = re.sub(r'[^A-Za-z0-9]+', '_', title).strip('_')[:28] or 'Section'
    name, n = base, 2
    while name.lower() in used:
        name = '%s_%d' % (base, n); n += 1
    used.add(name.lower())
    return name

def sections():
    out = []
    for path, part in SOURCES:
        lines = (ROOT / path).read_text(encoding='utf-8').splitlines()
        title, body, fence = part, [], False
        for line in lines:
            if line.startswith('```'):
                fence = not fence; continue
            if not fence and line.startswith('## '):
                if body and any(l.strip() for l in body):
                    out.append((title, body))
                title, body = line[3:].strip(), []
            elif not fence and line.startswith('# '):
                continue
            else:
                body.append(line if fence else line)
        if body and any(l.strip() for l in body):
            out.append((title, body))
    return out

def build():
    used = {'main'}
    parts = [(node_name(t, used), t, b) for t, b in sections()]
    g = ['@database AmiHASS.guide', '@$VER: AmiHASS.guide 0.3 (24.11.2026)', '@author "NovaDock contributors"',
         '@wordwrap', '@node Main "AmiHASS"', '@{b}AmiHASS 0.3@{ub} - Home Assistant for the Amiga', '',
         'Choose a topic:', '']
    for name, title, _ in parts:
        g.append('  @{" %s " link %s}' % (inline(latin1(title)).replace('"', "'"), name))
    g += ['', 'The same text is in Docs/USAGE.md and ARexx/README.MD.', '@endnode']
    for i, (name, title, body) in enumerate(parts):
        nav = []
        if i:
            nav.append('@prev %s' % parts[i - 1][0])
        if i + 1 < len(parts):
            nav.append('@next %s' % parts[i + 1][0])
        g += ['', '@node %s "%s"' % (name, latin1(title).replace('"', "'"))] + nav + ['@toc Main', '@{b}%s@{ub}' % inline(latin1(title)), '']
        for line in body:
            line = latin1(line)
            if line.startswith('### '):
                g.append('@{b}%s@{ub}' % inline(line[4:])); continue
            g.append(inline(line))
        g.append('@endnode')
    return ('\n'.join(g) + '\n').encode('latin-1')

if __name__ == '__main__':
    data = build(); target = ROOT / 'docs/AmiHASS.guide'
    if '--check' in sys.argv:
        if not target.is_file() or target.read_bytes() != data:
            sys.exit('docs/AmiHASS.guide is out of date (run tools/make_guide.py)')
        print('AmigaGuide current: %d nodes' % (data.count(b'\n@node ')))
    else:
        target.write_bytes(data); print('Wrote docs/AmiHASS.guide (%d nodes)' % data.count(b'\n@node '))
