from pathlib import Path
from copy import deepcopy
import hashlib, re, struct, math, collections, textwrap
from docx import Document
from docx.shared import Cm, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn

ROOT=Path.cwd()
REF=Path(r'C:\Users\Ilja\Downloads\Рекомендации_по_написанию_отчёта_1.docx')
OUT=ROOT/'output'/'Отчет_ЛР1_Защита_ПО.docx'
OUT.parent.mkdir(exist_ok=True)
d=Document(REF)
firstsect=deepcopy(d.sections[0]._sectPr)
body=d._element.body
start=next(i for i,e in enumerate(body) if e.tag==qn('w:p') and ''.join(e.itertext()).startswith('Первый Раздел')) if False else None
# Preserve the cover and its source formatting; replace the instructional body.
heading=next(p for p in d.paragraphs if p.text=='Первый Раздел')._p
idx=list(body).index(heading)
for e in list(body)[idx:]: body.remove(e)
body.append(firstsect)
for p in d.paragraphs:
    if p.text=='Основы криптосистемы RSA': p.text='Изучение методов защиты программного обеспечения'
    elif p.text=='«Криптографические методы защиты информации»': p.text='«ТРИПО»'
    elif 'Санкт-Петербург – 2025' in p.text: p.text='Санкт-Петербург – 2026'
    elif 'студент гр.' in p.text: p.text='студент гр. __________\t\t________________'
    elif 'ассистент ВШК' in p.text: p.text='____________________\t\t________________'
    if p.text=='Изучение методов защиты программного обеспечения':
        p.style=d.styles['Title']; p.alignment=WD_ALIGN_PARAGRAPH.CENTER
        for r in p.runs:r.font.name='Times New Roman';r.font.size=Pt(16);r.font.color.rgb=RGBColor(0,0,0)

# The sample title was shorter; remove redundant blank cover lines to keep the year on page one.
empties=[a for a in d.paragraphs if not a.text.strip()]
for a in empties[-8:]:a._p.getparent().remove(a._p)
for a in d.paragraphs:
    a.paragraph_format.first_line_indent=Cm(0)
    for r in a.runs:r.font.highlight_color=None
    if a.text.strip() and not any(v in a.text for v in ('Выполнил','студент гр.','Руководитель','____________________')):
        a.alignment=WD_ALIGN_PARAGRAPH.CENTER
for style in d.styles:
    pr=style.element.find(qn('w:pPr'))
    if pr is not None:
        border=pr.find(qn('w:pBdr'))
        if border is not None:pr.remove(border)

normal=d.styles['Normal'];normal.font.name='Times New Roman';normal.font.size=Pt(14)
pf=normal.paragraph_format;pf.first_line_indent=Cm(1.25);pf.line_spacing=1.5;pf.space_after=Pt(0);pf.widow_control=True
for name in ('Heading 1','Heading 2','Heading 3'):
    s=d.styles[name];s.font.name='Times New Roman';s.font.size=Pt(14);s.font.color.rgb=RGBColor(0,0,0)
    s.paragraph_format.keep_with_next=True;s.paragraph_format.space_before=Pt(14);s.paragraph_format.space_after=Pt(7)
    s.paragraph_format.page_break_before=name=='Heading 1'
    # Avoid the template example's automatic heading numbering.
    num=s.element.find(qn('w:pPr'))
    if num is not None:
        x=num.find(qn('w:numPr'))
        if x is not None:num.remove(x)
for s in d.sections:
    s.page_width=Cm(21);s.page_height=Cm(29.7);s.left_margin=Cm(3);s.right_margin=Cm(1);s.top_margin=Cm(2);s.bottom_margin=Cm(2)
    s.different_first_page_header_footer=True
    for p in s.footer.paragraphs:p.clear()
    p=s.footer.paragraphs[0];p.alignment=WD_ALIGN_PARAGRAPH.CENTER
    f=OxmlElement('w:fldSimple');f.set(qn('w:instr'),'PAGE');p._p.append(f)

fig=0;tab=0;lst=0
def p(t):
    x=d.add_paragraph(t);x.alignment=WD_ALIGN_PARAGRAPH.JUSTIFY;return x
