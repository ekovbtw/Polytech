import requests

BASE_URL = "https://ruz.spbstu.ru/api/v1/ruz"
TIMEOUT = 30  # seconds per request

_session = requests.Session()
_session.headers.update({
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36"
    )
})

# Module-level caches to avoid repeated API calls across function invocations
_teachers_cache = None  # full_name -> teacher_id (loaded lazily, once)
_groups_cache = None    # group_name -> group_id (loaded lazily, once)
_buildings_cache = None  # building_name -> building_id
_rooms_cache = {}       # building_id -> {room_name -> room_id}


def _get(url, params=None):
    resp = _session.get(url, params=params, timeout=TIMEOUT)
    resp.raise_for_status()
    return resp.json()


def _load_teachers():
    global _teachers_cache
    if _teachers_cache is not None:
        return _teachers_cache

    _teachers_cache = {}
    teachers = _get(f"{BASE_URL}/teachers", params={"q": ""}).get("teachers") or []
    for t in teachers:
        name = t.get("full_name", "")
        if name:
            _teachers_cache[name] = t["id"]
    return _teachers_cache


def _load_groups():
    global _groups_cache
    if _groups_cache is not None:
        return _groups_cache

    _groups_cache = {}
    faculties = _get(f"{BASE_URL}/faculties").get("faculties") or []

    all_groups = []
    for faculty in faculties:
        groups = _get(f"{BASE_URL}/faculties/{faculty['id']}/groups").get("groups") or []
        all_groups.extend(groups)

    # Pass 1: register every exact name
    for g in all_groups:
        _groups_cache[g.get("name", "")] = g["id"]

    # Pass 2: register base names (without _YEAR suffix) only when no exact entry exists
    for g in all_groups:
        g_name = g.get("name", "")
        if "_" in g_name:
            base = g_name.rsplit("_", 1)[0]
            if base not in _groups_cache:
                _groups_cache[base] = g["id"]

    return _groups_cache


def _load_buildings():
    global _buildings_cache
    if _buildings_cache is not None:
        return _buildings_cache

    _buildings_cache = {}
    buildings = _get(f"{BASE_URL}/buildings").get("buildings") or []
    for b in buildings:
        _buildings_cache[b["name"]] = b["id"]
    return _buildings_cache


def _load_rooms(building_id):
    if building_id not in _rooms_cache:
        rooms = _get(f"{BASE_URL}/buildings/{building_id}/rooms").get("rooms") or []
        _rooms_cache[building_id] = {r["name"]: r["id"] for r in rooms}
    return _rooms_cache[building_id]


def _format_lesson(lesson):
    time_start = lesson.get("time_start") or "None"
    time_end = lesson.get("time_end") or "None"
    subject = lesson.get("subject") or "None"

    type_obj = lesson.get("typeObj")
    lesson_type = (type_obj.get("name") if type_obj else None) or "None"

    teachers = lesson.get("teachers") or []
    teacher_str = ",".join(t.get("full_name", "None") for t in teachers) if teachers else "None"

    groups = lesson.get("groups") or []
    group_str = ",".join(g.get("name", "None") for g in groups) if groups else "None"

    auditories = lesson.get("auditories") or []
    place_str = ",".join(a.get("name", "None") for a in auditories) if auditories else "None"

    return (
        f"Time:{time_start}-{time_end}\n"
        f"Subject:{subject}\n"
        f"Type:{lesson_type}\n"
        f"Teacher:{teacher_str}\n"
        f"Groups:{group_str}\n"
        f"Place:{place_str}\n"
    )


def _extract_schedule(days, date):
    for day in days:
        if day.get("date") == date:
            lessons = day.get("lessons") or []
            if not lessons:
                return None
            return "".join(_format_lesson(lesson) for lesson in lessons)
    return None


def get_teacher_schedule(teacher_name, date):
    teachers = _load_teachers()
    teacher_id = teachers.get(teacher_name)
    if teacher_id is None:
        return None

    data = _get(f"{BASE_URL}/teachers/{teacher_id}/scheduler", params={"date": date})
    return _extract_schedule(data.get("days") or [], date)


def get_room_schedule(building_name, room_name, date):
    buildings = _load_buildings()
    building_id = buildings.get(building_name)
    if building_id is None:
        return None

    rooms = _load_rooms(building_id)
    room_id = rooms.get(room_name)
    if room_id is None:
        return None

    data = _get(
        f"{BASE_URL}/buildings/{building_id}/rooms/{room_id}/scheduler",
        params={"date": date}
    )
    return _extract_schedule(data.get("days") or [], date)


def get_group_schedule(group_name, date):
    groups = _load_groups()
    group_id = groups.get(group_name)
    if group_id is None:
        return None

    data = _get(f"{BASE_URL}/scheduler/{group_id}", params={"date": date})
    return _extract_schedule(data.get("days") or [], date)
