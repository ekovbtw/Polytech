import sqlite3
import json
from config import DB_PATH


def get_conn():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    conn = get_conn()
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS users (
            user_id     INTEGER PRIMARY KEY,
            username    TEXT,
            first_name  TEXT,
            setup_done  INTEGER DEFAULT 0,
            wake_time   TEXT DEFAULT '07:00',
            sleep_time  TEXT DEFAULT '23:00',
            uni_start   TEXT DEFAULT '09:00',
            uni_end     TEXT DEFAULT '17:00',
            uni_days    TEXT DEFAULT '[0,1,2,3,4]',
            created_at  TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS events (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id          INTEGER NOT NULL,
            title            TEXT NOT NULL,
            event_date       TEXT,
            start_time       TEXT NOT NULL,
            end_time         TEXT NOT NULL,
            is_fixed         INTEGER DEFAULT 0,
            is_recurring     INTEGER DEFAULT 0,
            recurrence_days  TEXT DEFAULT '[]',
            category         TEXT DEFAULT 'other',
            notes            TEXT,
            created_at       TEXT DEFAULT CURRENT_TIMESTAMP
        );
    """)
    conn.commit()
    conn.close()


def get_or_create_user(user_id: int, username: str, first_name: str) -> dict:
    conn = get_conn()
    c = conn.cursor()
    c.execute("SELECT * FROM users WHERE user_id = ?", (user_id,))
    row = c.fetchone()
    if not row:
        c.execute(
            "INSERT INTO users (user_id, username, first_name) VALUES (?, ?, ?)",
            (user_id, username, first_name),
        )
        conn.commit()
        c.execute("SELECT * FROM users WHERE user_id = ?", (user_id,))
        row = c.fetchone()
    conn.close()
    return dict(row)


def update_user(user_id: int, **kwargs):
    conn = get_conn()
    fields = ", ".join(f"{k} = ?" for k in kwargs)
    values = list(kwargs.values()) + [user_id]
    conn.execute(f"UPDATE users SET {fields} WHERE user_id = ?", values)
    conn.commit()
    conn.close()


def add_event(
    user_id: int,
    title: str,
    start_time: str,
    end_time: str,
    event_date: str = None,
    is_fixed: bool = False,
    is_recurring: bool = False,
    recurrence_days: list = None,
    category: str = "other",
    notes: str = None,
) -> int:
    conn = get_conn()
    c = conn.cursor()
    c.execute(
        """INSERT INTO events
           (user_id, title, event_date, start_time, end_time,
            is_fixed, is_recurring, recurrence_days, category, notes)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
        (
            user_id, title, event_date, start_time, end_time,
            1 if is_fixed else 0,
            1 if is_recurring else 0,
            json.dumps(recurrence_days or []),
            category, notes,
        ),
    )
    event_id = c.lastrowid
    conn.commit()
    conn.close()
    return event_id


def get_events_for_day(user_id: int, date_str: str, day_of_week: int) -> list:
    conn = get_conn()
    c = conn.cursor()
    # Fetch specific-date events + all recurring events, filter recurring in Python
    # (LIKE '%N%' gives false positives, e.g. day 1 matches [10,11])
    c.execute(
        """SELECT * FROM events
           WHERE user_id = ?
             AND (event_date = ? OR is_recurring = 1)
           ORDER BY start_time""",
        (user_id, date_str),
    )
    rows = []
    for r in c.fetchall():
        row = dict(r)
        if row["is_recurring"]:
            try:
                days = json.loads(row["recurrence_days"] or "[]")
                if day_of_week not in days:
                    continue
            except Exception:
                continue
        rows.append(row)
    conn.close()
    return rows


def get_all_events(user_id: int) -> list:
    conn = get_conn()
    c = conn.cursor()
    c.execute(
        "SELECT * FROM events WHERE user_id = ? ORDER BY event_date, start_time",
        (user_id,),
    )
    rows = [dict(r) for r in c.fetchall()]
    conn.close()
    return rows


def delete_event(event_id: int, user_id: int):
    conn = get_conn()
    conn.execute("DELETE FROM events WHERE id = ? AND user_id = ?", (event_id, user_id))
    conn.commit()
    conn.close()


def delete_all_events(user_id: int):
    conn = get_conn()
    conn.execute("DELETE FROM events WHERE user_id = ?", (user_id,))
    conn.commit()
    conn.close()
