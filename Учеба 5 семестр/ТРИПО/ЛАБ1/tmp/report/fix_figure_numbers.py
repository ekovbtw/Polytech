from zipfile import ZipFile, ZIP_DEFLATED
from pathlib import Path
from lxml import etree as E
import re, shutil
src=Path('output/Отчет.docx')
backup=Path('tmp/report/Отчет_до_исправления_рисунков.docx')
shutil.copy2(src,backup)
ns={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
with ZipFile(src) as z:
 infos=z.infolist();parts={i.filename:z.read(i.filename) for i in infos}
root=E.fromstring(parts['word/document.xml'])
changes=[];number=0
for p in root.xpath('//w:p',namespaces=ns):
 nodes=p.xpath('.//w:t',namespaces=ns)
 text=''.join(n.text or '' for n in nodes)
 m=re.match(r'^Рисунок\s+(\d+)\s*[-–—]',text)
 if not m:continue
 number+=1
 if int(m.group(1))==number:continue
 start,end=m.span(1);offset=0
 for n in nodes:
  s=n.text or '';last=offset+len(s)
  if offset<end and last>start:
   a=max(start-offset,0);b=min(end-offset,len(s))
   n.text=s[:a]+(str(number) if offset<=start<last else '')+s[b:]
  offset=last
 changes.append((int(m.group(1)),number,text))
assert number==16
parts['word/document.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
pending=src.with_suffix('.pending.docx')
with ZipFile(pending,'w',ZIP_DEFLATED) as z:
 for info in infos:z.writestr(info,parts[info.filename])
try:pending.replace(src);result=src
except PermissionError:
 result=src.with_name('Отчет_нумерация_исправлена.docx');pending.replace(result)
with ZipFile(backup) as a,ZipFile(result) as b:
 assert all(a.read(n)==b.read(n) for n in a.namelist() if n!='word/document.xml')
 r=E.fromstring(b.read('word/document.xml'))
 captions=[''.join(p.xpath('.//w:t/text()',namespaces=ns)) for p in r.xpath('//w:p',namespaces=ns)]
 nums=[int(re.match(r'^Рисунок\s+(\d+)',t).group(1)) for t in captions if re.match(r'^Рисунок\s+\d+',t)]
 assert nums==list(range(1,17))
Path('tmp/report/figure-number-changes.txt').write_text('\n'.join(str(c) for c in changes),encoding='utf-8')
Path('tmp/report/final-report-path.txt').write_text(str(result.resolve()),encoding='utf-8')
print('Corrected captions:',len(changes),'Total:',number)
