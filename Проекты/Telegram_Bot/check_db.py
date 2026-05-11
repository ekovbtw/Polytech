import sqlite3

conn = sqlite3.connect("scheduler.db")
conn.row_factory = sqlite3.Row

users = [dict(r) for r in conn.execute("SELECT * FROM users").fetchall()]
events_count = conn.execute("SELECT COUNT(*) FROM events").fetchone()[0]
print(f"Users: {len(users)}, Total events: {events_count}")
for u in users:
    print(f"  uid={u['user_id']} setup={u['setup_done']} wake={u['wake_time']} sleep={u['sleep_time']}")

events = [dict(r) for r in conn.execute("SELECT * FROM events ORDER BY start_time").fetchall()]
print("\nEvents:")
for e in events:
    print(f"  [{e['id']}] recurring={e['is_recurring']} days={e['recurrence_days']} {e['start_time']}-{e['end_time']} | {e['title']}")
conn.close()
