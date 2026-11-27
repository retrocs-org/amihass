"""AmigaGuide manual: current with the docs, balanced nodes, every link resolves, Latin-1 only."""
from pathlib import Path
import re,subprocess,sys
root=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(root/'tools/make_guide.py'),'--check'],check=True)
data=(root/'docs/AmiHASS.guide').read_bytes();text=data.decode('latin-1')
assert text.startswith('@database ') and data.isascii() or all(b<256 for b in data)
nodes=re.findall(r'^@node (\S+)',text,re.M);assert len(nodes)==text.count('\n@endnode') and len(set(n.lower() for n in nodes))==len(nodes) and nodes[0]=='Main'
for target in re.findall(r'@\{"[^"]*" link (\S+?)\}',text):assert target in nodes,target
for target in re.findall(r'^@(?:next|prev|toc) (\S+)',text,re.M):assert target in nodes,target
assert not re.search(r'(?<!\\)@(?!\{|node|endnode|database|\$VER|author|wordwrap|next|prev|toc|\\)',text.replace('\\@','')),'unescaped @'
print('AmigaGuide: current, %d nodes, all links resolve.'%len(nodes))
