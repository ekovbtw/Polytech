import json
from openai import OpenAI
from config import (
    OPENROUTER_API_KEY, OPENROUTER_BASE_URL,
    PRIMARY_MODEL, FALLBACK_MODELS,
    FAST_MODEL, FAST_FALLBACKS,
)

_client = OpenAI(
    api_key=OPENROUTER_API_KEY,
    base_url=OPENROUTER_BASE_URL,
)


def _call(prompt: str, system: str = None, max_tokens: int = 512,
          fast: bool = False) -> str:
    messages = []
    if system:
        messages.append({"role": "system", "content": system})
    messages.append({"role": "user", "content": prompt})

    models = ([FAST_MODEL] + FAST_FALLBACKS) if fast else ([PRIMARY_MODEL] + FALLBACK_MODELS)
    last_err = None
    for model in models:
        try:
            resp = _client.chat.completions.create(
                model=model,
                messages=messages,
                max_tokens=max_tokens,
                temperature=0.3,
            )
            return resp.choices[0].message.content
        except Exception as e:
            print(f"[ai] {model} failed: {e}")
            last_err = e

    raise RuntimeError(f"All models failed. Last error: {last_err}")


def _extract_json(text: str):
    text = text.strip()
    # Strip markdown code fences
    if "```" in text:
        parts = text.split("```")
        # Take the content inside the first code fence
        for part in parts[1::2]:
            cleaned = part.lstrip("json").lstrip("JSON").strip()
            if cleaned:
                text = cleaned
                break
    # Sometimes models add text before/after the JSON — find the outermost { } or [ ]
    for start_char, end_char in [('{', '}'), ('[', ']')]:
        start = text.find(start_char)
        end = text.rfind(end_char)
        if start != -1 and end != -1 and end > start:
            try:
                return json.loads(text[start:end + 1])
            except json.JSONDecodeError:
                continue
    return json.loads(text)


# ── Public API ─────────────────────────────────────────────────────────────────

def generate_default_schedule(user_info: dict) -> list:
    system = (
        "Ты — ассистент-планировщик для русскоязычного студента. "
        "Отвечай ТОЛЬКО валидным JSON без пояснений и markdown."
    )
    prompt = f"""Создай базовое недельное расписание для студента российского университета.

Параметры:
- Подъём: {user_info.get('wake_time', '07:00')}
- Отход ко сну: {user_info.get('sleep_time', '23:00')}
- Учёба: Пн–Пт {user_info.get('uni_start', '09:00')}–{user_info.get('uni_end', '17:00')}

Включи: утреннюю гигиену (умывание, чистка зубов, душ) каждый день; \
завтрак/обед/ужин каждый день; \
вечернюю чистку зубов каждый день; уборку в субботу; стирку в воскресенье; отход ко сну каждый день. \
НЕ включай блок «Университет» — он уже добавлен отдельно.

Верни JSON-массив. Каждый элемент:
{{"title":"...","start_time":"HH:MM","end_time":"HH:MM","is_recurring":true,\
"recurrence_days":[0,1,2,3,4,5,6],"category":"hygiene|meal|study|chore|sleep","notes":""}}
0=Пн 6=Вс. Только JSON-массив."""
    try:
        return _extract_json(_call(prompt, system, max_tokens=800))
    except Exception as e:
        print(f"[ai] generate_default_schedule failed: {e}")
        return _fallback_schedule(user_info)


def analyze_task(task: str, existing_events: list, user_info: dict) -> dict:
    from datetime import datetime
    today = datetime.now()

    system = (
        "Ты — ассистент-планировщик. Отвечай ТОЛЬКО валидным JSON без пояснений."
    )
    prompt = f"""Проанализируй задачу и найди лучшее время для неё в расписании студента.

Задача: "{task}"
Сегодня: {today.strftime('%A %d.%m.%Y')}, день недели {today.weekday()} (0=Пн)
Подъём: {user_info.get('wake_time','07:00')}  Сон: {user_info.get('sleep_time','23:00')}
Учёба Пн–Пт: {user_info.get('uni_start','09:00')}–{user_info.get('uni_end','17:00')}

Занятые слоты сегодня:
{_fmt_events(existing_events)}

Правила:
- учёба/чтение/умственная работа → вечером после 17:00
- гигиена → утром или перед сном
- спорт → утром или сразу после учёбы
- хозяйство → выходные или вечер
- готовка → за 30 мин до приёма пищи

Верни JSON:
{{"category":"study|hygiene|meal|exercise|leisure|chore|work|other",\
"suggested_day_offset":0,"suggested_start_time":"HH:MM",\
"estimated_duration_minutes":60,"is_recurring":false,"recurrence_days":[],\
"reasoning":"кратко на русском"}}"""
    try:
        return _extract_json(_call(prompt, system, max_tokens=256, fast=True))
    except Exception as e:
        print(f"[ai] analyze_task failed: {e}")
        return {
            "category": "other",
            "suggested_day_offset": 0,
            "suggested_start_time": "19:00",
            "estimated_duration_minutes": 60,
            "is_recurring": False,
            "recurrence_days": [],
            "reasoning": "Размещено в вечернее время по умолчанию",
        }


