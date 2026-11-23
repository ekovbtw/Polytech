from zipfile import ZipFile
from lxml import etree as E
from pathlib import Path
import sys
sys.stdout.reconfigure(encoding='utf-8')
ns={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main','a':'http://schemas.openxmlformats.org/drawingml/2006/main','r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
with ZipFile('output/Отчет.docx') as z:
 r=E.fromstring(z.read('word/document.xml'))
 rel=E.fromstring(z.read('word/_rels/document.xml.rels'))
 ids={e.get('Id'):e.get('Target') for e in rel}
 ps=r.xpath('//w:body/w:p',namespaces=ns)
 for i,p in enumerate(ps):
  t=''.join(p.xpath('.//w:t/text()',namespaces=ns))
  imgs=p.xpath('.//a:blip/@r:embed',namespaces=ns)
  if 'рисун' in t.lower() or imgs or p.xpath('.//w:instrText',namespaces=ns):
   print(i,repr(t),[ids[x] for x in imgs],p.xpath('.//w:instrText/text()',namespaces=ns))
   if imgs:
    for id in imgs:
     target=ids[id];name=Path(target).name
     dest=Path('tmp/report/figure-audit')/name;dest.parent.mkdir(exist_ok=True)
     dest.write_bytes(z.read('word/'+target))
