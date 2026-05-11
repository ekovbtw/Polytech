"""One-time script: add missing base schedule for existing users."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import database as db
import ai_scheduler as ai

conn = db.get_conn()
users = [dict(r) for r in conn.execute("SELECT * FROM users WHERE setup_done=1").fetchall()]
conn.close()

for u in users:
    uid = u["user_id"]
    user_info = {
        "wake_time":  u.get("wake_time",  "07:00"),
        "sleep_time": u.get("sleep_time", "23:00"),
        "uni_start":  u.get("uni_start",  "09:00"),
        "uni_end":    u.get("uni_end",    "17:00"),
    }
    events = ai._fallback_schedule(user_info)
    added = 0
    for ev in events:
        try:
            db.add_event(
                user_id=uid,
                title=ev["title"],
                start_time=ev["start_time"],
                end_time=ev["end_time"],
                is_recurring=ev.get("is_recurring", True),
                recurrence_days=ev.get("recurrence_days", [0,1,2,3,4,5,6]),
                category=ev.get("category", "other"),
                notes=ev.get("notes", "") or "",
            )
            added += 1
        except Exception as e:
            print(f"  skip {ev['title']}: {e}")
    print(f"uid={uid}: added {added} base events")

print("Done.")