def generate_clarifying_questions(event_description: str) -> dict:
    system = "Ты — ассистент-планировщик. Отвечай ТОЛЬКО валидным JSON."
    prompt = f"""Пользователь добавляет событие: "{event_description}"

Сгенерируй 2–4 уточняющих вопроса для правильной подготовки.
Первый вопрос — всегда про необходимость подготовки (yes_no).

Верни JSON:
{{"event_type":"exam|meeting|deadline|social|other","questions":[
  {{"key":"needs_preparation","question":"Нужна ли подготовка?","type":"yes_no"}},
  {{"key":"topic","question":"По какой теме?","type":"text","condition":"needs_preparation=да"}}
]}}"""
    try:
        return _extract_json(_call(prompt, system, max_tokens=300, fast=True))
    except Exception as e:
        print(f"[ai] generate_clarifying_questions failed: {e}")
        return {
            "event_type": "other",
            "questions": [
                {"key": "needs_preparation", "question": "Нужна ли подготовка к этому событию?", "type": "yes_no"}
            ],
        }


def generate_prep_questions(task: str) -> dict:
    """Generate clarifying questions for a prep task: topic, deadline, self-assessment, test question."""
    system = "Ты — ассистент-планировщик. Отвечай ТОЛЬКО валидным JSON."
    prompt = f"""Пользователь хочет подготовиться к: "{task}"

Сгенерируй ровно 5 уточняющих вопросов в таком ОБЯЗАТЕЛЬНОМ порядке.
ВАЖНО: НЕ добавляй поле "condition" ни к одному вопросу.

1. key="topic" — тема/предмет задачи. Если задача общая (курсовая, реферат, проект, экзамен) — спроси конкретную тему и предмет. Если тема уже есть в названии — спроси уточняющую деталь (подраздел, конкретный вопрос). type: "text"
2. key="deadline" — дедлайн (когда нужно сдать/завершить, просить точную дату). type: "text"
3. key="self_level" — самооценка знаний по теме. type: "choice", choices: ["1 — почти ничего не знаю", "2 — базовые понятия", "3 — средний уровень", "4 — хорошо знаю", "5 — отлично знаю"]
4. key="test_answer" — КОНКРЕТНЫЙ тестовый вопрос именно по теме задачи (не "что вы знаете", а реальный вопрос как на экзамене). type: "text"
5. key="details" — специфичный вопрос про формат/требования/объём (зависит от типа задачи). type: "text"

JSON:
{{"questions": [
  {{"key": "topic", "question": "По какой теме и предмету?", "type": "text"}},
  {{"key": "deadline", "question": "Когда дедлайн?", "type": "text"}},
  {{"key": "self_level", "question": "Как оцениваешь свои знания?", "type": "choice",
    "choices": ["1 — почти ничего не знаю", "2 — базовые понятия", "3 — средний уровень", "4 — хорошо знаю", "5 — отлично знаю"]}},
  {{"key": "test_answer", "question": "Конкретный тестовый вопрос по теме...", "type": "text"}},
  {{"key": "details", "question": "Специфичный вопрос про формат/требования...", "type": "text"}}
]}}"""
    try:
        result = _extract_json(_call(prompt, system, max_tokens=600, fast=True))
        # Guarantee topic question is always first
        questions = result.get("questions", [])
        keys = [q["key"] for q in questions]
        if "topic" not in keys:
            questions.insert(0, {"key": "topic", "question": f"По какой конкретно теме и предмету — «{task}»?", "type": "text"})
        return {"questions": questions}
    except Exception as e:
        print(f"[ai] generate_prep_questions failed: {e}")
        return {"questions": [
            {"key": "topic", "question": f"По какой теме и предмету — «{task}»?", "type": "text"},
            {"key": "deadline", "question": "Когда дедлайн (точная дата)?", "type": "text"},
            {"key": "self_level", "question": "Оцени знания по теме:", "type": "choice",
             "choices": ["1 — почти ничего не знаю", "2 — базовые понятия", "3 — средний уровень", "4 — хорошо знаю", "5 — отлично знаю"]},
            {"key": "details", "question": "Какие требования к работе (объём, формат)?", "type": "text"},
        ]}