def h(t,level=1):return d.add_heading(t,level)
def table(headers,rows,title):
    global tab;tab+=1;p(f'В таблице {tab} приведены {title.lower()}.')
    c=d.add_paragraph(f'Таблица {tab} — {title}',style='Название таблицы');c.paragraph_format.first_line_indent=Cm(0)
    t=d.add_table(rows=1,cols=len(headers));t.autofit=False
    widths=([2,3,12] if headers==['Версия','RVA','Изменение'] else [17/len(headers)]*len(headers))
    for col,width in zip(t.columns,widths):col.width=Cm(width)
    for c,v in zip(t.rows[0].cells,headers):c.text=v
    for row in rows:
        for c,v in zip(t.add_row().cells,row):c.text=str(v)
    for ri,row in enumerate(t.rows):
        for ci,c in enumerate(row.cells):
            c.width=Cm(widths[ci])
            tcPr=c._tc.get_or_add_tcPr();b=OxmlElement('w:tcBorders')
            for edge in ('top','left','bottom','right'):
                e=OxmlElement('w:'+edge);e.set(qn('w:val'),'single');e.set(qn('w:sz'),'4');e.set(qn('w:color'),'D9D9D9');b.append(e)
            tcPr.append(b)
            if ri==0:
                sh=OxmlElement('w:shd');sh.set(qn('w:fill'),'E7E6E6');tcPr.append(sh)
            for a in c.paragraphs:
                a.alignment=WD_ALIGN_PARAGRAPH.CENTER if ri==0 else WD_ALIGN_PARAGRAPH.LEFT
                a.paragraph_format.first_line_indent=Cm(0);a.paragraph_format.line_spacing=1.15;a.paragraph_format.space_after=Pt(4)
                for r in a.runs:r.font.size=Pt(10)
    repeat=OxmlElement('w:tblHeader');t.rows[0]._tr.get_or_add_trPr().append(repeat)
    d.add_paragraph().paragraph_format.line_spacing=1
def code(t,title,full=False):
    global lst;lst+=1
    p(f'В листинге {lst} приведён код: {title}.')
    x=d.add_paragraph(f'Листинг {lst} — {title}',style='Название таблицы');x.paragraph_format.first_line_indent=Cm(0)
    # Short bordered chunks allow code to flow across pages without a giant row.
    lines=t.expandtabs(4).splitlines()
    wrapped=[]
    for line in lines:
        wrapped.extend(textwrap.wrap(line,width=100 if full else 88,replace_whitespace=False,drop_whitespace=False) or [''])
    for i in range(0,len(wrapped),38):
        tb=d.add_table(rows=1,cols=1);c=tb.cell(0,0);a=c.paragraphs[0]
        a.style=d.styles['Программный код'];a.alignment=WD_ALIGN_PARAGRAPH.LEFT;a.paragraph_format.first_line_indent=Cm(0);a.paragraph_format.line_spacing=1;a.paragraph_format.space_after=Pt(0);a.paragraph_format.keep_together=False
        r=a.add_run('\n'.join(wrapped[i:i+38]));r.font.name='Consolas';r.font.size=Pt(8 if full else 9)
        borders=OxmlElement('w:tcBorders')
        for edge in ('top','left','bottom','right'):
            e=OxmlElement('w:'+edge);e.set(qn('w:val'),'single');e.set(qn('w:sz'),'4');e.set(qn('w:color'),'D9D9D9');borders.append(e)
        c._tc.get_or_add_tcPr().append(borders)
    d.add_paragraph().paragraph_format.line_spacing=1
def image(id,title,crop=None):
    global fig
    path=Path(r'C:\Users\Ilja\AppData\Local\Temp')/f'codex-clipboard-{id}.png'
    if not path.exists():return
    fig+=1;p(f'Результат наблюдения представлен на рисунке {fig}.')
    from PIL import Image
    im=Image.open(path)
    if crop:im=im.crop(crop)
    target=ROOT/'tmp'/'report'/f'figure-{fig}.png';im.save(target)
    a=d.add_paragraph();a.paragraph_format.first_line_indent=Cm(0);a.paragraph_format.keep_with_next=True
    a.alignment=WD_ALIGN_PARAGRAPH.CENTER
    width=min(17,15*im.width/im.height)
    a.add_run().add_picture(str(target),width=Cm(width))
    a=d.add_paragraph(f'Рисунок {fig} — {title}',style='Название рисунка');a.paragraph_format.first_line_indent=Cm(0);a.paragraph_format.line_spacing=1;a.paragraph_format.space_after=Pt(14);a.paragraph_format.keep_with_next=False

