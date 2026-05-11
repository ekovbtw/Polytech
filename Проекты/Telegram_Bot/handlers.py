import json
import re
from datetime import date, timedelta

from telegram import Update, InlineKeyboardButton, InlineKeyboardMarkup
from telegram.ext import (
    Application,
    CallbackQueryHandler,
    CommandHandler,
    ConversationHandler,
    ContextTypes,
    MessageHandler,
    filters,
)

import database as db
import ai_scheduler as ai
from config import TELEGRAM_TOKEN

# ── Conversation states ────────────────────────────────────────────────────────
(
    SETUP_WAKE,    # 0
    SETUP_SLEEP,   # 1
    SETUP_UNI,     # 2
    CLARIFY,       # 3  (shared: fixed event clarifying questions)
    ADD_DAY,       # 4  (pick a day from buttons)
    ADD_TIME,      # 5  (pick a time slot or type custom)
    ADD_NAME,      # 6  (type event/task name)
    EDIT_PREP,     # 7  (review/edit AI prep plan before saving)
) = range(8)

CATEGORY_EMOJI = {
    "hygiene": "🪥",
    "meal": "🍽",
    "study": "📚",
    "work": "💼",
    "exercise": "🏃",
    "leisure": "🎮",
    "chore": "🧹",
    "sleep": "😴",
    "other": "📌",
}

DAYS_RU = ["Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"]
MONTHS_SHORT = ["янв", "фев", "мар", "апр", "май", "июн",
                "июл", "авг", "сен", "окт", "ноя", "дек"]


# ── Helpers ────────────────────────────────────────────────────────────────────

def _parse_time(text: str):
    text = text.strip()
    m = re.match(r"^(\d{1,2})[:\.](\d{2})$", text)
    if m:
        h, mn = int(m.group(1)), int(m.group(2))
        if 0 <= h <= 23 and 0 <= mn <= 59:
            return f"{h:02d}:{mn:02d}"
    m = re.match(r"^(\d{1,2})$", text)
    if m:
        h = int(m.group(1))
        if 0 <= h <= 23:
            return f"{h:02d}:00"
    return None


def _add_minutes(t: str, mins: int) -> str:
    h, m = map(int, t.split(":"))
    total = (h * 60 + m + mins) % (24 * 60)
    return f"{total // 60:02d}:{total % 60:02d}"


def _time_to_min(t: str) -> int:
    h, m = map(int, t.split(":"))
    return h * 60 + m


def _fmt_day(events: list, wake_time: str = "07:00") -> str:
    if not events:
        return "  нет событий"
    wake_min = _time_to_min(wake_time)
    # Sort relative to wake time: events before wake wrap to end of the displayed day
    def sort_key(e):
        mins = _time_to_min(e["start_time"])
        return (mins - wake_min) % (24 * 60)
    lines = []
    for e in sorted(events, key=sort_key):
        emoji = CATEGORY_EMOJI.get(e["category"], "📌")
        pin = " 📌" if e.get("is_fixed") else ""
        lines.append(f"  {emoji} {e['start_time']}–{e['end_time']} {e['title']}{pin}")
    return "\n".join(lines)


def _date_header(d: date) -> str:
    return f"{d.day} {MONTHS_SHORT[d.month - 1]}, {DAYS_RU[d.weekday()]}"