def generate_detailed_plan(task: str, answers: dict, existing_events: list, user_info: dict) -> list:
    """Generate a detailed prep plan with subtask breakdown, based on clarifying answers."""
    from datetime import date as _date
    today_str = str(_date.today())
    topic = answers.get("topic", task)[:300]
    deadline = answers.get("deadline", "")[:50]
    self_level = answers.get("self_level", "")[:100]
    test_answer = answers.get("test_answer", "")[:300]
    details = answers.get("details", "")[:800]  # может быть длинным (список тем)

    system = "Ты — ассистент-планировщик. Отвечай ТОЛЬКО валидным JSON-массивом."
    prompt = f"""Составь детальный план подготовки к задаче для студента.

Задача: "{task}"
Тема/предмет: {topic}
Сегодня: {today_str}
{f'Дедлайн: {deadline}' if deadline else ''}
Самооценка знаний: {self_level}
{f'Ответ на тестовый вопрос: {test_answer}' if test_answer else ''}
{f'Детали/требования: {details}' if details else ''}

Занятые слоты:
{_fmt_events(existing_events)}

Учёба: Пн–Пт {user_info.get('uni_start','09:00')}–{user_info.get('uni_end','17:00')}
Подъём: {user_info.get('wake_time','07:00')}  Сон: {user_info.get('sleep_time','23:00')}

Инструкции:
1. Оцени реальный уровень по ответу на тестовый вопрос (важнее самооценки)
2. Разбей тему "{topic}" на конкретные подзадачи (не "изучить тему", а конкретные главы/понятия)
3. Распредели сессии равномерно до дедлайна; все даты ≥ {today_str}
4. В title пиши конкретную подзадачу, в notes — что именно делать
5. Умственная работа — вечером (17:00+) или в выходные; ≤ 2 ч/день

JSON-массив:
[{{"title":"[Тема]: конкретная подзадача","date":"YYYY-MM-DD","start_time":"HH:MM","end_time":"HH:MM","category":"study","notes":"конкретные действия"}}]"""
    try:
        result = _extract_json(_call(prompt, system, max_tokens=1200))
        return result if isinstance(result, list) else []
    except Exception as e:
        print(f"[ai] generate_detailed_plan failed: {e}")
        return []


def generate_task_plan(task: str, existing_events: list, user_info: dict) -> list:
    """Generate 1-4 distributed prep sessions for a task over the next 7 days."""
    from datetime import date, timedelta
    today = str(date.today())
    end = str(date.today() + timedelta(days=7))
    system = "Ты — ассистент-планировщик. Отвечай ТОЛЬКО валидным JSON-массивом."
    prompt = f"""Составь план подготовки к задаче. Распредели работу на 1–4 сессии.

Задача: "{task}"
Сегодня: {today}. Планируй только с {today} по {end}.

Занятые слоты (из расписания):
{_fmt_events(existing_events)}

Учёба: Пн–Пт {user_info.get('uni_start','09:00')}–{user_info.get('uni_end','17:00')}
Подъём: {user_info.get('wake_time','07:00')}  Сон: {user_info.get('sleep_time','23:00')}

Правила:
- умственная работа → вечером (после 17:00) или выходные
- не раньше {today}
- 45–90 минут на сессию

JSON-массив:
[{{"title":"...","date":"YYYY-MM-DD","start_time":"HH:MM","end_time":"HH:MM","category":"study","notes":"что делать"}}]"""
    try:
        result = _extract_json(_call(prompt, system, max_tokens=500))
        return result if isinstance(result, list) else []
    except Exception as e:
        print(f"[ai] generate_task_plan failed: {e}")
        return []