source=(ROOT/'code/TRIPO1.cpp').read_text(encoding='utf-8-sig')
asm=(ROOT/'code/antidis.asm').read_text(encoding='utf-8-sig')
def fn(name):
    m=re.search(r'^[^\n;]*\b'+re.escape(name)+r'\([^;\n]*\)\s*\n\{',source,re.M)
    if not m:return name
    beg=source.index('{',m.start());depth=0
    for i in range(beg,len(source)):
        if source[i]=='{':depth+=1
        elif source[i]=='}':
            depth-=1
            if depth==0:return source[m.start():i+1]

h('1 Цель и постановка задачи')
p('Цель работы — изучить методы защиты программного обеспечения от анализа и изменения, реализовать их в программе crackme и исследовать способы обхода. В качестве полезной функциональности использована ранее разработанная многопоточная программа перебора знаков между числами.')
p('Программа считывает пароль из password.txt. При правильном пароле она формирует serial.txt с номером вида KEY$xxxxxxxxxx$ и запускает полезные вычисления. При неверном пароле исходная защищённая программа не должна выполнять полезную часть. Предусмотрены изучение бинарного патчинга, упаковка EXE, поиск оригинальной точки входа и сравнение параметров файлов.')
p('Дополнительное задание: реализовать сравнение серийного номера, полученного из пароля, с эталоном в самомодифицирующемся коде и обойти эту проверку средствами реверс-инжиниринга и отладки.')
h('1.1 Средства разработки и исследования',2)
p('Использованы Windows x64, Visual Studio с компилятором MSVC, процедурный код в файле C++, MASM x64, отладчик x64dbg, UPX 5.3.0 и Python 3.14.6. Основная сборка — Release x64. Исходные тексты находятся в code, исходные исполняемые файлы — в exe, последовательные версии патчей — в patches.')
p('Абсолютные адреса на скриншотах отличаются из-за размещения модулей в памяти. Для воспроизводимого описания патчей далее используются RVA — смещения от базы загрузки модуля.')
h('2 Программа и исходная проверка пароля')
h('2.1 Полезная функциональность',2)
p('Программа подсчитывает число вариантов расстановки знаков «+» и «−» между N числами, при которых получается заданная сумма. Первое число используется без изменения знака. Входной файл содержит число потоков, N, последовательность из N чисел и целевую сумму. Для перебора применяются рабочие потоки, критическая секция, стартовый семафор и код Грея. В output.txt записываются число потоков, N и число решений, в time.txt — время вычислений.')
code((ROOT/'code/input.txt').read_text(),'Входные данные контрольного примера')
p('Для контрольного примера использованы 8 потоков и 28 чисел, целевая сумма равна 2. Контрольный результат — 3 220 095 решений. При правильном пароле secret123 в финальной защищённой версии формируется KEY$0831623743$. При wrong123 без патчинга выводится сообщение об ошибке, новые файлы результатов не создаются. Перед такими проверками прежние результаты удалялись.')
h('2.2 Проверка без защитных механизмов',2)
p('В исходном учебном варианте пароль сравнивался с открытой строкой secret123 через strcmp. При совпадении записывался фиксированный номер KEY$1234567890$, выводилось сообщение об успехе и вызывалась функция programm. Исторический вариант проверки приведён в листинге ниже; финальный код после добавления защит приведён в приложении 1.')
theory=(ROOT/'Теория.md').read_text(encoding='utf-8-sig')
initial=re.search(r'```(?:cpp|c)?\s*\n(.*?)```',theory,re.S)
if initial:code(initial.group(1),'Исходный вариант программы до добавления защит',True)
h('2.3 Первый бинарный патч',2)
p('В x64dbg место проверки находилось через ссылки на строки secret123 и сообщения об успехе. После strcmp выполнялись test eax, eax и jne к ветке отказа. Поскольку strcmp возвращает 0 при совпадении, переход jne выполнялся при неверном пароле. Инструкция 75 70 занимала два байта и была заменена на 90 90, то есть два NOP. После этого выполнение продолжалось по ветке успеха независимо от результата сравнения. Этот эксперимент относится к раннему варианту программы с фиксированным номером; далее анализируется защищённая версия.')
h('3 Реализация защитных механизмов')
h('3.1 Связь защит с состоянием программы',2)
p('Большинство защит изменяет глобальную переменную g_seed через corrupt_seed. Исходное значение равно 0xDEADBEEF. От неё зависят ключ расшифровки сообщений, формирование серийного номера и целевая сумма в рабочих потоках. Поэтому обнаружение анализа может проявиться позже в виде нечитаемых сообщений, неверного номера или неправильного результата вычислений. IsDebuggerPresent обрабатывается отдельно: программа сразу завершается с кодом 1.')
code(fn('str_key')+'\n\n'+fn('corrupt_seed')+'\n\n// Связь с полезной частью\nLL poisoned_result = result ^ (LL)(g_seed ^ 0xDEADBEEF);','Формирование ключа и изменение состояния')
h('3.2 Шифрование строк',2)
p('Пароль и основные диагностические сообщения хранятся в массивах байтов, закодированных XOR с ключом 0x5A. Операция обратима: повторное применение того же ключа восстанавливает исходный байт. str_key получает 0x5A из младшего байта исходного g_seed: EF XOR B5 = 5A. При изменении состояния сообщения расшифровываются неправильно. Утилита xor_encrypt.cpp генерирует массивы для включения в основной код.')
code(fn('decrypt'),'Расшифровка массива байтов')
p('Ограничение реализации: имена файлов, форматы ввода и вывода, некоторые пути и признаки виртуализации остаются открытыми строками. XOR затрудняет поиск выбранных строк в файле, но не скрывает восстановленный текст в памяти от отладчика.')
h('3.3 Обнаружение отладчика',2)
table(['Метод','Проверяемый признак','Реакция'],[
('IsDebuggerPresent','Наличие отладчика у текущего процесса','Сообщение и exit(1)'),
('PEB.BeingDebugged','Байт по смещению 2 в PEB','corrupt_seed'),
('CheckRemoteDebuggerPresent','Отладка текущего процесса','corrupt_seed'),
('GetTickCount','Время короткого цикла больше 100 мс','corrupt_seed'),
('Отладочные регистры','Ненулевые DR0–DR3','Попытка проверки через GetThreadContext')], 'Методы обнаружения отладки')
for name in ('anti_debug_1','anti_debug_2','anti_debug_3','anti_debug_4'):
    code(fn(name),'Реализация '+name)
