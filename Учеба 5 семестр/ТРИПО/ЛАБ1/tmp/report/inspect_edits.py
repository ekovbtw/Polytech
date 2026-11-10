from docx import Document
from pathlib import Path
import sys,json
sys.stdout.reconfigure(encoding='utf-8')
d=Document('output/Отчет_ЛР1_Защита_ПО.docx')
out=[]
for i,p in enumerate(d.paragraphs):
    if i>140:break
    out.append({'i':i,'text':p.text,'style':p.style.name,'pPr':str(p._p.pPr.xml if p._p.pPr is not None else ''),'runs':[(r.text[:35],r.font.name,r.font.size.pt if r.font.size else None,r.bold) for r in p.runs]})
for i,t in enumerate(d.tables):
    p=t.cell(0,0).paragraphs[0]
    out.append({'table':i,'text':p.text[:100],'style':p.style.name,'pPr':str(p._p.pPr.xml if p._p.pPr is not None else ''),'runs':[(r.font.name,r.font.size.pt if r.font.size else None) for r in p.runs[:2]]})
Path('tmp/report/edits.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
for x in out:
    print({k:v for k,v in x.items() if k!='pPr'})