def plan_preparation(
    event_description: str,
    event_date: str,
    event_time: str,
    answers: dict,
    existing_events: list,
    user_info: dict,
) -> list:
    from datetime import date as _date
    today_str = str(_date.today())
    answers_text = "\n".join(f"- {k}: {v}" for k, v in answers.items())
    system = "Ты — ассистент-планировщик. Отвечай ТОЛЬКО валидным JSON-массивом."
    prompt = f"""Создай план подготовки к событию для среднего студента.

Событие: "{event_description}"
Дата: {event_date}  Время: {event_time}
Сегодня: {today_str}. Все сессии подготовки должны быть {today_str} или позже — не добавляй прошедшие даты.
Ответы: {answers_text}

Занятые слоты:
{_fmt_events(existing_events)}

Учёба: Пн–Пт {user_info.get('uni_start','09:00')}–{user_info.get('uni_end','17:00')}
Сон: {user_info.get('sleep_time','23:00')}

Правила: занятия вечером или в выходные, не больше 2 ч/день, начинать за 3–5 дней до {event_date}.

JSON-массив:
[{{"title":"Подготовка: ...","date":"YYYY-MM-DD","start_time":"HH:MM",\
"end_time":"HH:MM","category":"study","notes":"что делать"}}]"""
    try:
        result = _extract_json(_call(prompt, system, max_tokens=600))
        return result if isinstance(result, list) else []
    except Exception as e:
        print(f"[ai] plan_preparation failed: {e}")
        return []


# ── Helpers ────────────────────────────────────────────────────────────────────

def _fmt_events(events: list) -> str:
    if not events:
        return "пусто"
    days_map = {0: "Пн", 1: "Вт", 2: "Ср", 3: "Чт", 4: "Пт", 5: "Сб", 6: "Вс"}
    lines = []
    for e in events[:20]:
        if e.get("is_recurring"):
            try:
                days = json.loads(e.get("recurrence_days", "[]"))
                day_str = ",".join(days_map.get(d, str(d)) for d in days)
            except Exception:
                day_str = "ежедневно"
        else:
            day_str = e.get("event_date", "?")
        lines.append(f"  {day_str} {e['start_time']}–{e['end_time']}: {e['title']}")
    return "\n".join(lines)


def _t_add(t: str, mins: int) -> str:
    h, m = map(int, t.split(":"))
    total = (h * 60 + m + mins) % (24 * 60)
    return f"{total // 60:02d}:{total % 60:02d}"


def _fallback_schedule(user_info: dict) -> list:
    w = user_info.get("wake_time", "07:00")
    s = user_info.get("sleep_time", "23:00")
    uni_s = user_info.get("uni_start", "09:00")
    uni_e = user_info.get("uni_end", "17:00")
    uni_days = [0, 1, 2, 3, 4]

    # Meal times relative to wake
    breakfast_s = _t_add(w, 30)
    breakfast_e = _t_add(w, 60)

    # Evening hygiene 30 min before sleep
    eve_hygiene = _t_add(s, -30)

    return [
        {"title": "Утренняя гигиена", "start_time": w, "end_time": _t_add(w, 25),
         "is_recurring": True, "recurrence_days": [0,1,2,3,4,5,6], "category": "hygiene", "notes": ""},
        {"title": "Завтрак", "start_time": breakfast_s, "end_time": breakfast_e,
         "is_recurring": True, "recurrence_days": [0,1,2,3,4,5,6], "category": "meal", "notes": ""},
        {"title": "Обед", "start_time": "13:00", "end_time": "13:30",
         "is_recurring": True, "recurrence_days": [0,1,2,3,4,5,6], "category": "meal", "notes": ""},
        {"title": "Ужин", "start_time": _t_add(uni_e, 90), "end_time": _t_add(uni_e, 120),
         "is_recurring": True, "recurrence_days": [0,1,2,3,4,5,6], "category": "meal", "notes": ""},
        {"title": "Вечерняя гигиена", "start_time": eve_hygiene, "end_time": _t_add(eve_hygiene, 15),
         "is_recurring": True, "recurrence_days": [0,1,2,3,4,5,6], "category": "hygiene", "notes": ""},
        {"title": "Отход ко сну", "start_time": s, "end_time": _t_add(s, 20),
         "is_recurring": True, "recurrence_days": [0,1,2,3,4,5,6], "category": "sleep", "notes": ""},
        {"title": "Уборка", "start_time": _t_add(w, 60), "end_time": _t_add(w, 120),
         "is_recurring": True, "recurrence_days": [5], "category": "chore", "notes": ""},
        {"title": "Стирка", "start_time": _t_add(w, 60), "end_time": _t_add(w, 90),
         "is_recurring": True, "recurrence_days": [6], "category": "chore", "notes": ""},
    ]