p('В Windows x64 адрес PEB считывается через GS:[0x60], затем проверяется BeingDebugged. CheckRemoteDebuggerPresent получает дескриптор текущего процесса; слово Remote в имени API не обозначает сетевую проверку. Тайминговая проверка является эвристикой и может зависеть от планирования потоков.')
p('Дополнительно реализована anti_debug5 с чтением отладочных регистров. Получение контекста текущего выполняющегося потока через GetThreadContext не даёт надёжного подтверждения работы этого метода. Во время исследования аппаратные точки отключались, чтобы уменьшить их влияние на проверяемое состояние.')
image('32252fbc-fd80-4718-94e6-d5e6604dbc53','Ветка завершения после обнаружения отладчика', (210,0,1440,240))
image('59a43bea-aacb-4b8a-9f73-d7f36c4b7023','Изменение g_seed после ручной проверки PEB',(220,0,1440,200))
h('3.4 Выявление виртуальных машин',2)
p('В anti_vm включены три способа: поиск характерного производителя системы в реестре BIOS, проверка префиксов MAC-адресов и поиск файлов драйверов VirtualBox и VMware. При обнаружении вызывается corrupt_seed. Проверка гипервизорного бита CPUID написана, но её вызов отключён и в число включённых методов не входит. Перечисленные признаки эвристические и могут отсутствовать в виртуальной среде либо сохраняться после её удаления.')
code(fn('anti_vm'),'Включённые проверки виртуальной среды')
h('3.5 Контроль целостности и повторные проверки',2)
p('verify_integrity вычисляет CRC32 байтов функции check_password_crc до функции-маркера marker_crc. Для сохранённого EXE начало имеет RVA 0x1930, маркер — 0x1970, длина участка — 64 байта. Независимый пересчёт дал 0xF5D709B8, совпадающий с эталоном в коде. При несовпадении меняется g_seed.')
code(fn('verify_integrity'),'Проверка целостности функции пароля')
p('CRC32 не является криптографическим хешем и допускает коллизии. Расположение функций определяется компилятором и компоновщиком, поэтому границы и эталон привязаны к сборке. При некорректной длине текущая реализация пропускает проверку. В бинарном обходе изменялись вызывающие ветки, а не контролируемая функция, поэтому её CRC оставалась прежней.')
p('Основная проверка выполняется в main, повторная — в programm через verify_password_again. Между ними распределены проверки среды и формирование номера. Ложная ветка strlen(password) == 999 расположена после чтения пароля; при буфере 100 байтов такое условие недостижимо и может быть устранено оптимизатором.')
h('3.6 Противодействие анализу потока управления',2)
p('В antidis_add1 после двух условных переходов расположен мусорный байт E8, который линейный дизассемблер может принять за начало CALL и включить последующие реальные инструкции в его операнд. В antidis_jmid безусловный переход перескакивает байт C7, который при разборе с неверной границы поглощает байты полезных инструкций. Процессор использует фактическую цель перехода, тогда как линейное отображение может оказаться неверным.')
code(asm.split('PUBLIC serial_check')[0],'Два ASM приёма нарушения линейного разбора')
p('Дополнительно используется непрозрачный предикат (opaque_var | 1) != 0 и передача управления через SEH. Они усложняют анализ потока; отдельный третий метод, гарантированно приводящий к неправильному дизассемблированию, в данной версии не подтверждён.')
code(fn('protected_action'),'Передача управления через исключение')
p('В protected_action намеренно вызывается целочисленное деление на ноль. Обработчик __except вызывает programm. В x64dbg наблюдалось first-chance исключение C0000094; команда Shift+F9 передавала его программе для обработки. Наличие обработчика видно в исходном коде и данных исключений PE, поэтому SEH не делает переход принципиально невидимым для анализа.')
h('4 Самомодифицирующийся код и дополнительное задание')
h('4.1 Получение серийного номера из пароля',2)
p('Серийный номер вычисляется алгоритмом FNV-1a с 32-битным беззнаковым накопителем. Каждый байт пароля смешивается XOR, затем результат умножается на 16777619 по модулю 2^32. Для secret123 получено 831623743, или 0x3191923F. Формат %010u дополняет десятичное значение нулём слева: KEY$0831623743$. FNV-1a используется для учебного отображения пароля в номер, а не для безопасного хранения паролей.')
code(fn('serial_from_password'),'Вычисление серийного номера')
p('После хеширования номер смешивается с g_seed XOR DEADBEEF. При нормальном состоянии эта маска равна нулю. Затем вычитается 4, а ASM-функции прибавляют 1 и 3, сохраняя исходный результат и включая антидизассемблерные фрагменты в вычисление номера.')
h('4.2 Изменение инструкции сравнения',2)
p('В ASM-функции serial_check сравнивается ECX с непосредственной 32-битной константой. Начало команды cmp ecx, imm32 имеет байты 81 F9. Эталон 0x3191923F хранится в порядке little-endian как 3F 92 91 31. В EXE эти четыре байта предварительно закодированы XOR AA и равны 95 38 3B 9B. Метка serial_check_value указывает внутрь инструкции, на изменяемую константу.')
code('PUBLIC serial_check\nPUBLIC serial_check_value\n'+asm.split('serial_check PROC')[1].split('END')[0].join(['serial_check PROC','']) if False else asm[asm.index('PUBLIC serial_check'):asm.rfind('END')], 'Самомодифицируемая ASM функция сравнения')
code(fn('check_serial_selfmod'),'Обёртка изменения и выполнения машинного кода')
p('VirtualProtect временно разрешает запись, цикл XOR восстанавливает константу, FlushInstructionCache обеспечивает согласованность кэша инструкций. После вызова serial_check байты кодируются обратно и возвращаются прежние права доступа. Изменяется часть машинной инструкции, а не отдельная переменная с эталоном. Обёртка применяется последовательно из основного потока.')
image('3f2a759d-34fb-4bbd-b245-6a50f84dfbfb','Восстановленная инструкция сравнения с эталоном',(215,0,1435,95))
h('4.3 Обход самомодифицирующейся проверки',2)
p('В эксперименте на входе serial_check находился номер ECX = 0x8597F352, не совпадающий с эталоном 0x3191923F. После cmp флаг ZF был равен 0. До выполнения sete al флаг вручную изменён на 1. Затем sete al и movzx eax, al сформировали EAX = 1. Таким образом, функция сообщила об успехе при несовпадающем номере. Это обход в текущем процессе, не изменение файла на диске и не исправление самого номера.')
image('e792c79a-5ee4-4422-b4b5-21931c050810','Успешный возврат при несовпадающем номере после подмены ZF',(220,0,1900,350))
p('После возврата обёртка повторно закодировала эталон. В дампе по адресу serial_check наблюдались 81 F9 95 38 3B 9B. Это подтверждает обе фазы изменения инструкции: восстановление для сравнения и обратное кодирование после него.')
image('ad759019-d766-40a5-9c15-834f6f59b8f9','Байты инструкции после обратного кодирования',(0,0,690,100))
h('5 Бинарный обход защищённой программы')
p('Правки выполнялись в x64dbg и сохранялись через окно исправлений в отдельные EXE. Изменения сохранялись до завершения процесса. Каждая следующая версия строилась на предыдущей, поэтому содержала накопленные правки. Программные точки использовались для наблюдения, аппаратные точки на запись — для первоначального поиска изменений g_seed.')
h('5.1 Последовательность патчей',2)
table(['Версия','RVA','Изменение'],[
('patch1','1ACA','74 29 → EB 29; пропуск exit после IsDebuggerPresent'),
('patch2','1DD5','74 18 → EB 18; пропуск порчи g_seed после PEB'),
('patch3','20C3','74 18 → EB 18; пропуск порчи после CheckRemoteDebuggerPresent'),
('patch4','209C–20A1','0F 85 2C 02 00 00 → шесть 90; пропуск отказа первой проверки'),
('patch4','215A–2161','B9 3F 92 91 31 90 90 90; подстановка правильного хеша'),
('patch4','17B5','75 29 → EB 29; пропуск отказа повторной проверки'),
('patch5','1791','76 18 → EB 18; пропуск порчи состояния по таймингу')], 'Накопленные изменения машинного кода')
p('Пропуск проверок пароля сам по себе не давал правильного номера: FNV-1a от wrong123 отличается от хеша secret123. Поэтому две операции XOR при формировании номера заменены на mov ecx, 3191923F и три NOP. Следующие sub ecx, 4, antidis_add1 и antidis_jmid сохранены. Пять байтов MOV и три NOP занимают тот же восьмибайтовый участок.')
image('946fbe46-7d1e-4aa7-a6df-678dc31cf284','Безусловный переход вместо реакции IsDebuggerPresent')
image('f56f9574-d397-4014-969e-1c5a0fb2a9e9','Пропуск изменения состояния после проверки PEB',(220,0,1435,170))
image('65f86e44-4b19-421b-91fc-f6272fbfa27d','Шесть NOP вместо перехода к отказу первой проверки',(220,0,1435,150))
image('e87284c9-4030-4cfc-91e8-f547ed959384','Подстановка правильного значения для серийного номера')
image('04bff664-4203-45c0-b168-823ff8ee3665','Ветка отказа повторной проверки перед патчингом',(220,0,1435,200))
image('4de10a6f-207f-4d82-8fdc-f3d21ecb2be3','Тайминговая проверка перед изменением состояния',(220,310,1435,665))
h('5.2 Проверка результатов обхода',2)
p('После первых трёх патчей запуск под отладчиком с правильным паролем завершался с кодом 0 и давал правильные результаты. Затем выполнялся обход с wrong123. Промежуточная версия давала правильный номер, но 0 решений: ветки пароля уже были пропущены, однако состояние полезных вычислений оставалось повреждённым. После патча тайминговой ветки получены правильные результаты без ручного исправления регистров и памяти.')
table(['Сценарий','Серийный номер','Полезная часть'],[
('Исходная защищённая версия, secret123','KEY$0831623743$','3 220 095 решений'),
('Исходная защищённая версия, wrong123','Не формируется','Не запускается'),
('patch3, secret123, x64dbg','KEY$0831623743$','3 220 095 решений'),
('patch5, wrong123, x64dbg','KEY$0831623743$','3 220 095 решений')], 'Результаты контрольных запусков')
p('При исследовании также обнаружена ошибка окружения: отсутствие input.txt приводило к exit(1), хотя защитные ветки уже были пройдены. После размещения входного файла вычисления запускались. Поэтому код выхода проверялся совместно с сообщениями и новыми файлами результатов; старые serial.txt и output.txt перед контрольными запусками удалялись.')
h('6 Упаковка и анализ исполняемого файла')
h('6.1 Упаковка с помощью UPX',2)
p('Упакован исходный защищённый TRIPO1.exe, а не бинарный патч. Использована версия UPX 5.3.0 для Windows x64. Выходной файл записан отдельно, исходный сохранён. Вне отладчика с secret123 упакованная версия дала правильный серийный номер и контрольное число решений.')
code('upx.exe -o TRIPO1_upx.exe TRIPO1.exe\n// Результат UPX: 18944 -> 11776, Ratio 62.16%, win64/pe','Команда и результат упаковки')
p('UPX хранит сжатое представление программы и код загрузчика. Во время запуска загрузчик распаковывает данные в память, подготавливает необходимые структуры исполнения и передаёт управление исходному коду. Упаковка не равнозначна шифрованию и не устраняет возможность динамического анализа.')
h('6.2 Поиск оригинальной точки входа',2)
p('В x64dbg точка входа упакованного файла находилась по адресу 00007FF6AA5EBEE0 при базе 00007FF6AA5E0000, то есть RVA BEE0. Здесь наблюдался код загрузчика. По известному RVA исходной точки входа 2650 установлена аппаратная точка на выполнение 00007FF6AA5E2650. После запуска она сработала на восстановленном коде mainCRTStartup.')
image('47434160-a0f2-40d0-82a2-ac41cf26d933','Начало загрузчика UPX в точке входа',(220,0,1440,560))
code('push rbx\npush rsi\npush rdi\npush rbp\nlea rsi, [00007FF6AA5EA000h] ; адрес сжатых данных\nlea rdi, [rsi-9000h]         ; адрес области восстановления\npush rdi\nxor ebx, ebx\nxor ecx, ecx\nor rbp, -1\ncall 00007FF6AA5EBF50h      ; подпрограмма загрузчика','Фрагмент начала загрузчика по дизассемблеру')
image('44544a9f-6b5b-4d1e-b68b-6d4b93535e9c','Остановка на восстановленной OEP',(220,0,1440,135))
p('Подтверждение выполнено сравнением первых машинных инструкций с исходным TRIPO1.exe: sub rsp, 28h; call __security_init_cookie; add rsp, 28h; jmp __scrt_common_main_seh. Байты совпали, а различие абсолютных адресов обусловлено базой загрузки. OEP относится к среде запуска C/C++, а не непосредственно к main.')
image('6db25f7c-5c6e-4748-a565-40284927cb63','Точка входа исходного EXE для сравнения',(220,0,1440,135))
h('6.3 Размер энтропия и структура секций',2)
p('Энтропия рассчитана по распределению всех байтов файла по формуле H = −Σ pᵢ log₂ pᵢ, где pᵢ — частота значения байта. Диапазон показателя составляет от 0 до 8 бит/байт. Рост показателя после упаковки согласуется со сжатием, но сам по себе не доказывает шифрование.')
table(['Параметр','TRIPO1.exe','TRIPO1_upx.exe'],[
('Размер, байт','18 944','11 776'),('Энтропия, бит/байт','5,3786','7,0947'),('RVA точки входа','0x2650','0xBEE0')], 'Параметры исходного и упакованного файлов')
p('Упакованный файл занимает 62,16% исходного размера, уменьшение составляет 37,84%. Энтропия увеличилась на 1,7161 бит/байт. RawSize отражает данные секции в файле, VirtualSize — размер секции в памяти.')
table(['Файл и секция','RVA','VirtualSize','RawSize'],[
('Исходный .text','1000','2228','2400'),('Исходный .rdata','4000','15D8','1600'),('Исходный .data','6000','02F8','0200'),('Исходный .pdata','7000','021C','0400'),('Исходный .rsrc','8000','02B0','0400'),('Исходный .reloc','9000','002C','0200'),('UPX0','1000','9000','0000'),('UPX1','A000','3000','2400'),('Упакованный .rsrc','D000','1000','0600')], 'Секции PE до и после упаковки в шестнадцатеричной записи')
code('PE-заголовки: AddressOfEntryPoint = BEE0\n  UPX0  RVA 1000, VirtualSize 9000, RawSize 0\n        Область восстановления программы, OEP RVA 2650\n  UPX1  RVA A000, VirtualSize 3000, RawSize 2400\n        Сжатые данные и загрузчик, вход RVA BEE0\n  .rsrc RVA D000, VirtualSize 1000, RawSize 0600\n        Ресурсы','Схема структуры упакованного файла')
h('7 Выводы')
p('В ходе работы исследованы бинарный патчинг и взаимосвязанные защитные механизмы. Реализованы проверки отладки и виртуальной среды, контроль целостности, повторные проверки пароля, XOR-кодирование выбранных строк, запутывание потока управления и самомодифицирующееся сравнение серийного номера. Показано, что подмена флага результата позволяет обойти сравнение даже после динамического восстановления инструкции.')
p('Для защищённой версии получен сохранённый патч, который при неверном пароле формирует правильный серийный номер и сохраняет корректный результат полезных вычислений. Эксперимент показал необходимость контролировать общее состояние программы, а не только ветку отказа. Упаковка UPX уменьшила размер файла, повысила энтропию и изменила точку входа; исходная точка входа найдена после распаковки и подтверждена сравнением байтов.')
p('Ограничения версии: шифрование строк неполное; отдельно не подтверждён третий метод некорректного дизассемблирования; проверка отладочных регистров требует доработки; границы CRC зависят от компоновки. Эти ограничения не отменяют полученные результаты, но не позволяют считать защиту полной или неуязвимой.')
h('Список использованных источников')
for s in ['Методические указания к лабораторной работе № 1 «Изучение методов защиты программного обеспечения».','Дополнительное задание к лабораторной работе № 1.','Sikorski M., Honig A. Practical Malware Analysis. No Starch Press, 2012.','Документация Microsoft Learn по IsDebuggerPresent, CheckRemoteDebuggerPresent, VirtualProtect, FlushInstructionCache и GetThreadContext. https://learn.microsoft.com/windows/win32/api/','UPX — Ultimate Packer for eXecutables. https://github.com/upx/upx','x64dbg. Документация отладчика. https://help.x64dbg.com/']:
    p(s)
