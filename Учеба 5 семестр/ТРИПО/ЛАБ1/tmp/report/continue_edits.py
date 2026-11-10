from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
from lxml import etree as E
from copy import deepcopy
import re, shutil

path=Path('output/Отчет_ЛР1_Защита_ПО.docx')
backup=Path('tmp/report/Отчет_перед_продолжением_правок.docx')
shutil.copy2(path,backup)
ns={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
W='{'+ns['w']+'}'
def text(e):return ''.join(e.itertext()) if False else ''.join(e.xpath('.//w:t/text()',namespaces=ns))
with ZipFile(path) as z: parts={n:z.read(n) for n in z.namelist()}
root=E.fromstring(parts['word/document.xml'])
body=root.find(W+'body')
paras=body.findall(W+'p')
sample_listing=next(p for p in paras if text(p).startswith('Листинг 5 '))
sample_figure=next(p for p in paras if text(p).startswith('Рисунок 1 '))
removed=0
for p in list(body.findall(W+'p')):
    t=text(p)
    if re.match(r'^В листинге \d+ приведён код:',t) or re.match(r'^Результат наблюдения представлен на рисунке \d+\.$',t):
        body.remove(p);removed+=1
listing_num=0
for p in body.findall(W+'p'):
    t=text(p)
    sample=None
    if re.match(r'^Листинг \d+ [—–]',t):
        listing_num+=1
        nodes=p.xpath('.//w:t',namespaces=ns)
        # Prefix is in the first run in both source and edited captions.
        if nodes:nodes[0].text=re.sub(r'^Листинг \d+',f'Листинг {listing_num}',nodes[0].text or '')
        sample=sample_listing
    elif re.match(r'^Рисунок \d+ [—–]',t):sample=sample_figure
    if sample is not None:
        old=p.find(W+'pPr')
        if old is not None:p.remove(old)
        pr=deepcopy(sample.find(W+'pPr'))
        if pr is None:pr=E.Element(W+'pPr')
        # Keep listing captions with code, but figure captions follow the picture.
        if sample is sample_listing:
            keep=E.SubElement(pr,W+'keepNext');keep.set(W+'val','1')
        p.insert(0,pr)
parts['word/document.xml']=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
replaced=0
for n,b in list(parts.items()):
    if n.startswith('word/') and n.endswith('.xml'):
        tree=E.fromstring(b)
        for node in tree.xpath('//w:t',namespaces=ns):
            if node.text and '—' in node.text:
                replaced+=node.text.count('—');node.text=node.text.replace('—','–')
        parts[n]=E.tostring(tree,xml_declaration=True,encoding='UTF-8',standalone=True)
temp=path.with_suffix('.pending.docx')
with ZipFile(temp,'w',ZIP_DEFLATED) as z:
    for n,b in parts.items():z.writestr(n,b)
temp.replace(path)
print('Removed redundant lead-ins:',removed,'; dashes:',replaced,'; listings:',listing_num)
# Verify the user-edited title and added listing survived; image binaries unchanged.
with ZipFile(backup) as old,ZipFile(path) as new:
    assert all(old.read(n)==new.read(n) for n in old.namelist() if n.startswith('word/media/'))
assert 'И.А. Эков' in text(root)
assert 'anti_debug_5' in text(root)