def _get_free_slots(user_id: int, date_str: str, dow: int,
                    wake: str, sleep: str, min_dur: int = 45) -> list:
    """Return list of free HH:MM start times (up to 8 slots of ≥min_dur min)."""
    events = db.get_events_for_day(user_id, date_str, dow)
    wake_m = _time_to_min(wake)
    sleep_m = _time_to_min(sleep)
    if sleep_m <= wake_m:
        sleep_m += 24 * 60  # overnight person

    busy = []
    for e in events:
        s = _time_to_min(e["start_time"])
        en = _time_to_min(e["end_time"])
        if s < wake_m:
            s += 24 * 60
        if en <= s:
            en += 24 * 60
        if s < sleep_m:
            busy.append([s, min(en, sleep_m)])
    busy.sort()

    merged: list = []
    for s, e in busy:
        if merged and s <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], e)
        else:
            merged.append([s, e])

    slots = []
    cursor = wake_m
    for seg_s, seg_e in (merged + [[sleep_m, sleep_m]]):
        if seg_s - cursor >= min_dur:
            rounded = ((cursor + 29) // 30) * 30
            while rounded + min_dur <= seg_s and len(slots) < 8:
                t = rounded % (24 * 60)
                slots.append(f"{t // 60:02d}:{t % 60:02d}")
                rounded += 30
        cursor = max(cursor, seg_e)
    return slots


def _day_keyboard(n_days: int = 10) -> InlineKeyboardMarkup:
    today = date.today()
    buttons, row = [], []
    for i in range(n_days):
        d = today + timedelta(days=i)
        if i == 0:
            label = f"Сегодня {DAYS_RU[d.weekday()]} {d.day:02d}.{d.month:02d}"
        elif i == 1:
            label = f"Завтра {DAYS_RU[d.weekday()]} {d.day:02d}.{d.month:02d}"
        else:
            label = f"{DAYS_RU[d.weekday()]} {d.day:02d}.{d.month:02d}"
        row.append(InlineKeyboardButton(label, callback_data=f"add_day_{i}"))
        if len(row) == 2:
            buttons.append(row)
            row = []
    if row:
        buttons.append(row)
    buttons.append([InlineKeyboardButton("❌ Отмена", callback_data="add_cancel")])
    return InlineKeyboardMarkup(buttons)


def _time_keyboard(slots: list) -> InlineKeyboardMarkup:
    buttons, row = [], []
    for s in slots:
        row.append(InlineKeyboardButton(f"🕐 {s}", callback_data=f"add_slot_{s}"))
        if len(row) == 3:
            buttons.append(row)
            row = []
    if row:
        buttons.append(row)
    buttons.append([
        InlineKeyboardButton("✏️ Другое время", callback_data="add_slot_custom"),
        InlineKeyboardButton("❌ Отмена", callback_data="add_cancel"),
    ])
    return InlineKeyboardMarkup(buttons)


def _parse_fixed_event(text: str) -> dict:
    today = date.today()
    result = {"title": text, "date": str(today), "time": "12:00", "category": "other"}

    # Date: DD.MM or DD.MM.YYYY
    dm = re.search(r"(\d{1,2})\.(\d{1,2})(?:\.(\d{4}))?", text)
    if dm:
        try:
            d = date(int(dm.group(3) or today.year), int(dm.group(2)), int(dm.group(1)))
            result["date"] = str(d)
            text = text.replace(dm.group(0), "").strip()
        except ValueError:
            pass

    # Relative
    if "послезавтра" in text.lower():
        result["date"] = str(today + timedelta(days=2))
        text = re.sub("послезавтра", "", text, flags=re.IGNORECASE).strip()
    elif "завтра" in text.lower():
        result["date"] = str(today + timedelta(days=1))
        text = re.sub("завтра", "", text, flags=re.IGNORECASE).strip()
    elif "сегодня" in text.lower():
        result["date"] = str(today)
        text = re.sub("сегодня", "", text, flags=re.IGNORECASE).strip()

    # Named weekday
    weekdays = {
        "понедельник": 0, "вторник": 1, "среду": 2, "среда": 2,
        "четверг": 3, "пятницу": 4, "пятница": 4,
        "субботу": 5, "суббота": 5, "воскресенье": 6,
    }
    for name, dow in weekdays.items():
        if name in text.lower():
            delta = (dow - today.weekday()) % 7 or 7
            result["date"] = str(today + timedelta(days=delta))
            break

    # Time "в HH:MM"
    tm = re.search(r"в\s+(\d{1,2})[:\.](\d{2})", text, re.IGNORECASE)
    if tm:
        result["time"] = f"{int(tm.group(1)):02d}:{int(tm.group(2)):02d}"
        text = re.sub(r"в\s+\d{1,2}[:.]\d{2}", "", text, flags=re.IGNORECASE).strip()

    result["title"] = re.sub(r"\s+", " ", text).strip() or "Событие"
    return result


# ── Setup keyboard helpers ─────────────────────────────────────────────────────

def _setup_wake_keyboard() -> InlineKeyboardMarkup:
    times = ["06:00", "07:00", "08:00", "09:00", "10:00", "11:00", "12:00"]
    buttons, row = [], []
    for t in times:
        row.append(InlineKeyboardButton(t, callback_data=f"swake_{t}"))
        if len(row) == 4:
            buttons.append(row)
            row = []
    if row:
        buttons.append(row)
    buttons.append([InlineKeyboardButton("✏️ Другое время", callback_data="swake_custom")])
    return InlineKeyboardMarkup(buttons)


def _setup_sleep_keyboard() -> InlineKeyboardMarkup:
    times = ["21:00", "22:00", "23:00", "00:00", "01:00", "02:00", "03:00"]
    buttons, row = [], []
    for t in times:
        row.append(InlineKeyboardButton(t, callback_data=f"ssleep_{t}"))
        if len(row) == 4:
            buttons.append(row)
            row = []
    if row:
        buttons.append(row)
    buttons.append([InlineKeyboardButton("✏️ Другое время", callback_data="ssleep_custom")])
    buttons.append([InlineKeyboardButton("🔄 Начать заново", callback_data="ssleep_restart")])
    return InlineKeyboardMarkup(buttons)


def _setup_uni_time_keyboard() -> InlineKeyboardMarkup:
    presets = ["08:00-14:00", "09:00-15:00", "09:00-17:00", "10:00-16:00", "14:00-20:00"]
    buttons, row = [], []
    for p in presets:
        row.append(InlineKeyboardButton(p, callback_data=f"suni_t_{p}"))
        if len(row) == 2:
            buttons.append(row)
            row = []
    if row:
        buttons.append(row)
    buttons.append([
        InlineKeyboardButton("✏️ Ввести вручную", callback_data="suni_manual"),
        InlineKeyboardButton("❌ Нет учёбы", callback_data="suni_none"),
    ])
    buttons.append([InlineKeyboardButton("🔄 Начать заново", callback_data="suni_restart")])
    return InlineKeyboardMarkup(buttons)


def _setup_uni_days_keyboard(selected: list) -> InlineKeyboardMarkup:
    names = ["Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"]
    row = [
        InlineKeyboardButton(
            ("✅ " if i in selected else "") + names[i],
            callback_data=f"suni_d_{i}",
        )
        for i in range(7)
    ]
    return InlineKeyboardMarkup([
        row[:4], row[4:],
        [InlineKeyboardButton("✅ Готово", callback_data="suni_done")],
        [InlineKeyboardButton("🔄 Начать заново", callback_data="suni_restart")],
    ])


def _sleep_duration_ok(wake: str, sleep: str) -> bool:
    """True if sleep window is at least 6 hours (WHO minimum recommendation)."""
    wake_m = _time_to_min(wake)
    sleep_m = _time_to_min(sleep)
    return ((wake_m - sleep_m) % (24 * 60)) >= 6 * 60


def _main_menu_keyboard() -> InlineKeyboardMarkup:
    return InlineKeyboardMarkup([
        [
            InlineKeyboardButton("📅 Расписание сегодня", callback_data="menu_schedule"),
            InlineKeyboardButton("📆 На неделю", callback_data="menu_week"),
        ],
        [InlineKeyboardButton("📚 Запланировать подготовку", callback_data="menu_add")],
        [InlineKeyboardButton("📌 Добавить событие", callback_data="menu_fixed")],
        [InlineKeyboardButton("🗑 Удалить событие", callback_data="menu_remove")],
        [InlineKeyboardButton("⚠️ Сбросить всё расписание", callback_data="menu_reset")],
    ])


# ── /start and setup flow ──────────────────────────────────────────────────────

async def cmd_start(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    u = update.effective_user
    user = db.get_or_create_user(u.id, u.username or "", u.first_name or "")
    if user["setup_done"]:
        await update.message.reply_text(
            f"Привет, {u.first_name}! 👋\n\n"
            "Выбери действие или просто напиши задачу — я сам её распланирую:",
            reply_markup=_main_menu_keyboard(),
        )
        return ConversationHandler.END

    await update.message.reply_text(
        f"Привет, {u.first_name}! 👋\n\n"
        "Я умный планировщик — расставлю задачи по дню, настрою расписание "
        "и помогу подготовиться к важным событиям.\n\n"
        "В котором часу ты обычно просыпаешься?",
        reply_markup=_setup_wake_keyboard(),
    )
    return SETUP_WAKE


async def setup_wake_cb(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    q = update.callback_query
    await q.answer()
    if q.data == "swake_custom":
        await q.edit_message_text("Введи время подъёма (например: 7:30):")
        return SETUP_WAKE
    t = _parse_time(q.data[len("swake_"):])
    ctx.user_data["wake_time"] = t
    await q.edit_message_text(
        f"🌅 Подъём в *{t}*.\n\nВ котором часу ложишься спать?",
        parse_mode="Markdown",
        reply_markup=_setup_sleep_keyboard(),
    )
    return SETUP_SLEEP


async def setup_wake(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    t = _parse_time(update.message.text)
    if not t:
        await update.message.reply_text(
            "Не понял. Напиши в формате HH:MM, например: 7:00",
            reply_markup=_setup_wake_keyboard(),
        )
        return SETUP_WAKE
    ctx.user_data["wake_time"] = t
    await update.message.reply_text(
        f"🌅 Подъём в *{t}*.\n\nВ котором часу ложишься спать?",
        parse_mode="Markdown",
        reply_markup=_setup_sleep_keyboard(),
    )
    return SETUP_SLEEP


async def _ask_sleep(send_fn, wake: str, parse_mode=None, **kwargs):
    text = f"🌙 Введи время отбоя.\n_Подъём: {wake}. Нужно минимум 6 часов сна._"
    if parse_mode:
        await send_fn(text, parse_mode=parse_mode, reply_markup=_setup_sleep_keyboard(), **kwargs)
    else:
        await send_fn(text, parse_mode="Markdown", reply_markup=_setup_sleep_keyboard())


async def setup_sleep_cb(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    q = update.callback_query
    await q.answer()
    if q.data == "ssleep_restart":
        ctx.user_data.pop("wake_time", None)
        ctx.user_data.pop("sleep_time", None)
        await q.edit_message_text(
            "🔄 Начнём сначала.\n\nВ котором часу ты обычно просыпаешься?",
            reply_markup=_setup_wake_keyboard(),
        )
        return SETUP_WAKE
    if q.data == "ssleep_custom":
        await q.edit_message_text(
            f"Введи время отбоя (подъём: {ctx.user_data.get('wake_time','07:00')}):"
        )
        return SETUP_SLEEP
    t = _parse_time(q.data[len("ssleep_"):])
    wake = ctx.user_data.get("wake_time", "07:00")
    if not _sleep_duration_ok(wake, t):
        dur = round((((_time_to_min(wake) - _time_to_min(t)) % (24 * 60)) / 60), 1)
        await q.edit_message_text(
            f"⚠️ При подъёме в {wake} и отбое в {t} сон составит *{dur} ч* — это меньше нормы.\n"
            "Рекомендуется минимум *6 часов* (норма — 7–9 ч по рекомендациям ВОЗ).\n\n"
            "Выбери другое время отбоя:",
            parse_mode="Markdown",
            reply_markup=_setup_sleep_keyboard(),
        )
        return SETUP_SLEEP
    ctx.user_data["sleep_time"] = t
    await q.edit_message_text(
        f"🌙 Отбой в *{t}*.\n\nКогда у тебя учёба или работа?",
        parse_mode="Markdown",
        reply_markup=_setup_uni_time_keyboard(),
    )
    return SETUP_UNI


async def setup_sleep(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    t = _parse_time(update.message.text)
    wake = ctx.user_data.get("wake_time", "07:00")
    if not t:
        await update.message.reply_text(
            "Не понял. Напиши в формате HH:MM, например: 23:00",
            reply_markup=_setup_sleep_keyboard(),
        )
        return SETUP_SLEEP
    if not _sleep_duration_ok(wake, t):
        dur = round((((_time_to_min(wake) - _time_to_min(t)) % (24 * 60)) / 60), 1)
        await update.message.reply_text(
            f"⚠️ При подъёме в {wake} и отбое в {t} сон составит *{dur} ч* — это меньше нормы.\n"
            "Рекомендуется минимум *6 часов* (норма — 7–9 ч по рекомендациям ВОЗ).\n\n"
            "Введи другое время отбоя:",
            parse_mode="Markdown",
            reply_markup=_setup_sleep_keyboard(),
        )
        return SETUP_SLEEP
    ctx.user_data["sleep_time"] = t
    await update.message.reply_text(
        f"🌙 Отбой в *{t}*.\n\nКогда у тебя учёба или работа?",
        parse_mode="Markdown",
        reply_markup=_setup_uni_time_keyboard(),
    )
    return SETUP_UNI


# Короткие названия дней → номер (0=Пн)
_DAY_NAMES = {
    "пн": 0, "вт": 1, "ср": 2, "чт": 3, "пт": 4, "сб": 5, "вс": 6,
    "пон": 0, "понедельник": 0, "вторник": 1, "среда": 2, "среду": 2,
    "четверг": 3, "пятница": 4, "пятницу": 4, "суббота": 5, "субботу": 5,
    "воскресенье": 6,
}


def _parse_uni_blocks(text: str) -> list:
    """Parse one or more lines of 'HH:MM-HH:MM [days]' into list of {start, end, days}."""
    blocks = []
    for line in text.split("\n"):
        line = line.strip()
        if not line:
            continue
        m = re.search(r"(\d{1,2}[:.]\d{2})\s*[-–]\s*(\d{1,2}[:.]\d{2})", line.lower())
        if not m:
            continue
        t1 = _parse_time(m.group(1))
        t2 = _parse_time(m.group(2))
        if not t1 or not t2:
            continue
        after = line.lower()[m.end():].strip()
        days = []
        for token in re.split(r"[,\s]+", after):
            token = token.strip()
            if token in _DAY_NAMES:
                days.append(_DAY_NAMES[token])
        if not days:
            days = [0, 1, 2, 3, 4]  # default Mon–Fri
        blocks.append({"start": t1, "end": t2, "days": sorted(set(days))})
    return blocks


async def setup_uni_cb(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    q = update.callback_query
    await q.answer()
    uid = update.effective_user.id

    # Restart from the very beginning
    if q.data == "suni_restart":
        ctx.user_data.pop("wake_time", None)
        ctx.user_data.pop("sleep_time", None)
        ctx.user_data.pop("uni_preset_start", None)
        ctx.user_data.pop("uni_preset_end", None)
        ctx.user_data.pop("uni_selected_days", None)
        await q.edit_message_text(
            "🔄 Начнём сначала.\n\nВ котором часу ты обычно просыпаешься?",
            reply_markup=_setup_wake_keyboard(),
        )
        return SETUP_WAKE

    # Preset time selected → show day picker
    if q.data.startswith("suni_t_"):
        time_str = q.data[len("suni_t_"):]          # e.g. "09:00-17:00"
        s, e = time_str.split("-", 1)
        ctx.user_data["uni_preset_start"] = s
        ctx.user_data["uni_preset_end"] = e
        ctx.user_data["uni_selected_days"] = [0, 1, 2, 3, 4]
        await q.edit_message_text(
            f"🕐 *{time_str}*\n\nВыбери дни (нажми чтобы включить/выключить):",
            parse_mode="Markdown",
            reply_markup=_setup_uni_days_keyboard([0, 1, 2, 3, 4]),
        )
        return SETUP_UNI

    # Toggle a day
    if q.data.startswith("suni_d_"):
        day = int(q.data[len("suni_d_"):])
        selected = ctx.user_data.get("uni_selected_days", [0, 1, 2, 3, 4])
        if day in selected:
            selected.remove(day)
        else:
            selected.append(day)
        ctx.user_data["uni_selected_days"] = selected
        s = ctx.user_data.get("uni_preset_start", "09:00")
        e = ctx.user_data.get("uni_preset_end", "17:00")
        await q.edit_message_text(
            f"🕐 *{s}–{e}*\n\nВыбери дни:",
            parse_mode="Markdown",
            reply_markup=_setup_uni_days_keyboard(selected),
        )
        return SETUP_UNI

    # Confirmed day selection
    if q.data == "suni_done":
        selected = sorted(set(ctx.user_data.get("uni_selected_days", [])))
        s = ctx.user_data.get("uni_preset_start", "09:00")
        e = ctx.user_data.get("uni_preset_end", "17:00")
        if not selected:
            await q.edit_message_text(
                "Выбери хотя бы один день или нажми «Нет учёбы»:",
                reply_markup=_setup_uni_days_keyboard([]),
            )
            return SETUP_UNI
        user_info = {
            "wake_time": ctx.user_data.get("wake_time", "07:00"),
            "sleep_time": ctx.user_data.get("sleep_time", "23:00"),
            "uni_start": s, "uni_end": e,
            "uni_days": json.dumps(selected),
        }
        db.update_user(uid, **user_info, setup_done=1)
        await q.edit_message_text("⏳ Генерирую базовое расписание через AI…")
        return await _finish_setup(q.message, ctx, uid, user_info, [{"start": s, "end": e, "days": selected}])

    # Switch to manual text input
    if q.data == "suni_manual":
        await q.edit_message_text(
            "Напиши расписание учёбы (каждый блок на новой строке):\n\n"
            "`9:00-17:00` — Пн–Пт по умолчанию\n"
            "`14:00-18:00 пн,ср,пт` — конкретные дни\n\n"
            "Или *нет*, если без учёбы.",
            parse_mode="Markdown",
        )
        return SETUP_UNI

    # No university
    if q.data == "suni_none":
        user_info = {
            "wake_time": ctx.user_data.get("wake_time", "07:00"),
            "sleep_time": ctx.user_data.get("sleep_time", "23:00"),
            "uni_start": "09:00", "uni_end": "17:00",
            "uni_days": json.dumps([]),
        }
        db.update_user(uid, **user_info, setup_done=1)
        await q.edit_message_text("⏳ Генерирую базовое расписание через AI…")
        return await _finish_setup(q.message, ctx, uid, user_info, [])


async def setup_uni(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    uid = update.effective_user.id
    text = update.message.text.strip()
    lower = text.lower().strip()

    if lower in ("нет", "нет.", "no"):
        user_info = {
            "wake_time": ctx.user_data.get("wake_time", "07:00"),
            "sleep_time": ctx.user_data.get("sleep_time", "23:00"),
            "uni_start": "09:00", "uni_end": "17:00",
            "uni_days": json.dumps([]),
        }
        db.update_user(uid, **user_info, setup_done=1)
        await update.message.reply_text("⏳ Генерирую базовое расписание через AI…")
        return await _finish_setup(update.message, ctx, uid, user_info, [])

    blocks = _parse_uni_blocks(text)
    if not blocks:
        await update.message.reply_text(
            "Не смог разобрать. Напиши каждый блок на строке:\n"
            "`14:00-16:30 пн`\n`12:00-17:30 вт`\n\n"
            "Или одной строкой: `9:00-17:00` (Пн–Пт)\n"
            "Или *нет*, если без учёбы.",
            parse_mode="Markdown",
            reply_markup=_setup_uni_time_keyboard(),
        )
        return SETUP_UNI

    main = max(blocks, key=lambda b: len(b["days"]))
    all_days = sorted(set(d for b in blocks for d in b["days"]))
    user_info = {
        "wake_time": ctx.user_data.get("wake_time", "07:00"),
        "sleep_time": ctx.user_data.get("sleep_time", "23:00"),
        "uni_start": main["start"], "uni_end": main["end"],
        "uni_days": json.dumps(all_days),
    }
    db.update_user(uid, **user_info, setup_done=1)
    await update.message.reply_text("⏳ Генерирую базовое расписание через AI…")
    return await _finish_setup(update.message, ctx, uid, user_info, blocks)


async def _finish_setup(msg, ctx, uid: int, user_info: dict, uni_blocks: list):
    for block in uni_blocks:
        try:
            db.add_event(
                user_id=uid, title="Университет",
                start_time=block["start"], end_time=block["end"],
                is_recurring=True, recurrence_days=block["days"], category="study",
            )
        except Exception as e:
            print(f"[db] uni block error: {e}")

    events = ai.generate_default_schedule(user_info)
    if not events:
        events = ai._fallback_schedule(user_info)
    for ev in events:
        if "университет" in ev.get("title", "").lower():
            continue
        try:
            db.add_event(
                user_id=uid, title=ev["title"],
                start_time=ev["start_time"], end_time=ev["end_time"],
                is_recurring=ev.get("is_recurring", True),
                recurrence_days=ev.get("recurrence_days", [0, 1, 2, 3, 4, 5, 6]),
                category=ev.get("category", "other"),
                notes=ev.get("notes", "") or "",
            )
        except Exception as e:
            print(f"[db] save event error: {e}")

    today = date.today()
    day_events = db.get_events_for_day(uid, str(today), today.weekday())
    await msg.reply_text(
        f"✅ Базовое расписание создано!\n\n"
        f"📅 Сегодня ({_date_header(today)}):\n{_fmt_day(day_events, user_info['wake_time'])}\n\n"
        "Выбери действие или просто напиши задачу — я сам её распланирую:",
        reply_markup=_main_menu_keyboard(),
    )
    return ConversationHandler.END


# ── /schedule, /week ───────────────────────────────────────────────────────────

async def cmd_schedule(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    uid = update.effective_user.id
    user = db.get_or_create_user(uid, "", "")
    today = date.today()
    events = db.get_events_for_day(uid, str(today), today.weekday())
    await update.message.reply_text(
        f"📅 Расписание на сегодня ({_date_header(today)}):\n\n"
        f"{_fmt_day(events, user['wake_time'])}"
    )


async def cmd_week(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    uid = update.effective_user.id
    user = db.get_or_create_user(uid, "", "")
    wake = user["wake_time"]
    today = date.today()
    lines = ["📅 Расписание на неделю:\n"]
    for i in range(7):
        d = today + timedelta(days=i)
        events = db.get_events_for_day(uid, str(d), d.weekday())
        lines.append(f"*{DAYS_RU[d.weekday()]}, {d.strftime('%d.%m')}*")
        lines.append(_fmt_day(events, wake))
        lines.append("")
    await update.message.reply_text("\n".join(lines), parse_mode="Markdown")


# ── /add and /fixed — button-based flow ───────────────────────────────────────

async def cmd_add(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    ctx.user_data["add_mode"] = "prep"
    if update.callback_query:
        await update.callback_query.answer()
        send = update.callback_query.message.reply_text
    else:
        send = update.message.reply_text
    await send(
        "📚 *Запланировать подготовку*\n\n"
        "Напиши, к чему нужно подготовиться:\n"
        "_(Пример: экзамен по математике, Writing по английскому, курсовая)_",
        parse_mode="Markdown",
    )
    return ADD_NAME


async def cmd_fixed(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    ctx.user_data["add_mode"] = "fixed"
    if update.callback_query:
        await update.callback_query.answer()
        send = update.callback_query.message.reply_text
    else:
        send = update.message.reply_text
    await send(
        "📌 *Фиксированное событие*\n\nВыбери день:",
        parse_mode="Markdown",
        reply_markup=_day_keyboard(14),
    )
    return ADD_DAY


async def add_day_cb(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    q = update.callback_query
    await q.answer()
    uid = update.effective_user.id

    if q.data == "add_cancel":
        await q.edit_message_text("❌ Отменено.")
        return ConversationHandler.END

    offset = int(q.data.split("_")[2])
    target = date.today() + timedelta(days=offset)
    ctx.user_data["add_date"] = str(target)
    ctx.user_data["add_dow"] = target.weekday()

    user = db.get_or_create_user(uid, "", "")
    slots = _get_free_slots(uid, str(target), target.weekday(),
                            user["wake_time"], user["sleep_time"])

    day_label = _date_header(target)
    if slots:
        await q.edit_message_text(
            f"🗓 *{day_label}*\n\nСвободное время — выбери или введи своё:",
            parse_mode="Markdown",
            reply_markup=_time_keyboard(slots),
        )
    else:
        await q.edit_message_text(
            f"🗓 *{day_label}*\n\n⚠️ Весь день занят.\nНапиши желаемое время (HH:MM):",
            parse_mode="Markdown",
        )
    return ADD_TIME


async def add_time_cb(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    q = update.callback_query
    await q.answer()

    if q.data == "add_cancel":
        await q.edit_message_text("❌ Отменено.")
        return ConversationHandler.END

    if q.data == "add_slot_custom":
        await q.edit_message_text("Напиши желаемое время (HH:MM или HH:MM-HH:MM):")
        return ADD_TIME

    slot = q.data[len("add_slot_"):]
    ctx.user_data["add_start"] = slot
    ctx.user_data["add_end"] = _add_minutes(slot, 60)

    mode = ctx.user_data.get("add_mode", "task")
    prompt = "Что нужно сделать?" if mode == "task" else "Название события:"
    await q.edit_message_text(
        f"🕐 *{slot}* выбрано.\n\n{prompt}",
        parse_mode="Markdown",
    )
    return ADD_NAME


async def add_time_text(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    """Handle manually typed time in ADD_TIME state."""
    text = update.message.text.strip()
    t1 = t2 = None
    if "-" in text and not text.startswith("-"):
        parts = text.split("-", 1)
        t1 = _parse_time(parts[0])
        t2 = _parse_time(parts[1])
    else:
        t1 = _parse_time(text)

    if not t1:
        await update.message.reply_text("Не понял. Напиши время: 19:00 или 19:00-20:30")
        return ADD_TIME

    ctx.user_data["add_start"] = t1
    ctx.user_data["add_end"] = t2 or _add_minutes(t1, 60)

    mode = ctx.user_data.get("add_mode", "task")
    prompt = "Что нужно сделать?" if mode == "task" else "Название события:"
    await update.message.reply_text(f"🕐 {t1}. {prompt}")
    return ADD_NAME


async def add_name_text(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    """Handle task/event name entry."""
    uid = update.effective_user.id
    name = update.message.text.strip()
    mode = ctx.user_data.get("add_mode", "task")
    date_str = ctx.user_data.get("add_date", str(date.today()))
    start = ctx.user_data.get("add_start", "19:00")
    end = ctx.user_data.get("add_end", _add_minutes(start, 60))

    if mode == "prep":
        ctx.user_data["prep_task"] = name
        ctx.user_data["fixed"] = {"title": name}
        ctx.user_data["answers"] = {}
        ctx.user_data["q_idx"] = 0
        await update.message.reply_text("🤔 Генерирую уточняющие вопросы…")
        questions_data = ai.generate_prep_questions(name)
        ctx.user_data["questions"] = questions_data.get("questions", [])
        return await _ask_next_question(update, ctx, is_message=True)

    if mode == "task":
        await update.message.reply_text("🤔 Определяю категорию…")
        user_info = db.get_or_create_user(uid, "", "")
        analysis = ai.analyze_task(name, [], user_info)
        category = analysis.get("category", "other")

        db.add_event(
            user_id=uid, title=name,
            start_time=start, end_time=end,
            event_date=date_str, category=category,
        )
        d = date.fromisoformat(date_str)
        emoji = CATEGORY_EMOJI.get(category, "📌")
        await update.message.reply_text(
            f"✅ Добавлено!\n\n{emoji} *{name}*\n📅 {_date_header(d)}, {start}–{end}",
            parse_mode="Markdown",
        )
        return ConversationHandler.END

    # fixed
        ctx.user_data["fixed"] = {
            "title": name, "date": date_str,
            "time": start, "category": "other", "raw": name,
        }
        ctx.user_data["answers"] = {}
        ctx.user_data["q_idx"] = 0

        await update.message.reply_text("🤔 Генерирую уточняющие вопросы…")
        questions_data = ai.generate_clarifying_questions(name)
        ctx.user_data["questions"] = questions_data.get("questions", [])
        return await _ask_next_question(update, ctx, is_message=True)


# These keys are always shown — AI must not add conditions to them
_REQUIRED_KEYS = {"topic", "deadline", "self_level", "test_answer"}


async def _ask_next_question(obj, ctx, is_message=False):
    questions = ctx.user_data.get("questions", [])
    answers = ctx.user_data.get("answers", {})
    idx = ctx.user_data.get("q_idx", 0)

    # Skip questions whose condition is not met (never skip required keys)
    while idx < len(questions):
        q = questions[idx]
        if q.get("key") in _REQUIRED_KEYS:
            break  # always show required questions
        cond = q.get("condition", "")
        if cond:
            try:
                key, val = cond.split("=", 1)
                if answers.get(key) != val:
                    idx += 1
                    ctx.user_data["q_idx"] = idx
                    continue
            except ValueError:
                pass  # malformed condition — just show the question
        break

    if idx >= len(questions):
        mode = ctx.user_data.get("add_mode", "fixed")
        if mode == "prep":
            return await (_finish_prep(obj, ctx) if is_message else _finish_prep_cb(obj, ctx))
        return await (_finish_fixed(obj, ctx) if is_message else _finish_fixed_cb(obj, ctx))

    q = questions[idx]
    q_type = q.get("type", "text")
    total = len(questions)
    header = f"❓ *Вопрос {idx + 1} из {total}*\n\n{q['question']}"

    if q_type == "yes_no":
        kb = InlineKeyboardMarkup([[
            InlineKeyboardButton("✅ Да", callback_data="clarify_yes"),
            InlineKeyboardButton("❌ Нет", callback_data="clarify_no"),
        ]])
        if is_message:
            await obj.message.reply_text(header, parse_mode="Markdown", reply_markup=kb)
        else:
            await obj.edit_message_text(header, parse_mode="Markdown", reply_markup=kb)

    elif q_type == "choice":
        choices = q.get("choices", [])
        kb = InlineKeyboardMarkup([
            [InlineKeyboardButton(c, callback_data=f"clarify_choice_{i}")]
            for i, c in enumerate(choices)
        ])
        if is_message:
            await obj.message.reply_text(header, parse_mode="Markdown", reply_markup=kb)
        else:
            await obj.edit_message_text(header, parse_mode="Markdown", reply_markup=kb)

    else:  # text
        if is_message:
            await obj.message.reply_text(header, parse_mode="Markdown")
        else:
            await obj.edit_message_text(header, parse_mode="Markdown")

    return CLARIFY


async def clarify_callback(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    q_obj = update.callback_query
    await q_obj.answer()

    questions = ctx.user_data.get("questions", [])
    idx = ctx.user_data.get("q_idx", 0)
    answers = ctx.user_data.get("answers", {})

    if idx < len(questions):
        key = questions[idx]["key"]
        if q_obj.data in ("clarify_yes", "clarify_no"):
            answers[key] = "да" if q_obj.data == "clarify_yes" else "нет"
        elif q_obj.data.startswith("clarify_choice_"):
            choice_idx = int(q_obj.data.split("_")[2])
            choices = questions[idx].get("choices", [])
            answers[key] = choices[choice_idx] if choice_idx < len(choices) else str(choice_idx)
        ctx.user_data["answers"] = answers
        ctx.user_data["q_idx"] = idx + 1

    return await _ask_next_question(q_obj, ctx, is_message=False)


_MAX_ANSWER = 600  # chars; prevents prompt overflow for huge pasted texts


async def clarify_text(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    questions = ctx.user_data.get("questions", [])
    idx = ctx.user_data.get("q_idx", 0)
    answers = ctx.user_data.get("answers", {})

    if idx < len(questions):
        text = update.message.text.strip()
        if len(text) > _MAX_ANSWER:
            await update.message.reply_text(
                f"⚠️ Слишком длинный ответ ({len(text)} симв.). "
                f"Обрезаю до {_MAX_ANSWER} — лучше напиши кратко."
            )
            text = text[:_MAX_ANSWER]
        answers[questions[idx]["key"]] = text
        ctx.user_data["answers"] = answers
        ctx.user_data["q_idx"] = idx + 1

    return await _ask_next_question(update, ctx, is_message=True)


async def _run_finish_prep(msg, uid: int, ctx) -> int:
    task = ctx.user_data.get("prep_task", ctx.user_data.get("fixed", {}).get("title", ""))
    answers = ctx.user_data.get("answers", {})
    user_info = db.get_or_create_user(uid, "", "")
    existing = db.get_all_events(uid)

    await msg.reply_text("⏳ Анализирую ответы и составляю план…")
    prep_events = ai.generate_detailed_plan(task, answers, existing, user_info)
    if prep_events:
        ctx.user_data["fixed"] = {"title": task}
        ctx.user_data["prep_events"] = prep_events
        return await _show_prep_edit(msg, ctx, is_callback=False)

    # Retry with fallback model
    await msg.reply_text("⚠️ Первая попытка не удалась, пробую ещё раз…")
    prep_events = ai.generate_task_plan(task, existing, user_info)
    if prep_events:
        ctx.user_data["fixed"] = {"title": task}
        ctx.user_data["prep_events"] = prep_events
        return await _show_prep_edit(msg, ctx, is_callback=False)

    await msg.reply_text(
        "❌ Не удалось составить план.\n\n"
        "Попробуй /add ещё раз или /fixed для ручного добавления."
    )
    return ConversationHandler.END


async def _finish_prep(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    return await _run_finish_prep(update.message, update.effective_user.id, ctx)


async def _finish_prep_cb(q_obj, ctx):
    return await _run_finish_prep(q_obj.message, q_obj.from_user.id, ctx)


def _fmt_prep_plan(prep_events: list) -> str:
    lines = []
    for i, p in enumerate(prep_events, 1):
        lines.append(f"  {i}. {p['date']} {p['start_time']}–{p['end_time']}: {p['title']}")
    return "\n".join(lines) if lines else "  нет сессий"


def _prep_keyboard() -> InlineKeyboardMarkup:
    return InlineKeyboardMarkup([
        [InlineKeyboardButton("✅ Подтвердить", callback_data="prep_confirm")],
        [InlineKeyboardButton("✏️ Изменить сессию", callback_data="prep_editlist")],
        [InlineKeyboardButton("➖ Убрать последнюю сессию", callback_data="prep_remove")],
        [InlineKeyboardButton("❌ Без подготовки", callback_data="prep_cancel")],
    ])


def _prep_session_list_keyboard(prep_events: list) -> InlineKeyboardMarkup:
    buttons = []
    for i, p in enumerate(prep_events):
        label = f"✏️ {i + 1}. {p['date']} {p['start_time']} — {p['title'][:28]}"
        buttons.append([InlineKeyboardButton(label, callback_data=f"prep_sess_{i}")])
    buttons.append([InlineKeyboardButton("◀️ Назад", callback_data="prep_back")])
    return InlineKeyboardMarkup(buttons)


def _prep_field_keyboard(idx: int) -> InlineKeyboardMarkup:
    return InlineKeyboardMarkup([
        [InlineKeyboardButton("📅 Изменить дату", callback_data=f"prep_fld_date_{idx}")],
        [InlineKeyboardButton("🕐 Изменить время", callback_data=f"prep_fld_time_{idx}")],
        [InlineKeyboardButton("✏️ Изменить название", callback_data=f"prep_fld_title_{idx}")],
        [InlineKeyboardButton("◀️ Назад к списку", callback_data="prep_editlist")],
    ])


async def _show_prep_edit(msg_obj, ctx, is_callback: bool = False):
    prep_events = ctx.user_data.get("prep_events", [])
    event = ctx.user_data.get("fixed", {})
    ctx.user_data.pop("prep_editing", None)  # clear any active field edit
    text = (
        f"📚 *План подготовки к «{event.get('title', 'событию')}»* "
        f"({len(prep_events)} сессий):\n\n"
        f"{_fmt_prep_plan(prep_events)}\n\n"
        "Можешь изменить, убрать сессии, написать нужное число или подтвердить:"
    )
    if is_callback:
        await msg_obj.edit_message_text(text, parse_mode="Markdown", reply_markup=_prep_keyboard())
    else:
        await msg_obj.reply_text(text, parse_mode="Markdown", reply_markup=_prep_keyboard())
    return EDIT_PREP


def _save_main_event(uid: int, event: dict):
    db.add_event(
        user_id=uid,
        title=event.get("title", "Событие"),
        start_time=event.get("time", "12:00"),
        end_time=_add_minutes(event.get("time", "12:00"), 90),
        event_date=event.get("date"),
        is_fixed=True,
        category=event.get("category", "other"),
    )


async def _finish_fixed(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    uid = update.effective_user.id
    event = ctx.user_data.get("fixed", {})
    answers = ctx.user_data.get("answers", {})
    user_info = db.get_or_create_user(uid, "", "")
    all_events = db.get_all_events(uid)

    _save_main_event(uid, event)

    if answers.get("needs_preparation") == "да":
        await update.message.reply_text("⏳ Строю план подготовки…")
        prep_events = ai.plan_preparation(
            event.get("raw", ""), event.get("date", ""), event.get("time", ""),
            answers, all_events, user_info,
        )
        if prep_events:
            ctx.user_data["prep_events"] = prep_events
            return await _show_prep_edit(update.message, ctx, is_callback=False)

    d = date.fromisoformat(event.get("date", str(date.today())))
    await update.message.reply_text(
        f"✅ *{event.get('title', 'Событие')}* добавлено!\n📅 {_date_header(d)} в {event.get('time')}",
        parse_mode="Markdown",
    )
    return ConversationHandler.END


async def _finish_fixed_cb(q_obj, ctx):
    uid = q_obj.from_user.id
    event = ctx.user_data.get("fixed", {})
    answers = ctx.user_data.get("answers", {})
    user_info = db.get_or_create_user(uid, "", "")
    all_events = db.get_all_events(uid)

    _save_main_event(uid, event)

    if answers.get("needs_preparation") == "да":
        await q_obj.message.reply_text("⏳ Строю план подготовки…")
        prep_events = ai.plan_preparation(
            event.get("raw", ""), event.get("date", ""), event.get("time", ""),
            answers, all_events, user_info,
        )
        if prep_events:
            ctx.user_data["prep_events"] = prep_events
            return await _show_prep_edit(q_obj.message, ctx, is_callback=False)

    d = date.fromisoformat(event.get("date", str(date.today())))
    await q_obj.message.reply_text(
        f"✅ *{event.get('title', 'Событие')}* добавлено!\n📅 {_date_header(d)} в {event.get('time')}",
        parse_mode="Markdown",
    )
    return ConversationHandler.END


async def prep_edit_callback(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    q = update.callback_query
    await q.answer()
    uid = update.effective_user.id
    event = ctx.user_data.get("fixed", {})
    prep_events = ctx.user_data.get("prep_events", [])

    # ── Confirm: save all prep sessions ──────────────────────────────────────
    if q.data == "prep_confirm":
        for p in prep_events:
            db.add_event(
                user_id=uid, title=p["title"],
                start_time=p["start_time"], end_time=p["end_time"],
                event_date=p["date"], category=p.get("category", "study"),
                notes=p.get("notes", ""),
            )
        await q.edit_message_text(
            f"✅ *{event.get('title', 'Событие')}* и {len(prep_events)} сессий подготовки сохранены!",
            parse_mode="Markdown",
        )
        return ConversationHandler.END

    # ── Remove last session ───────────────────────────────────────────────────
    if q.data == "prep_remove":
        if prep_events:
            prep_events.pop()
            ctx.user_data["prep_events"] = prep_events
        if not prep_events:
            await q.edit_message_text(
                f"✅ *{event.get('title', 'Событие')}* добавлено без подготовки.",
                parse_mode="Markdown",
            )
            return ConversationHandler.END
        return await _show_prep_edit(q, ctx, is_callback=True)

    # ── Cancel prep entirely ──────────────────────────────────────────────────
    if q.data == "prep_cancel":
        await q.edit_message_text(
            f"✅ *{event.get('title', 'Событие')}* добавлено без подготовки.",
            parse_mode="Markdown",
        )
        return ConversationHandler.END

    # ── Back to main prep screen ──────────────────────────────────────────────
    if q.data == "prep_back":
        return await _show_prep_edit(q, ctx, is_callback=True)

    # ── Show session list for editing ─────────────────────────────────────────
    if q.data == "prep_editlist":
        await q.edit_message_text(
            f"✏️ *Выбери сессию для редактирования:*",
            parse_mode="Markdown",
            reply_markup=_prep_session_list_keyboard(prep_events),
        )
        return EDIT_PREP

    # ── Session selected → show field picker ─────────────────────────────────
    if q.data.startswith("prep_sess_"):
        idx = int(q.data.split("_")[2])
        p = prep_events[idx]
        await q.edit_message_text(
            f"✏️ *Сессия {idx + 1}:*\n"
            f"📅 {p['date']}  🕐 {p['start_time']}–{p['end_time']}\n"
            f"📝 {p['title']}\n\nЧто изменить?",
            parse_mode="Markdown",
            reply_markup=_prep_field_keyboard(idx),
        )
        return EDIT_PREP

    # ── Field selected → ask for new value ───────────────────────────────────
    if q.data.startswith("prep_fld_"):
        parts = q.data.split("_")   # prep_fld_<field>_<idx>
        field = parts[2]
        idx = int(parts[3])
        ctx.user_data["prep_editing"] = {"idx": idx, "field": field}
        prompts = {
            "date": "Введи новую дату (ДД.ММ или ДД.ММ.ГГГГ):",
            "time": "Введи новое время начала (ЧЧ:ММ или ЧЧ:ММ-ЧЧ:ММ):",
            "title": "Введи новое название сессии:",
        }
        await q.message.reply_text(prompts.get(field, "Введи новое значение:"))
        return EDIT_PREP


async def prep_edit_text(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    text = update.message.text.strip()
    event = ctx.user_data.get("fixed", {})
    prep_events = ctx.user_data.get("prep_events", [])

    # ── Editing a specific field ──────────────────────────────────────────────
    editing = ctx.user_data.get("prep_editing")
    if editing:
        idx = editing["idx"]
        field = editing["field"]
        p = prep_events[idx]

        if field == "date":
            today = date.today()
            dm = re.search(r"(\d{1,2})\.(\d{1,2})(?:\.(\d{4}))?", text)
            if not dm:
                await update.message.reply_text("Не понял. Напиши дату: 15.05 или 15.05.2026")
                return EDIT_PREP
            try:
                new_date = date(int(dm.group(3) or today.year), int(dm.group(2)), int(dm.group(1)))
                p["date"] = str(new_date)
            except ValueError:
                await update.message.reply_text("Неверная дата. Попробуй ещё раз: 15.05")
                return EDIT_PREP

        elif field == "time":
            if "-" in text:
                parts = text.split("-", 1)
                t1 = _parse_time(parts[0])
                t2 = _parse_time(parts[1])
            else:
                t1 = _parse_time(text)
                t2 = None
            if not t1:
                await update.message.reply_text("Не понял. Напиши: 19:00 или 19:00-20:30")
                return EDIT_PREP
            p["start_time"] = t1
            p["end_time"] = t2 or _add_minutes(t1, 90)

        elif field == "title":
            if not text:
                await update.message.reply_text("Название не может быть пустым.")
                return EDIT_PREP
            p["title"] = text

        ctx.user_data["prep_events"] = prep_events
        ctx.user_data.pop("prep_editing", None)
        await update.message.reply_text(f"✅ Сессия {idx + 1} обновлена.")
        return await _show_prep_edit(update.message, ctx, is_callback=False)

    # ── Number → set session count ────────────────────────────────────────────
    try:
        n = int(text)
    except ValueError:
        await update.message.reply_text("Напиши число сессий или используй кнопки ниже.")
        return EDIT_PREP

    n = max(0, min(n, len(prep_events)))
    ctx.user_data["prep_events"] = prep_events[:n]

    if n == 0:
        await update.message.reply_text(
            f"✅ *{event.get('title', 'Событие')}* добавлено без подготовки.",
            parse_mode="Markdown",
        )
        return ConversationHandler.END

    return await _show_prep_edit(update.message, ctx, is_callback=False)


# ── /remove ────────────────────────────────────────────────────────────────────

def _remove_day_keyboard() -> InlineKeyboardMarkup:
    today = date.today()
    buttons, row = [], []
    for i in range(7):
        d = today + timedelta(days=i)
        label = ("Сегодня " if i == 0 else "Завтра " if i == 1 else "") + \
                f"{DAYS_RU[d.weekday()]} {d.day:02d}.{d.month:02d}"
        row.append(InlineKeyboardButton(label, callback_data=f"rday_{i}"))
        if len(row) == 2:
            buttons.append(row)
            row = []
    if row:
        buttons.append(row)
    buttons.append([InlineKeyboardButton("❌ Отмена", callback_data="del_cancel")])
    return InlineKeyboardMarkup(buttons)


async def cmd_remove(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    await update.message.reply_text(
        "🗑 *Удалить событие*\n\nВыбери день:",
        parse_mode="Markdown",
        reply_markup=_remove_day_keyboard(),
    )


async def remove_callback(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    q = update.callback_query
    await q.answer()
    uid = update.effective_user.id

    if q.data == "del_cancel":
        await q.edit_message_text("Отменено.")
        return

    # Day selected → show events for that day
    if q.data.startswith("rday_"):
        offset = int(q.data.split("_")[1])
        target = date.today() + timedelta(days=offset)
        user = db.get_or_create_user(uid, "", "")
        events = db.get_events_for_day(uid, str(target), target.weekday())

        if not events:
            await q.edit_message_text(
                f"На *{_date_header(target)}* нет событий.",
                parse_mode="Markdown",
            )
            return

        sorted_events = sorted(events, key=lambda e: (
            (_time_to_min(e["start_time"]) - _time_to_min(user["wake_time"])) % (24 * 60)
        ))
        buttons = [
            [InlineKeyboardButton(
                f"🗑 {e['start_time']} {e['title'][:30]}",
                callback_data=f"del_{e['id']}",
            )]
            for e in sorted_events
        ]
        buttons.append([InlineKeyboardButton("◀️ Назад к дням", callback_data="del_back")])
        buttons.append([InlineKeyboardButton("❌ Отмена", callback_data="del_cancel")])
        await q.edit_message_text(
            f"🗑 *{_date_header(target)}* — выбери событие:",
            parse_mode="Markdown",
            reply_markup=InlineKeyboardMarkup(buttons),
        )
        return

    # Back → show day selector again
    if q.data == "del_back":
        await q.edit_message_text(
            "🗑 *Удалить событие*\n\nВыбери день:",
            parse_mode="Markdown",
            reply_markup=_remove_day_keyboard(),
        )
        return

    # Delete specific event
    event_id = int(q.data.split("_", 1)[1])
    db.delete_event(event_id, uid)
    await q.edit_message_text("✅ Удалено.")


# ── /help ──────────────────────────────────────────────────────────────────────

async def cmd_help(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    await update.message.reply_text(
        "Выбери действие или просто напиши задачу:",
        reply_markup=_main_menu_keyboard(),
    )


# ── Plain text → schedule query or smart task ─────────────────────────────────

_SCHEDULE_TRIGGERS = ["расписание", "покажи", "что сегодня", "что завтра",
                      "план на", "что на", "занято", "свободно", "покажи на"]

# Pattern: "день с HH:MM до HH:MM название" → recurring event
_RECURRING_EVENT_RE = re.compile(
    r"^(каждый\s+|каждую\s+|каждое\s+)?"
    r"(понедельник|вторник|среду?|четверг|пятницу?|субботу?|воскресенье)\s+"
    r"с\s+(\d{1,2}[:.]\d{2})\s+до\s+(\d{1,2}[:.]\d{2})\s+(.+)$",
    re.IGNORECASE,
)

_DAY_OFFSETS = {
    "сегодня": 0, "сейчас": 0,
    "завтра": 1, "послезавтра": 2,
    "понедельник": None, "вторник": None, "среду": None, "среда": None,
    "четверг": None, "пятницу": None, "пятница": None,
    "субботу": None, "суббота": None, "воскресенье": None,
}
_DAY_OFFSET_BY_DOW = {
    "понедельник": 0, "вторник": 1, "среду": 2, "среда": 2,
    "четверг": 3, "пятницу": 4, "пятница": 4,
    "субботу": 5, "суббота": 5, "воскресенье": 6,
}


def _detect_schedule_query(text: str):
    """Return day offset if text looks like a schedule query, else None."""
    lower = text.lower()
    if not any(kw in lower for kw in _SCHEDULE_TRIGGERS):
        return None
    # Determine which day
    for name, dow in _DAY_OFFSET_BY_DOW.items():
        if name in lower:
            today_dow = date.today().weekday()
            delta = (dow - today_dow) % 7
            return delta if delta > 0 else (7 if delta == 0 and "следующ" in lower else 0)
    if "завтра" in lower:
        return 1
    if "послезавтра" in lower:
        return 2
    return 0  # default: today


async def plain_text(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    text = update.message.text.strip()
    uid = update.effective_user.id

    # 1. "Воскресенье с 11:00 до 17:00 работа" → recurring event
    m = _RECURRING_EVENT_RE.match(text)
    if m:
        day_word = m.group(2).lower()
        start = _parse_time(m.group(3))
        end = _parse_time(m.group(4))
        title = m.group(5).strip()
        dow = _DAY_NAMES.get(day_word)
        if start and end and dow is not None:
            db.add_event(
                user_id=uid, title=title,
                start_time=start, end_time=end,
                is_recurring=True, recurrence_days=[dow],
                category="work",
            )
            day_names_full = ["понедельник","вторник","среду","четверг","пятницу","субботу","воскресенье"]
            await update.message.reply_text(
                f"✅ Добавлено каждый(ую) *{day_names_full[dow]}*:\n"
                f"💼 {title} {start}–{end}",
                parse_mode="Markdown",
            )
            return

    # 2. "Покажи расписание на вторник" → show schedule
    offset = _detect_schedule_query(text)
    if offset is not None:
        user = db.get_or_create_user(uid, "", "")
        target = date.today() + timedelta(days=offset)
        events = db.get_events_for_day(uid, str(target), target.weekday())
        label = "сегодня" if offset == 0 else "завтра" if offset == 1 else _date_header(target)
        await update.message.reply_text(
            f"📅 Расписание на {label} ({_date_header(target)}):\n\n"
            f"{_fmt_day(events, user['wake_time'])}"
        )
        return

    # 3. Everything else → AI task analysis
    return await handle_task_text(update, ctx)


async def menu_callback(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    q = update.callback_query
    await q.answer()
    uid = update.effective_user.id

    if q.data == "menu_schedule":
        user = db.get_or_create_user(uid, "", "")
        today = date.today()
        events = db.get_events_for_day(uid, str(today), today.weekday())
        await q.message.reply_text(
            f"📅 Расписание на сегодня ({_date_header(today)}):\n\n"
            f"{_fmt_day(events, user['wake_time'])}"
        )
        return

    if q.data == "menu_week":
        user = db.get_or_create_user(uid, "", "")
        today = date.today()
        lines = ["📅 Расписание на неделю:\n"]
        for i in range(7):
            d = today + timedelta(days=i)
            events = db.get_events_for_day(uid, str(d), d.weekday())
            lines.append(f"*{DAYS_RU[d.weekday()]}, {d.strftime('%d.%m')}*")
            lines.append(_fmt_day(events, user["wake_time"]))
            lines.append("")
        await q.message.reply_text("\n".join(lines), parse_mode="Markdown")
        return

    if q.data == "menu_remove":
        await q.message.reply_text(
            "🗑 *Удалить событие*\n\nВыбери день:",
            parse_mode="Markdown",
            reply_markup=_remove_day_keyboard(),
        )
        return

    if q.data == "menu_reset":
        await q.message.reply_text(
            "⚠️ *Сбросить всё расписание?*\n\n"
            "Удалятся все события и настройки — потребуется настройка заново.",
            parse_mode="Markdown",
            reply_markup=InlineKeyboardMarkup([[
                InlineKeyboardButton("✅ Да, удалить всё", callback_data="reset_confirm"),
                InlineKeyboardButton("❌ Отмена", callback_data="reset_cancel"),
            ]]),
        )
        return

    if q.data == "reset_confirm":
        db.delete_all_events(uid)
        db.update_user(uid, setup_done=0)
        await q.edit_message_text(
            "✅ Всё удалено. Напиши /start чтобы настроить заново."
        )
        return

    if q.data == "reset_cancel":
        await q.edit_message_text("Отменено.")
        return


async def handle_task_text(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    """AI-powered plain-text task addition."""
    uid = update.effective_user.id
    text = update.message.text.strip()

    user_info = db.get_or_create_user(uid, "", "")
    today = date.today()
    existing = db.get_events_for_day(uid, str(today), today.weekday())

    await update.message.reply_text("🤔 Анализирую задачу…")
    analysis = ai.analyze_task(text, existing, user_info)

    offset = analysis.get("suggested_day_offset", 0)
    target = today + timedelta(days=offset)
    start = analysis.get("suggested_start_time", "19:00")
    duration = analysis.get("estimated_duration_minutes", 60)
    end = _add_minutes(start, duration)
    category = analysis.get("category", "other")
    reasoning = analysis.get("reasoning", "")

    db.add_event(
        user_id=uid, title=text,
        start_time=start, end_time=end,
        event_date=str(target),
        category=category,
    )

    emoji = CATEGORY_EMOJI.get(category, "📌")
    msg = f"✅ Добавлено!\n\n{emoji} *{text}*\n📅 {_date_header(target)}, {start}–{end}"
    if reasoning:
        msg += f"\n💡 _{reasoning}_"
    await update.message.reply_text(msg, parse_mode="Markdown")


# ── Build application ──────────────────────────────────────────────────────────

def build_application() -> Application:
    app = Application.builder().token(TELEGRAM_TOKEN).build()

    setup_conv = ConversationHandler(
        entry_points=[CommandHandler("start", cmd_start)],
        states={
            SETUP_WAKE: [
                CallbackQueryHandler(setup_wake_cb, pattern="^swake_"),
                MessageHandler(filters.TEXT & ~filters.COMMAND, setup_wake),
            ],
            SETUP_SLEEP: [
                CallbackQueryHandler(setup_sleep_cb, pattern="^ssleep_"),
                MessageHandler(filters.TEXT & ~filters.COMMAND, setup_sleep),
            ],
            SETUP_UNI: [
                CallbackQueryHandler(setup_uni_cb, pattern="^suni_"),
                MessageHandler(filters.TEXT & ~filters.COMMAND, setup_uni),
            ],
        },
        fallbacks=[CommandHandler("start", cmd_start)],
    )

    add_conv = ConversationHandler(
        entry_points=[
            CommandHandler("add", cmd_add),
            CommandHandler("fixed", cmd_fixed),
            CallbackQueryHandler(cmd_add, pattern="^menu_add$"),
            CallbackQueryHandler(cmd_fixed, pattern="^menu_fixed$"),
        ],
        states={
            ADD_DAY: [
                CallbackQueryHandler(add_day_cb, pattern=r"^add_day_\d+$|^add_cancel$"),
            ],
            ADD_TIME: [
                CallbackQueryHandler(add_time_cb, pattern=r"^add_slot_|^add_cancel$"),
                MessageHandler(filters.TEXT & ~filters.COMMAND, add_time_text),
            ],
            ADD_NAME: [
                MessageHandler(filters.TEXT & ~filters.COMMAND, add_name_text),
            ],
            CLARIFY: [
                CallbackQueryHandler(clarify_callback, pattern="^clarify_"),  # yes/no/choice
                MessageHandler(filters.TEXT & ~filters.COMMAND, clarify_text),
            ],
            EDIT_PREP: [
                CallbackQueryHandler(prep_edit_callback, pattern="^prep_"),
                MessageHandler(filters.TEXT & ~filters.COMMAND, prep_edit_text),
            ],
        },
        fallbacks=[CommandHandler("start", cmd_start)],
        per_message=False,
    )

    app.add_handler(setup_conv)
    app.add_handler(add_conv)
    app.add_handler(CommandHandler("schedule", cmd_schedule))
    app.add_handler(CommandHandler("week", cmd_week))
    app.add_handler(CommandHandler("remove", cmd_remove))
    app.add_handler(CommandHandler("help", cmd_help))
    app.add_handler(CallbackQueryHandler(remove_callback, pattern="^del_|^rday_|^del_back$|^del_cancel$"))
    app.add_handler(CallbackQueryHandler(menu_callback, pattern="^menu_|^reset_"))
    app.add_handler(MessageHandler(filters.TEXT & ~filters.COMMAND, plain_text))

    return app