h('Приложение 1 Исходный код защищённой программы')
p('Ниже приведён полный исходный текст сохранённой версии code/TRIPO1.cpp. Исходники при подготовке отчёта не изменялись.')
code(source,'Полный исходный текст TRIPO1.cpp',True)
h('Приложение 2 Ассемблерные функции и утилита кодирования')
code(asm,'Полный исходный текст antidis.asm',True)
code((ROOT/'code/xor_encrypt.cpp').read_text(encoding='utf-8-sig'),'Полный исходный текст xor_encrypt.cpp',True)
h('Приложение 3 Контрольные суммы исполняемых файлов')
p('SHA-256 идентифицирует конкретные файлы, использованные при подготовке отчёта. Контрольные суммы позволяют отличать сохранённые варианты после пересборки или повторного патчинга.')
for path in sorted(list((ROOT/'exe').glob('*.exe'))+list((ROOT/'patches').glob('*.exe'))):
    a=d.add_paragraph();a.paragraph_format.first_line_indent=Cm(0);a.paragraph_format.line_spacing=1
    r=a.add_run(path.name+'\n'+hashlib.sha256(path.read_bytes()).hexdigest());r.font.name='Consolas';r.font.size=Pt(9)
# Disable stale example heading numbering inherited through the source styles.
for a in d.paragraphs:
    if a.style.name.startswith('Heading'):
        pr=a._p.get_or_add_pPr();n=OxmlElement('w:numPr');v=OxmlElement('w:numId');v.set(qn('w:val'),'0');n.append(v);pr.append(n)
settings=d.settings.element
auto=OxmlElement('w:autoHyphenation');auto.set(qn('w:val'),'true');settings.append(auto)
d.save(OUT)
(ROOT/'tmp/report/artifact.md').write_text('Reference: '+str(REF)+'\nSHA256: '+hashlib.sha256(REF.read_bytes()).hexdigest()+'\nTemplate is instructional: retain cover and named styles, replace examples with report sections. A4 portrait 3/1/2/2 cm, TNR14 1.5, indent1.25. Figure and table captions derived from template. Landscape sample sections removed as instructional examples; code uses bordered cells, 8pt for full appendices. Identity fields remain blank.\n',encoding='utf-8')
print(OUT)
print('Figures',fig,'Tables',tab,'Listings',lst)
