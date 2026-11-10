from pathlib import Path
import pypdfium2 as pdf
from PIL import Image,ImageDraw
from pypdf import PdfReader
d=pdf.PdfDocument('tmp/report/report.pdf')
out=Path('tmp/report/pages');out.mkdir(exist_ok=True)
thumbs=[]
for i,page in enumerate(d):
    im=page.render(scale=1.4).to_pil();im.save(out/f'page-{i+1}.png')
    im.thumbnail((238,337));thumbs.append(im.copy())
for start in range(0,len(thumbs),12):
    sheet=Image.new('RGB',(1000,1100),'#cccccc');dr=ImageDraw.Draw(sheet)
    for j,im in enumerate(thumbs[start:start+12]):
        x=(j%4)*250;y=(j//4)*365;sheet.paste(im,(x,y+20));dr.text((x+5,y+3),str(start+j+1),fill='black')
    sheet.save(out/f'sheet-{start//12+1}.png')
r=PdfReader('tmp/report/report.pdf')
Path('tmp/report/page-text.txt').write_text('\n'.join(f'PAGE {i+1}\n'+p.extract_text() for i,p in enumerate(r.pages)),encoding='utf-8')
print('Pages',len(d))
print('Short pages',[(i+1,len(p.extract_text())) for i,p in enumerate(r.pages) if len(p.extract_text())<150])
