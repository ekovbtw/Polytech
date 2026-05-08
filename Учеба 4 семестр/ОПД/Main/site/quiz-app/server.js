const express  = require('express');
const Database = require('better-sqlite3');
const bcrypt   = require('bcryptjs');
const jwt      = require('jsonwebtoken');
const path     = require('path');
const fs       = require('fs');
const multer   = require('multer');

const app  = express();
const PORT = process.env.PORT || 3000;
const JWT_SECRET = process.env.JWT_SECRET || 'quizmaster_secret_key_change_in_production';

app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// ─────────────────── Uploads dirs ───────────────────
const uploadsDir        = path.join(__dirname, 'public', 'uploads', 'avatars');
const qImagesDir        = path.join(__dirname, 'public', 'uploads', 'question-images');
const answerFilesDir    = path.join(__dirname, 'public', 'uploads', 'answer-files');
fs.mkdirSync(uploadsDir,     { recursive: true });
fs.mkdirSync(qImagesDir,     { recursive: true });
fs.mkdirSync(answerFilesDir, { recursive: true });

const avatarStorage = multer.diskStorage({
  destination: uploadsDir,
  filename: (req, file, cb) => {
    const ext = path.extname(file.originalname).toLowerCase() || '.jpg';
    cb(null, `${req.user.id}_${Date.now()}${ext}`);
  }
});
const avatarUpload = multer({
  storage: avatarStorage,
  limits: { fileSize: 3 * 1024 * 1024 },
  fileFilter: (req, file, cb) => {
    if (file.mimetype.startsWith('image/')) cb(null, true);
    else cb(new Error('Только изображения'));
  }
});

const qImageStorage = multer.diskStorage({
  destination: qImagesDir,
  filename: (req, file, cb) => {
    const ext = path.extname(file.originalname).toLowerCase() || '.jpg';
    cb(null, `qi_${Date.now()}_${Math.random().toString(36).slice(2)}${ext}`);
  }
});
const qImageUpload = multer({
  storage: qImageStorage,
  limits: { fileSize: 10 * 1024 * 1024 },
  fileFilter: (req, file, cb) => {
    if (file.mimetype.startsWith('image/')) cb(null, true);
    else cb(new Error('Только изображения'));
  }
});

const answerFileStorage = multer.diskStorage({
  destination: answerFilesDir,
  filename: (req, file, cb) => {
    const ext = path.extname(file.originalname).toLowerCase() || '.bin';
    cb(null, `af_${Date.now()}_${Math.random().toString(36).slice(2)}${ext}`);
  }
});
const answerFileUpload = multer({
  storage: answerFileStorage,
  limits: { fileSize: 50 * 1024 * 1024 }
});

// ─────────────────── База данных ───────────────────
const db = new Database(path.join(__dirname, 'quiz.db'));

db.exec(`
  PRAGMA foreign_keys = ON;
  PRAGMA journal_mode = WAL;

  CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT    UNIQUE NOT NULL COLLATE NOCASE,
    email         TEXT    UNIQUE NOT NULL COLLATE NOCASE,
    password_hash TEXT    NOT NULL,
    role          TEXT    NOT NULL CHECK(role IN ('student', 'teacher')),
    is_new        INTEGER DEFAULT 1,
    created_at    DATETIME DEFAULT CURRENT_TIMESTAMP
  );

  CREATE TABLE IF NOT EXISTS tests (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    code               TEXT    UNIQUE NOT NULL,
    title              TEXT    NOT NULL,
    description        TEXT    DEFAULT '',
    teacher_id         INTEGER NOT NULL,
    password           TEXT    DEFAULT NULL,
    is_anonymous       INTEGER DEFAULT 0,
    show_results       INTEGER DEFAULT 1,
    shuffle_questions  INTEGER DEFAULT 0,
    created_at         DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at         DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (teacher_id) REFERENCES users(id)
  );

  CREATE TABLE IF NOT EXISTS questions (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    test_id        INTEGER NOT NULL,
    question_text  TEXT    NOT NULL,
    question_order INTEGER DEFAULT 0,
    FOREIGN KEY (test_id) REFERENCES tests(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS answers (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    question_id INTEGER NOT NULL,
    answer_text TEXT    NOT NULL,
    is_correct  INTEGER DEFAULT 0,
    FOREIGN KEY (question_id) REFERENCES questions(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS submissions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    test_id         INTEGER NOT NULL,
    user_id         INTEGER DEFAULT NULL,
    student_name    TEXT    DEFAULT 'Аноним',
    score           INTEGER DEFAULT 0,
    total_questions INTEGER DEFAULT 0,
    completed_at    DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (test_id) REFERENCES tests(id),
    FOREIGN KEY (user_id) REFERENCES users(id)
  );

  CREATE TABLE IF NOT EXISTS submission_answers (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id INTEGER NOT NULL,
    question_id   INTEGER NOT NULL,
    answer_id     INTEGER DEFAULT NULL,
    FOREIGN KEY (submission_id) REFERENCES submissions(id) ON DELETE CASCADE,
    FOREIGN KEY (question_id)   REFERENCES questions(id),
    FOREIGN KEY (answer_id)     REFERENCES answers(id)
  );

  CREATE TABLE IF NOT EXISTS classes (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL,
    code       TEXT    UNIQUE NOT NULL,
    password   TEXT    DEFAULT NULL,
    teacher_id INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (teacher_id) REFERENCES users(id)
  );

  CREATE TABLE IF NOT EXISTS class_members (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    class_id   INTEGER NOT NULL,
    student_id INTEGER NOT NULL,
    joined_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(class_id, student_id),
    FOREIGN KEY (class_id)   REFERENCES classes(id) ON DELETE CASCADE,
    FOREIGN KEY (student_id) REFERENCES users(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS class_tests (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    class_id    INTEGER NOT NULL,
    test_id     INTEGER NOT NULL,
    assigned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(class_id, test_id),
    FOREIGN KEY (class_id) REFERENCES classes(id) ON DELETE CASCADE,
    FOREIGN KEY (test_id)  REFERENCES tests(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS notifications (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id    INTEGER NOT NULL,
    type       TEXT    NOT NULL,
    title      TEXT    NOT NULL,
    message    TEXT    NOT NULL,
    test_id    INTEGER DEFAULT NULL,
    is_read    INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (test_id) REFERENCES tests(id)  ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS question_banks (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    teacher_id  INTEGER NOT NULL,
    name        TEXT    NOT NULL,
    description TEXT    DEFAULT '',
    is_public   INTEGER DEFAULT 0,
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (teacher_id) REFERENCES users(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS bank_questions (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    bank_id        INTEGER NOT NULL,
    question_text  TEXT    NOT NULL,
    question_order INTEGER DEFAULT 0,
    created_at     DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (bank_id) REFERENCES question_banks(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS bank_answers (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    bank_question_id INTEGER NOT NULL,
    answer_text      TEXT    NOT NULL,
    is_correct       INTEGER DEFAULT 0,
    FOREIGN KEY (bank_question_id) REFERENCES bank_questions(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS bank_favorites (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    teacher_id INTEGER NOT NULL,
    bank_id    INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(teacher_id, bank_id),
    FOREIGN KEY (teacher_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (bank_id)    REFERENCES question_banks(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS friend_requests (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    from_user_id INTEGER NOT NULL,
    to_user_id   INTEGER NOT NULL,
    status       TEXT    NOT NULL DEFAULT 'pending' CHECK(status IN ('pending','accepted')),
    created_at   DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(from_user_id, to_user_id),
    FOREIGN KEY (from_user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (to_user_id)   REFERENCES users(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS class_invites (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    class_id   INTEGER NOT NULL,
    user_id    INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(class_id, user_id),
    FOREIGN KEY (class_id) REFERENCES classes(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id)  REFERENCES users(id)   ON DELETE CASCADE
  );
`);

// ── Migrations: add new columns to existing tables ──
const addIfMissing = (table, col, def) => {
  const cols = db.pragma(`table_info(${table})`).map(r => r.name);
  if (!cols.includes(col)) db.exec(`ALTER TABLE ${table} ADD COLUMN ${col} ${def}`);
};
addIfMissing('users',       'first_name',          'TEXT DEFAULT ""');
addIfMissing('users',       'last_name',            'TEXT DEFAULT ""');
addIfMissing('users',       'institution',          'TEXT DEFAULT ""');
addIfMissing('users',       'avatar_url',           'TEXT DEFAULT NULL');
addIfMissing('users',       'show_profile',         'INTEGER DEFAULT 1');
addIfMissing('users',       'allow_invites',        'INTEGER DEFAULT 1');
addIfMissing('users',       'allow_friend_requests', 'INTEGER DEFAULT 1');
addIfMissing('notifications', 'link_url',            'TEXT DEFAULT NULL');
addIfMissing('tests',              'deadline_at',          'DATETIME DEFAULT NULL');
addIfMissing('tests',              'time_limit_minutes',   'INTEGER DEFAULT NULL');
addIfMissing('tests',              'profile_hidden',       'INTEGER DEFAULT 0');
addIfMissing('submissions',        'is_hidden',            'INTEGER DEFAULT 0');
addIfMissing('submissions',        'result_hidden',        'INTEGER DEFAULT 0');
// Question types & media
addIfMissing('questions',          'type',                 "TEXT DEFAULT 'single_choice'");
addIfMissing('questions',          'image_url',            'TEXT DEFAULT NULL');
addIfMissing('questions',          'answer_tolerance',     'REAL DEFAULT NULL');
addIfMissing('questions',          'points',               'INTEGER DEFAULT 1');
addIfMissing('submissions',        'total_points',         'INTEGER DEFAULT NULL');
// Submission answer types
addIfMissing('submission_answers', 'text_answer',          'TEXT DEFAULT NULL');
addIfMissing('submission_answers', 'file_url',             'TEXT DEFAULT NULL');
addIfMissing('submission_answers', 'needs_review',         'INTEGER DEFAULT 0');

// ─────────────────── Планировщик уведомлений ───────────────────
const DEADLINE_THRESHOLDS = [
  { ms: 24 * 60 * 60 * 1000, type: 'deadline_24h', label: '24 часа'  },
  { ms: 12 * 60 * 60 * 1000, type: 'deadline_12h', label: '12 часов' },
  { ms:  3 * 60 * 60 * 1000, type: 'deadline_3h',  label: '3 часа'   },
  { ms:  1 * 60 * 60 * 1000, type: 'deadline_1h',  label: '1 час'    },
];

function checkDeadlineNotifications() {
  try {
    const now   = Date.now();
    const tests = db.prepare(`
      SELECT t.id, t.title, t.deadline_at, t.teacher_id
      FROM tests t
      WHERE t.deadline_at IS NOT NULL
        AND datetime(t.deadline_at) > datetime('now')
    `).all();

    for (const test of tests) {
      const remaining = new Date(test.deadline_at).getTime() - now;

      for (const thr of DEADLINE_THRESHOLDS) {
        if (remaining > thr.ms) continue; // not yet in this window

        // Students in classes that have this test
        const students = db.prepare(`
          SELECT DISTINCT cm.student_id AS uid
          FROM class_tests ct
          JOIN class_members cm ON ct.class_id = cm.class_id
          WHERE ct.test_id = ?
        `).all(test.id).map(r => r.uid);

        const recipients = [...new Set([...students, test.teacher_id])];

        for (const uid of recipients) {
          const exists = db.prepare(
            'SELECT id FROM notifications WHERE user_id=? AND test_id=? AND type=?'
          ).get(uid, test.id, thr.type);
          if (exists) continue;

          const isTeacher = uid === test.teacher_id;
          db.prepare(
            'INSERT INTO notifications (user_id, type, title, message, test_id) VALUES (?,?,?,?,?)'
          ).run(
            uid, thr.type,
            `Дедлайн через ${thr.label}`,
            `${isTeacher ? 'Ваш тест' : 'Тест'} «${test.title}» закроется через ${thr.label}`,
            test.id
          );
        }
      }
    }
  } catch (e) { console.error('Notif check error:', e.message); }
}
checkDeadlineNotifications();
setInterval(checkDeadlineNotifications, 5 * 60 * 1000); // каждые 5 минут

// ─────────────────── Утилиты ───────────────────
function generateCode(len = 6) {
  const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  let code = '';
  for (let i = 0; i < len; i++) code += chars[Math.floor(Math.random() * chars.length)];
  return code;
}
function getUniqueCode() {
  let code;
  do { code = generateCode(6); }
  while (db.prepare('SELECT id FROM tests WHERE code = ?').get(code));
  return code;
}
function getUniqueClassCode() {
  let code;
  do { code = generateCode(8); }
  while (db.prepare('SELECT id FROM classes WHERE code = ?').get(code));
  return code;
}

// ─────────────────── Middleware ───────────────────
function auth(req, res, next) {
  const token = req.headers.authorization?.split(' ')[1];
  if (!token) return res.status(401).json({ error: 'Необходима авторизация' });
  try {
    req.user = jwt.verify(token, JWT_SECRET);
    next();
  } catch {
    res.status(401).json({ error: 'Недействительный токен' });
  }
}
function teacherOnly(req, res, next) {
  if (req.user.role !== 'teacher')
    return res.status(403).json({ error: 'Доступ только для преподавателей' });
  next();
}
function studentOnly(req, res, next) {
  if (req.user.role !== 'student')
    return res.status(403).json({ error: 'Доступ только для студентов' });
  next();
}

// ─────────────────── AUTH ───────────────────
app.post('/api/auth/register', (req, res) => {
  const { username, email, password, role } = req.body;
  if (!username || !email || !password || !role)
    return res.status(400).json({ error: 'Все поля обязательны' });
  if (!['student', 'teacher'].includes(role))
    return res.status(400).json({ error: 'Неверная роль' });
  const uname = username.trim();
  if (uname.length < 3 || uname.length > 30)
    return res.status(400).json({ error: 'Имя пользователя: от 3 до 30 символов' });
  if (!/^[a-zA-Z0-9_а-яА-ЯёЁ]+$/.test(uname))
    return res.status(400).json({ error: 'Имя может содержать только буквы, цифры и _' });
  if (password.length < 6)
    return res.status(400).json({ error: 'Пароль должен содержать не менее 6 символов' });
  if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email))
    return res.status(400).json({ error: 'Введите корректный email адрес' });
  const byName  = db.prepare('SELECT id FROM users WHERE username = ?').get(uname);
  const byEmail = db.prepare('SELECT id FROM users WHERE email = ?').get(email.toLowerCase());
  if (byName)  return res.status(400).json({ error: 'Пользователь с таким именем уже существует' });
  if (byEmail) return res.status(400).json({ error: 'Пользователь с таким email уже существует' });
  const hash = bcrypt.hashSync(password, 10);
  const r = db.prepare(
    'INSERT INTO users (username, email, password_hash, role) VALUES (?, ?, ?, ?)'
  ).run(uname, email.toLowerCase(), hash, role);
  const user = { id: r.lastInsertRowid, username: uname, role, is_new: 1 };
  const token = jwt.sign(user, JWT_SECRET, { expiresIn: '7d' });
  res.json({ token, user });
});

app.post('/api/auth/login', (req, res) => {
  const { login, password } = req.body;
  if (!login || !password)
    return res.status(400).json({ error: 'Введите логин и пароль' });
  const user = db.prepare(
    'SELECT * FROM users WHERE username = ? OR email = ?'
  ).get(login, login.toLowerCase());
  if (!user || !bcrypt.compareSync(password, user.password_hash))
    return res.status(400).json({ error: 'Неверный логин или пароль' });
  const payload = { id: user.id, username: user.username, role: user.role, is_new: user.is_new };
  const token = jwt.sign(payload, JWT_SECRET, { expiresIn: '7d' });
  res.json({ token, user: payload });
});

app.post('/api/auth/onboarded', auth, (req, res) => {
  db.prepare('UPDATE users SET is_new = 0 WHERE id = ?').run(req.user.id);
  res.json({ ok: true });
});

// ─────────────────── PROFILE ───────────────────
app.get('/api/profile', auth, (req, res) => {
  const u = db.prepare(
    'SELECT id, username, first_name, last_name, institution, avatar_url, role, show_profile, allow_invites, allow_friend_requests FROM users WHERE id = ?'
  ).get(req.user.id);
  res.json(u);
});

app.put('/api/profile', auth, (req, res) => {
  const { first_name, last_name, institution, show_profile, allow_invites, allow_friend_requests } = req.body;
  db.prepare(`UPDATE users SET
    first_name           = COALESCE(?, first_name),
    last_name            = COALESCE(?, last_name),
    institution          = COALESCE(?, institution),
    show_profile         = COALESCE(?, show_profile),
    allow_invites        = COALESCE(?, allow_invites),
    allow_friend_requests = COALESCE(?, allow_friend_requests)
    WHERE id = ?`
  ).run(
    first_name            !== undefined ? String(first_name).slice(0, 60)   : null,
    last_name             !== undefined ? String(last_name).slice(0, 60)    : null,
    institution           !== undefined ? String(institution).slice(0, 120) : null,
    show_profile          !== undefined ? (show_profile          ? 1 : 0) : null,
    allow_invites         !== undefined ? (allow_invites         ? 1 : 0) : null,
    allow_friend_requests !== undefined ? (allow_friend_requests ? 1 : 0) : null,
    req.user.id
  );
  res.json({ ok: true });
});

app.post('/api/profile/avatar', auth, (req, res, next) => {
  avatarUpload.single('avatar')(req, res, err => {
    if (err) return res.status(400).json({ error: err.message });
    if (!req.file) return res.status(400).json({ error: 'Файл не загружен' });
    const url = `/uploads/avatars/${req.file.filename}`;
    db.prepare('UPDATE users SET avatar_url = ? WHERE id = ?').run(url, req.user.id);
    res.json({ avatar_url: url });
  });
});

app.get('/api/profile/:username', (req, res) => {
  const u = db.prepare(
    'SELECT id, username, first_name, last_name, institution, avatar_url, role, show_profile, allow_invites, allow_friend_requests FROM users WHERE username = ?'
  ).get(req.params.username);
  if (!u) return res.status(404).json({ error: 'Пользователь не найден' });
  res.json(u);
});

// Public history for another user's profile
app.get('/api/profile/:username/submissions', (req, res) => {
  const u = db.prepare(
    'SELECT id, role, show_profile FROM users WHERE username = ?'
  ).get(req.params.username);
  if (!u) return res.status(404).json({ error: 'Пользователь не найден' });
  if (!u.show_profile) return res.status(403).json({ error: 'История скрыта' });

  if (u.role === 'student') {
    const subs = db.prepare(`
      SELECT s.id, s.score, s.total_questions, s.completed_at, s.student_name,
             s.is_hidden, s.result_hidden,
             t.title, t.code, t.show_results
      FROM submissions s
      JOIN tests t ON s.test_id = t.id
      WHERE s.user_id = ? AND s.is_hidden = 0
      ORDER BY s.completed_at DESC
      LIMIT 50
    `).all(u.id);
    res.json(subs);
  } else {
    // teacher — return publicly visible tests
    const tests = db.prepare(`
      SELECT t.id, t.code, t.title, t.created_at, t.profile_hidden AS is_hidden,
        (SELECT COUNT(*) FROM questions   WHERE test_id = t.id) AS question_count,
        (SELECT COUNT(*) FROM submissions WHERE test_id = t.id) AS submission_count
      FROM tests t
      WHERE t.teacher_id = ? AND (t.profile_hidden IS NULL OR t.profile_hidden = 0)
      ORDER BY t.created_at DESC
      LIMIT 50
    `).all(u.id);
    res.json({ role: 'teacher', tests });
  }
});

// ─────────────────── TESTS (teacher) ───────────────────
app.get('/api/tests', auth, teacherOnly, (req, res) => {
  const tests = db.prepare(`
    SELECT t.*,
      (SELECT COUNT(*) FROM questions   WHERE test_id = t.id) AS question_count,
      (SELECT COUNT(*) FROM submissions WHERE test_id = t.id) AS submission_count
    FROM tests t WHERE t.teacher_id = ?
    ORDER BY t.created_at DESC
  `).all(req.user.id);
  res.json(tests);
});

app.get('/api/tests/:id', auth, teacherOnly, (req, res) => {
  const test = db.prepare(
    'SELECT * FROM tests WHERE id = ? AND teacher_id = ?'
  ).get(req.params.id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  const questions = db.prepare(
    'SELECT * FROM questions WHERE test_id = ? ORDER BY question_order'
  ).all(req.params.id);
  questions.forEach(q => {
    q.answers = db.prepare('SELECT * FROM answers WHERE question_id = ?').all(q.id);
  });
  res.json({ ...test, questions });
});

// PATCH visibility of teacher's test on profile
app.patch('/api/tests/:id/profile-visibility', auth, teacherOnly, (req, res) => {
  const test = db.prepare('SELECT id FROM tests WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  const hidden = req.body.hidden ? 1 : 0;
  db.prepare('UPDATE tests SET profile_hidden = ? WHERE id = ?').run(hidden, req.params.id);
  res.json({ ok: true });
});

function validateTestBody(body) {
  const { title, questions } = body;
  if (!title || !title.trim()) return 'Название теста обязательно';
  if (!questions || questions.length < 1) return 'Добавьте хотя бы один вопрос';
  for (const q of questions) {
    if (!q.question_text?.trim()) return 'Текст вопроса не может быть пустым';
    const type = q.type || 'single_choice';
    if (type === 'single_choice') {
      if (!q.answers || q.answers.length < 2) return 'Вопрос с выбором ответа должен иметь минимум 2 варианта';
      if (!q.answers.some(a => a.is_correct)) return 'Отметьте правильный вариант ответа для каждого вопроса';
    } else if (type === 'multiple_choice') {
      if (!q.answers || q.answers.length < 2) return 'Вопрос с множественным выбором должен иметь минимум 2 варианта';
      if (q.answers.filter(a => a.is_correct).length < 2) return 'Отметьте минимум 2 правильных варианта для вопроса с множественным выбором';
    } else if (type === 'text_answer') {
      if (!q.answers?.some(a => a.answer_text?.trim())) return 'Укажите хотя бы один правильный ответ для текстового вопроса';
    } else if (type === 'number_answer') {
      const num = q.answers?.[0]?.answer_text?.trim();
      if (!num || isNaN(parseFloat(num))) return 'Укажите правильное число для числового вопроса';
    }
    // file_answer: no validation required
  }
  return null;
}

app.post('/api/tests', auth, teacherOnly, (req, res) => {
  const err = validateTestBody(req.body);
  if (err) return res.status(400).json({ error: err });
  const { title, description, questions, password, is_anonymous, show_results,
          shuffle_questions, deadline_at, time_limit_minutes } = req.body;
  const code = getUniqueCode();
  const save = db.transaction(() => {
    const t = db.prepare(`
      INSERT INTO tests (code, title, description, teacher_id, password, is_anonymous,
        show_results, shuffle_questions, deadline_at, time_limit_minutes)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(code, title.trim(), (description||'').trim(), req.user.id,
           password || null, is_anonymous ? 1 : 0,
           show_results !== false ? 1 : 0, shuffle_questions ? 1 : 0,
           deadline_at || null, time_limit_minutes || null);
    const tid = t.lastInsertRowid;
    questions.forEach((q, i) => {
      const qr = db.prepare(
        'INSERT INTO questions (test_id, question_text, question_order, type, image_url, answer_tolerance, points) VALUES (?, ?, ?, ?, ?, ?, ?)'
      ).run(tid, q.question_text.trim(), i, q.type || 'single_choice', q.image_url || null, q.answer_tolerance ?? null, q.points || 1);
      (q.answers || []).forEach(a => {
        if (a.answer_text?.trim())
          db.prepare('INSERT INTO answers (question_id, answer_text, is_correct) VALUES (?, ?, ?)')
            .run(qr.lastInsertRowid, a.answer_text.trim(), a.is_correct ? 1 : 0);
      });
    });
    db.prepare('UPDATE users SET is_new = 0 WHERE id = ?').run(req.user.id);
    return tid;
  });
  try {
    const tid = save();
    res.json(db.prepare('SELECT * FROM tests WHERE id = ?').get(tid));
  } catch (e) {
    console.error('POST /api/tests error:', e.message);
    res.status(500).json({ error: 'Ошибка создания теста: ' + e.message });
  }
});

app.put('/api/tests/:id', auth, teacherOnly, (req, res) => {
  const test = db.prepare('SELECT * FROM tests WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  const err = validateTestBody(req.body);
  if (err) return res.status(400).json({ error: err });
  const { title, description, questions, password, is_anonymous, show_results,
          shuffle_questions, deadline_at, time_limit_minutes } = req.body;
  try {
    db.transaction(() => {
      db.prepare(`UPDATE tests SET title=?, description=?, password=?, is_anonymous=?,
        show_results=?, shuffle_questions=?, deadline_at=?, time_limit_minutes=?,
        updated_at=CURRENT_TIMESTAMP WHERE id=?`
      ).run(title.trim(), (description||'').trim(), password||null,
            is_anonymous?1:0, show_results!==false?1:0, shuffle_questions?1:0,
            deadline_at||null, time_limit_minutes||null, req.params.id);

      // Remove old submission_answers that reference this test's questions
      // (preserves submission totals/scores, only per-question detail is cleared)
      db.prepare(`
        DELETE FROM submission_answers
        WHERE question_id IN (SELECT id FROM questions WHERE test_id = ?)
      `).run(req.params.id);

      db.prepare('DELETE FROM questions WHERE test_id = ?').run(req.params.id);

      questions.forEach((q, i) => {
        const qr = db.prepare(
          'INSERT INTO questions (test_id, question_text, question_order, type, image_url, answer_tolerance, points) VALUES (?, ?, ?, ?, ?, ?, ?)'
        ).run(req.params.id, q.question_text.trim(), i, q.type || 'single_choice', q.image_url || null, q.answer_tolerance ?? null, q.points || 1);
        (q.answers || []).forEach(a => {
          if (a.answer_text?.trim())
            db.prepare('INSERT INTO answers (question_id, answer_text, is_correct) VALUES (?, ?, ?)')
              .run(qr.lastInsertRowid, a.answer_text.trim(), a.is_correct ? 1 : 0);
        });
      });
    })();
    res.json(db.prepare('SELECT * FROM tests WHERE id = ?').get(req.params.id));
  } catch (e) {
    console.error('PUT /api/tests error:', e.message);
    res.status(500).json({ error: 'Ошибка сохранения теста: ' + e.message });
  }
});

app.delete('/api/tests/:id', auth, teacherOnly, (req, res) => {
  const test = db.prepare('SELECT * FROM tests WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  db.prepare('DELETE FROM tests WHERE id = ?').run(req.params.id);
  res.json({ ok: true });
});

// ─────────────────── TESTS (public) ───────────────────
app.get('/api/public/tests/:code', (req, res) => {
  const test = db.prepare(`
    SELECT id, code, title, description, is_anonymous, show_results, shuffle_questions,
      deadline_at, time_limit_minutes,
      CASE WHEN password IS NOT NULL AND password != '' THEN 1 ELSE 0 END AS has_password,
      (SELECT COUNT(*) FROM questions WHERE test_id = tests.id) AS question_count
    FROM tests WHERE code = ?
  `).get(req.params.code.toUpperCase());
  if (!test) return res.status(404).json({ error: 'Тест не найден. Проверьте код.' });
  if (test.deadline_at && new Date() > new Date(test.deadline_at))
    return res.status(403).json({ error: 'Срок прохождения теста истёк' });
  res.json(test);
});

app.post('/api/public/tests/:id/questions', (req, res) => {
  const { password } = req.body;
  const test = db.prepare('SELECT * FROM tests WHERE id = ?').get(req.params.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  if (test.deadline_at && new Date() > new Date(test.deadline_at))
    return res.status(403).json({ error: 'Срок прохождения теста истёк' });
  if (test.password && test.password !== '') {
    if (!password || password !== test.password)
      return res.status(403).json({ error: 'Неверный пароль к тесту' });
  }
  let questions = db.prepare(
    'SELECT id, question_text, question_order, type, image_url, points FROM questions WHERE test_id = ? ORDER BY question_order'
  ).all(req.params.id);
  if (test.shuffle_questions) {
    for (let i = questions.length - 1; i > 0; i--) {
      const j = Math.floor(Math.random() * (i + 1));
      [questions[i], questions[j]] = [questions[j], questions[i]];
    }
  }
  questions = questions.map(q => ({
    ...q,
    // for choice types send answer options; for others no options needed
    answers: (q.type === 'single_choice' || q.type === 'multiple_choice' || !q.type)
      ? db.prepare('SELECT id, answer_text FROM answers WHERE question_id = ?').all(q.id)
      : []
  }));
  res.json({
    test: {
      id: test.id, title: test.title, is_anonymous: test.is_anonymous,
      show_results: test.show_results, time_limit_minutes: test.time_limit_minutes,
      deadline_at: test.deadline_at
    },
    questions
  });
});

// ─────────────────── SUBMIT ───────────────────
app.post('/api/submissions', (req, res) => {
  const { test_id, answers, password, student_name, user_id } = req.body;
  if (!test_id) return res.status(400).json({ error: 'Не указан тест' });
  const test = db.prepare('SELECT * FROM tests WHERE id = ?').get(test_id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  if (test.deadline_at && new Date() > new Date(test.deadline_at))
    return res.status(403).json({ error: 'Срок прохождения теста истёк' });
  if (test.password && test.password !== '') {
    if (!password || password !== test.password)
      return res.status(403).json({ error: 'Неверный пароль' });
  }
  const questions = db.prepare('SELECT * FROM questions WHERE test_id = ?').all(test_id);
  const normalize = s => String(s).trim().toLowerCase().replace(/\s+/g, ' ');
  let score = 0;
  const totalPoints = questions.reduce((s, q) => s + (q.points || 1), 0);
  const subId = db.transaction(() => {
    const sr = db.prepare(`
      INSERT INTO submissions (test_id, user_id, student_name, total_questions, total_points)
      VALUES (?, ?, ?, ?, ?)
    `).run(test_id, user_id || null, student_name || 'Аноним', questions.length, totalPoints);
    const sid = sr.lastInsertRowid;
    for (const q of questions) {
      const ans  = answers?.[q.id];
      const type = q.type || 'single_choice';
      const pts  = q.points || 1;
      let answerId   = null;
      let textAnswer = null;
      let fileUrl    = null;
      let needsReview = 0;

      if (type === 'single_choice') {
        answerId = typeof ans === 'object' ? (ans?.answer_id ?? null) : (ans ?? null);
        if (answerId) {
          const a = db.prepare('SELECT is_correct FROM answers WHERE id=? AND question_id=?').get(answerId, q.id);
          if (a?.is_correct) score += pts;
        }
      } else if (type === 'multiple_choice') {
        const selectedIds = Array.isArray(ans?.answer_ids) ? ans.answer_ids.map(Number) : [];
        if (selectedIds.length) {
          textAnswer = JSON.stringify(selectedIds);
          const correctIds = db.prepare('SELECT id FROM answers WHERE question_id=? AND is_correct=1').all(q.id).map(r => r.id);
          const correctSet  = new Set(correctIds);
          const selectedSet = new Set(selectedIds);
          const exact = correctSet.size === selectedSet.size && correctIds.every(id => selectedSet.has(id));
          if (exact) score += pts;
        }
      } else if (type === 'text_answer') {
        textAnswer = (typeof ans === 'object' ? ans?.text : ans) ?? null;
        if (textAnswer?.trim()) {
          const correctList = db.prepare('SELECT answer_text FROM answers WHERE question_id=? AND is_correct=1').all(q.id);
          if (correctList.some(c => normalize(c.answer_text) === normalize(textAnswer))) score += pts;
        }
      } else if (type === 'number_answer') {
        textAnswer = (typeof ans === 'object' ? ans?.text : ans) ?? null;
        if (textAnswer !== null) {
          const correctRow = db.prepare('SELECT answer_text FROM answers WHERE question_id=? AND is_correct=1 LIMIT 1').get(q.id);
          if (correctRow) {
            const diff = Math.abs(parseFloat(textAnswer) - parseFloat(correctRow.answer_text));
            const tol  = q.answer_tolerance ?? 0;
            if (!isNaN(diff) && diff <= tol) score += pts;
          }
        }
      } else if (type === 'file_answer') {
        fileUrl     = (typeof ans === 'object' ? ans?.file_url : null) ?? null;
        needsReview = fileUrl ? 1 : 0;
      }

      db.prepare(
        'INSERT INTO submission_answers (submission_id, question_id, answer_id, text_answer, file_url, needs_review) VALUES (?,?,?,?,?,?)'
      ).run(sid, q.id, answerId, textAnswer, fileUrl, needsReview);
    }
    db.prepare('UPDATE submissions SET score = ? WHERE id = ?').run(score, sid);
    return sid;
  })();
  res.json({ submission_id: subId, score, total: questions.length, total_points: totalPoints, show_results: test.show_results === 1 });
});

app.get('/api/submissions/:id', (req, res) => {
  const sub = db.prepare(`
    SELECT s.*, t.title, t.show_results, t.id AS test_id
    FROM submissions s JOIN tests t ON s.test_id = t.id WHERE s.id = ?
  `).get(req.params.id);
  if (!sub) return res.status(404).json({ error: 'Результат не найден' });
  if (!sub.show_results) {
    return res.json({ title: sub.title, show_results: false, score: sub.score, total: sub.total_questions });
  }
  const detail = db.prepare(`
    SELECT sa.question_id, sa.answer_id, sa.text_answer, sa.file_url, sa.needs_review,
      q.question_text, q.type, q.image_url, q.answer_tolerance,
      a.answer_text AS selected_answer,
      a.is_correct  AS selected_is_correct,
      (SELECT answer_text FROM answers WHERE question_id = q.id AND is_correct = 1 LIMIT 1) AS correct_answer
    FROM submission_answers sa
    JOIN questions q ON sa.question_id = q.id
    LEFT JOIN answers a ON sa.answer_id = a.id
    WHERE sa.submission_id = ?
  `).all(req.params.id);

  // Compute is_correct for non-choice answer types
  const normalize = s => String(s).trim().toLowerCase().replace(/\s+/g, ' ');
  for (const d of detail) {
    const type = d.type || 'single_choice';
    if (type === 'multiple_choice') {
      let selectedIds = [];
      try { selectedIds = d.text_answer ? JSON.parse(d.text_answer) : []; } catch {}
      const correctRows = db.prepare('SELECT id, answer_text FROM answers WHERE question_id=? AND is_correct=1').all(d.question_id);
      const allRows     = db.prepare('SELECT id, answer_text FROM answers WHERE question_id=?').all(d.question_id);
      const correctSet  = new Set(correctRows.map(r => r.id));
      const selectedSet = new Set(selectedIds.map(Number));
      d.selected_is_correct = correctSet.size === selectedSet.size && correctRows.every(r => selectedSet.has(r.id)) ? 1 : 0;
      // Build human-readable labels
      d.selected_answer = allRows.filter(r => selectedSet.has(r.id)).map(r => r.answer_text).join(', ') || '— не выбрано';
      d.correct_answer  = correctRows.map(r => r.answer_text).join(', ');
    } else if (type === 'text_answer' && d.text_answer) {
      const list = db.prepare('SELECT answer_text FROM answers WHERE question_id=? AND is_correct=1').all(d.question_id);
      d.selected_is_correct = list.some(c => normalize(c.answer_text) === normalize(d.text_answer)) ? 1 : 0;
      d.selected_answer = d.text_answer;
      d.correct_answer  = list.map(c => c.answer_text).join(' / ');
    } else if (type === 'number_answer' && d.text_answer !== null) {
      const cr  = db.prepare('SELECT answer_text FROM answers WHERE question_id=? AND is_correct=1 LIMIT 1').get(d.question_id);
      const tol = d.answer_tolerance ?? 0;
      const diff = cr ? Math.abs(parseFloat(d.text_answer) - parseFloat(cr.answer_text)) : Infinity;
      d.selected_is_correct = (!isNaN(diff) && diff <= tol) ? 1 : 0;
      d.selected_answer = d.text_answer;
      d.correct_answer  = cr ? `${cr.answer_text}${tol > 0 ? ` ±${tol}` : ''}` : null;
    } else if (type === 'file_answer') {
      d.selected_is_correct = 0;
      d.selected_answer     = d.file_url || null;
    }
  }
  res.json({ ...sub, answers: detail });
});

// ─────────────────── STUDENT HISTORY ───────────────────
app.get('/api/student/submissions', auth, (req, res) => {
  const subs = db.prepare(`
    SELECT s.id, s.score, s.total_questions, s.completed_at, s.student_name,
           s.is_hidden, s.result_hidden,
           t.title, t.code, t.show_results
    FROM submissions s
    JOIN tests t ON s.test_id = t.id
    WHERE s.user_id = ?
    ORDER BY s.completed_at DESC
    LIMIT 50
  `).all(req.user.id);
  res.json(subs);
});

// PATCH visibility of a submission on student profile
app.patch('/api/student/submissions/:id/visibility', auth, (req, res) => {
  const sub = db.prepare('SELECT id FROM submissions WHERE id = ? AND user_id = ?').get(req.params.id, req.user.id);
  if (!sub) return res.status(404).json({ error: 'Попытка не найдена' });
  const { hide_test, hide_result } = req.body;
  if (hide_test !== undefined)
    db.prepare('UPDATE submissions SET is_hidden = ? WHERE id = ?').run(hide_test ? 1 : 0, req.params.id);
  if (hide_result !== undefined)
    db.prepare('UPDATE submissions SET result_hidden = ? WHERE id = ? ').run(hide_result ? 1 : 0, req.params.id);
  res.json({ ok: true });
});

// ─────────────────── RESULTS (teacher) ───────────────────
app.get('/api/tests/:id/results', auth, teacherOnly, (req, res) => {
  const test = db.prepare('SELECT * FROM tests WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  const subs = db.prepare(`
    SELECT s.*,
           u.username,
           u.first_name,
           u.last_name,
           u.email,
           u.institution
    FROM submissions s
    LEFT JOIN users u ON s.user_id = u.id
    WHERE s.test_id = ?
    ORDER BY s.completed_at DESC
  `).all(req.params.id);
  res.json({ test, submissions: subs });
});

// ─────────────────── FRIENDS ───────────────────
const insertNotif = (userId, type, title, message, linkUrl = null) =>
  db.prepare('INSERT INTO notifications (user_id, type, title, message, link_url) VALUES (?,?,?,?,?)')
    .run(userId, type, title, message, linkUrl);

const displayName = (u) => [u.first_name, u.last_name].filter(Boolean).join(' ') || u.username;

// Incoming pending requests
app.get('/api/friends/requests', auth, (req, res) => {
  const requests = db.prepare(`
    SELECT fr.id, fr.created_at, u.username, u.first_name, u.last_name, u.avatar_url
    FROM friend_requests fr
    JOIN users u ON fr.from_user_id = u.id
    WHERE fr.to_user_id = ? AND fr.status = 'pending'
    ORDER BY fr.created_at DESC
  `).all(req.user.id);
  res.json(requests);
});

// Friends list
app.get('/api/friends', auth, (req, res) => {
  const friends = db.prepare(`
    SELECT fr.id AS friendship_id,
           u.id, u.username, u.first_name, u.last_name, u.avatar_url
    FROM friend_requests fr
    JOIN users u ON (fr.from_user_id = ? AND u.id = fr.to_user_id)
                 OR (fr.to_user_id   = ? AND u.id = fr.from_user_id)
    WHERE fr.status = 'accepted'
    ORDER BY fr.created_at DESC
  `).all(req.user.id, req.user.id);
  res.json(friends);
});

// Friendship status with a specific user
app.get('/api/friends/status/:username', auth, (req, res) => {
  const target = db.prepare(
    'SELECT id, allow_friend_requests FROM users WHERE username = ?'
  ).get(req.params.username);
  if (!target) return res.status(404).json({ error: 'Пользователь не найден' });

  const row = db.prepare(`
    SELECT * FROM friend_requests
    WHERE (from_user_id = ? AND to_user_id = ?) OR (from_user_id = ? AND to_user_id = ?)
  `).get(req.user.id, target.id, target.id, req.user.id);

  if (!row) return res.json({ status: 'none', allow_friend_requests: target.allow_friend_requests });
  if (row.status === 'accepted') return res.json({ status: 'friends', row_id: row.id });
  if (row.from_user_id === req.user.id) return res.json({ status: 'sent', row_id: row.id });
  return res.json({ status: 'received', row_id: row.id });
});

// Send friend request
app.post('/api/friends/request', auth, (req, res) => {
  const { username } = req.body;
  if (!username) return res.status(400).json({ error: 'Имя пользователя обязательно' });
  const target = db.prepare('SELECT id, allow_friend_requests FROM users WHERE username = ?').get(username);
  if (!target) return res.status(404).json({ error: 'Пользователь не найден' });
  if (target.id === req.user.id) return res.status(400).json({ error: 'Нельзя добавить себя в друзья' });
  if (!target.allow_friend_requests) return res.status(403).json({ error: 'Пользователь не принимает заявки в друзья' });

  const existing = db.prepare(`
    SELECT * FROM friend_requests
    WHERE (from_user_id = ? AND to_user_id = ?) OR (from_user_id = ? AND to_user_id = ?)
  `).get(req.user.id, target.id, target.id, req.user.id);

  if (existing) {
    if (existing.status === 'accepted') return res.status(400).json({ error: 'Вы уже друзья' });
    if (existing.from_user_id === req.user.id) return res.status(400).json({ error: 'Запрос уже отправлен' });
    // They already sent us a request — auto-accept
    db.prepare('UPDATE friend_requests SET status = ? WHERE id = ?').run('accepted', existing.id);
    // Notify the original requester that we accepted
    const me = db.prepare('SELECT username, first_name, last_name FROM users WHERE id = ?').get(req.user.id);
    insertNotif(existing.from_user_id, 'friend_accepted', 'Заявка в друзья принята',
      `${displayName(me)} принял(а) вашу заявку в друзья`,
      `/profile.html?user=${encodeURIComponent(me.username)}`);
    return res.json({ ok: true, status: 'accepted', row_id: existing.id });
  }

  const r = db.prepare('INSERT INTO friend_requests (from_user_id, to_user_id) VALUES (?, ?)').run(req.user.id, target.id);
  // Notify the target about the new request
  const me = db.prepare('SELECT username, first_name, last_name FROM users WHERE id = ?').get(req.user.id);
  insertNotif(target.id, 'friend_request', 'Новая заявка в друзья',
    `${displayName(me)} хочет добавить вас в друзья`,
    `/profile.html?user=${encodeURIComponent(me.username)}`);
  res.json({ ok: true, status: 'sent', row_id: r.lastInsertRowid });
});

// Accept incoming request
app.post('/api/friends/requests/:id/accept', auth, (req, res) => {
  const row = db.prepare(
    "SELECT * FROM friend_requests WHERE id = ? AND to_user_id = ? AND status = 'pending'"
  ).get(req.params.id, req.user.id);
  if (!row) return res.status(404).json({ error: 'Запрос не найден' });
  db.prepare("UPDATE friend_requests SET status = 'accepted' WHERE id = ?").run(row.id);
  // Notify the original requester
  const me = db.prepare('SELECT username, first_name, last_name FROM users WHERE id = ?').get(req.user.id);
  insertNotif(row.from_user_id, 'friend_accepted', 'Заявка в друзья принята',
    `${displayName(me)} принял(а) вашу заявку в друзья`,
    `/profile.html?user=${encodeURIComponent(me.username)}`);
  res.json({ ok: true });
});

// Reject / cancel / unfriend
app.delete('/api/friends/:id', auth, (req, res) => {
  const row = db.prepare(
    'SELECT * FROM friend_requests WHERE id = ? AND (from_user_id = ? OR to_user_id = ?)'
  ).get(req.params.id, req.user.id, req.user.id);
  if (!row) return res.status(404).json({ error: 'Не найдено' });
  db.prepare('DELETE FROM friend_requests WHERE id = ?').run(row.id);
  res.json({ ok: true });
});

// ─────────────────── CLASSES ───────────────────
// Create class (teacher)
app.post('/api/classes', auth, teacherOnly, (req, res) => {
  const { name, password } = req.body;
  if (!name || !name.trim()) return res.status(400).json({ error: 'Название класса обязательно' });
  const code = getUniqueClassCode();
  const r = db.prepare(
    'INSERT INTO classes (name, code, password, teacher_id) VALUES (?, ?, ?, ?)'
  ).run(name.trim().slice(0, 80), code, password || null, req.user.id);
  res.json(db.prepare('SELECT * FROM classes WHERE id = ?').get(r.lastInsertRowid));
});

// Get teacher's classes
app.get('/api/classes', auth, teacherOnly, (req, res) => {
  const classes = db.prepare(`
    SELECT c.*,
      (SELECT COUNT(*) FROM class_members WHERE class_id = c.id) AS member_count,
      (SELECT COUNT(*) FROM class_tests   WHERE class_id = c.id) AS test_count
    FROM classes c WHERE c.teacher_id = ?
    ORDER BY c.created_at DESC
  `).all(req.user.id);
  res.json(classes);
});

// Get class detail (teacher or member student)
app.get('/api/classes/:id', auth, (req, res) => {
  const cls = db.prepare('SELECT * FROM classes WHERE id = ?').get(req.params.id);
  if (!cls) return res.status(404).json({ error: 'Класс не найден' });
  const isTeacher = req.user.role === 'teacher' && cls.teacher_id === req.user.id;
  const isMember  = !!db.prepare('SELECT id FROM class_members WHERE class_id = ? AND student_id = ?').get(cls.id, req.user.id);
  if (!isTeacher && !isMember) return res.status(403).json({ error: 'Нет доступа к классу' });
  const teacher = db.prepare('SELECT username, first_name, last_name FROM users WHERE id = ?').get(cls.teacher_id);
  res.json({ ...cls, teacher_name: teacher?.username, has_password: !!(cls.password) });
});

// Delete class (teacher)
app.delete('/api/classes/:id', auth, teacherOnly, (req, res) => {
  const cls = db.prepare('SELECT id FROM classes WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!cls) return res.status(404).json({ error: 'Класс не найден' });
  db.prepare('DELETE FROM classes WHERE id = ?').run(req.params.id);
  res.json({ ok: true });
});

// Join class (student)
app.post('/api/classes/join', auth, studentOnly, (req, res) => {
  const { code, password } = req.body;
  if (!code) return res.status(400).json({ error: 'Введите код класса' });
  const cls = db.prepare('SELECT * FROM classes WHERE code = ?').get(code.toUpperCase());
  if (!cls) return res.status(404).json({ error: 'Класс не найден. Проверьте код.' });
  if (cls.password && cls.password !== '') {
    if (!password || password !== cls.password)
      return res.status(403).json({ error: 'Неверный пароль класса' });
  }
  const already = db.prepare('SELECT id FROM class_members WHERE class_id = ? AND student_id = ?').get(cls.id, req.user.id);
  if (already) return res.status(400).json({ error: 'Вы уже состоите в этом классе' });
  db.prepare('INSERT INTO class_members (class_id, student_id) VALUES (?, ?)').run(cls.id, req.user.id);
  res.json({ id: cls.id, name: cls.name, code: cls.code });
});

// Leave class (student)
app.delete('/api/classes/:id/leave', auth, studentOnly, (req, res) => {
  db.prepare('DELETE FROM class_members WHERE class_id = ? AND student_id = ?').run(req.params.id, req.user.id);
  res.json({ ok: true });
});

// Get class members (teacher)
app.get('/api/classes/:id/members', auth, teacherOnly, (req, res) => {
  const cls = db.prepare('SELECT id FROM classes WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!cls) return res.status(404).json({ error: 'Класс не найден' });
  const members = db.prepare(`
    SELECT u.id, u.username, u.first_name, u.last_name, u.avatar_url, cm.joined_at
    FROM class_members cm JOIN users u ON cm.student_id = u.id
    WHERE cm.class_id = ? ORDER BY cm.joined_at ASC
  `).all(req.params.id);
  res.json(members);
});

// Remove member from class (teacher)
app.delete('/api/classes/:id/members/:userId', auth, teacherOnly, (req, res) => {
  const cls = db.prepare('SELECT id FROM classes WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!cls) return res.status(404).json({ error: 'Класс не найден' });
  db.prepare('DELETE FROM class_members WHERE class_id = ? AND student_id = ?').run(req.params.id, req.params.userId);
  res.json({ ok: true });
});

// Invite user to class by username (teacher) — creates pending invite
app.post('/api/classes/:id/invite', auth, teacherOnly, (req, res) => {
  const cls = db.prepare('SELECT * FROM classes WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!cls) return res.status(404).json({ error: 'Класс не найден' });

  const { username } = req.body;
  if (!username) return res.status(400).json({ error: 'Имя пользователя обязательно' });

  const target = db.prepare('SELECT id, allow_invites FROM users WHERE username = ?').get(username);
  if (!target) return res.status(404).json({ error: 'Пользователь не найден' });
  if (!target.allow_invites) return res.status(403).json({ error: 'Пользователь закрыл приглашения в класс' });

  const alreadyMember = db.prepare('SELECT id FROM class_members WHERE class_id = ? AND student_id = ?').get(cls.id, target.id);
  if (alreadyMember) return res.status(400).json({ error: 'Пользователь уже в этом классе' });

  try {
    db.prepare('INSERT INTO class_invites (class_id, user_id) VALUES (?, ?)').run(cls.id, target.id);
  } catch {
    return res.status(400).json({ error: 'Приглашение уже отправлено' });
  }

  const me = db.prepare('SELECT username, first_name, last_name FROM users WHERE id = ?').get(req.user.id);
  insertNotif(target.id, 'class_invite', 'Приглашение в класс',
    `${displayName(me)} приглашает вас в класс «${cls.name}»`,
    '/my-classes.html');
  res.json({ ok: true, class_name: cls.name });
});

// Get pending class invites for current user
app.get('/api/class-invites', auth, (req, res) => {
  const invites = db.prepare(`
    SELECT ci.id, ci.created_at, c.id AS class_id, c.name AS class_name, c.code,
           u.username AS teacher_username, u.first_name AS teacher_first, u.last_name AS teacher_last
    FROM class_invites ci
    JOIN classes c ON ci.class_id = c.id
    JOIN users u ON c.teacher_id = u.id
    WHERE ci.user_id = ?
    ORDER BY ci.created_at DESC
  `).all(req.user.id);
  res.json(invites);
});

// Accept class invite
app.post('/api/class-invites/:id/accept', auth, (req, res) => {
  const inv = db.prepare('SELECT * FROM class_invites WHERE id = ? AND user_id = ?').get(req.params.id, req.user.id);
  if (!inv) return res.status(404).json({ error: 'Приглашение не найдено' });

  const already = db.prepare('SELECT id FROM class_members WHERE class_id = ? AND student_id = ?').get(inv.class_id, req.user.id);
  if (!already) {
    db.prepare('INSERT INTO class_members (class_id, student_id) VALUES (?, ?)').run(inv.class_id, req.user.id);
  }
  db.prepare('DELETE FROM class_invites WHERE id = ?').run(inv.id);

  const cls = db.prepare('SELECT name, teacher_id FROM classes WHERE id = ?').get(inv.class_id);
  if (cls) {
    const me = db.prepare('SELECT username, first_name, last_name FROM users WHERE id = ?').get(req.user.id);
    insertNotif(cls.teacher_id, 'class_invite_accepted', 'Приглашение принято',
      `${displayName(me)} принял(а) приглашение в класс «${cls.name}»`,
      null);
  }
  res.json({ ok: true });
});

// Decline class invite
app.delete('/api/class-invites/:id', auth, (req, res) => {
  const inv = db.prepare('SELECT id FROM class_invites WHERE id = ? AND user_id = ?').get(req.params.id, req.user.id);
  if (!inv) return res.status(404).json({ error: 'Приглашение не найдено' });
  db.prepare('DELETE FROM class_invites WHERE id = ?').run(inv.id);
  res.json({ ok: true });
});

// Assign test to class (teacher)
app.post('/api/classes/:id/tests', auth, teacherOnly, (req, res) => {
  const cls = db.prepare('SELECT id FROM classes WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!cls) return res.status(404).json({ error: 'Класс не найден' });
  const { test_id } = req.body;
  const test = db.prepare('SELECT id FROM tests WHERE id = ? AND teacher_id = ?').get(test_id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  try {
    db.prepare('INSERT INTO class_tests (class_id, test_id) VALUES (?, ?)').run(req.params.id, test_id);
  } catch {
    return res.status(400).json({ error: 'Тест уже добавлен в этот класс' });
  }
  res.json({ ok: true });
});

// Remove test from class (teacher)
app.delete('/api/classes/:id/tests/:testId', auth, teacherOnly, (req, res) => {
  const cls = db.prepare('SELECT id FROM classes WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!cls) return res.status(404).json({ error: 'Класс не найден' });
  db.prepare('DELETE FROM class_tests WHERE class_id = ? AND test_id = ?').run(req.params.id, req.params.testId);
  res.json({ ok: true });
});

// Get tests in class
app.get('/api/classes/:id/tests', auth, (req, res) => {
  const cls = db.prepare('SELECT * FROM classes WHERE id = ?').get(req.params.id);
  if (!cls) return res.status(404).json({ error: 'Класс не найден' });
  const isTeacher = req.user.role === 'teacher' && cls.teacher_id === req.user.id;
  const isMember  = !!db.prepare('SELECT id FROM class_members WHERE class_id = ? AND student_id = ?').get(cls.id, req.user.id);
  if (!isTeacher && !isMember) return res.status(403).json({ error: 'Нет доступа' });
  const tests = db.prepare(`
    SELECT t.id, t.code, t.title, t.deadline_at, t.time_limit_minutes,
           t.is_anonymous, t.show_results,
           CASE WHEN t.password IS NOT NULL AND t.password != '' THEN 1 ELSE 0 END AS has_password,
           (SELECT COUNT(*) FROM questions WHERE test_id = t.id) AS question_count,
           ct.assigned_at
    FROM class_tests ct JOIN tests t ON ct.test_id = t.id
    WHERE ct.class_id = ?
    ORDER BY ct.assigned_at DESC
  `).all(req.params.id);
  res.json(tests);
});

// Get all joined classes with their tests (any role)
app.get('/api/my-classes', auth, (req, res) => {
  const classes = db.prepare(`
    SELECT c.id, c.name, c.code,
           u.username AS teacher_name
    FROM class_members cm
    JOIN classes c ON cm.class_id = c.id
    JOIN users u ON c.teacher_id = u.id
    WHERE cm.student_id = ?
    ORDER BY cm.joined_at DESC
  `).all(req.user.id);

  classes.forEach(cls => {
    cls.tests = db.prepare(`
      SELECT t.id, t.code, t.title, t.deadline_at, t.time_limit_minutes,
             t.is_anonymous, t.show_results,
             CASE WHEN t.password IS NOT NULL AND t.password != '' THEN 1 ELSE 0 END AS has_password,
             (SELECT COUNT(*) FROM questions WHERE test_id = t.id) AS question_count
      FROM class_tests ct JOIN tests t ON ct.test_id = t.id
      WHERE ct.class_id = ?
      ORDER BY ct.assigned_at DESC
    `).all(cls.id);
  });
  res.json(classes);
});

// ─────────────────── NOTIFICATIONS ───────────────────
app.get('/api/notifications', auth, (req, res) => {
  const notifs = db.prepare(`
    SELECT n.*, t.code AS test_code
    FROM notifications n
    LEFT JOIN tests t ON n.test_id = t.id
    WHERE n.user_id = ?
    ORDER BY n.created_at DESC
    LIMIT 50
  `).all(req.user.id);
  res.json(notifs);
});

app.get('/api/notifications/unread-count', auth, (req, res) => {
  const row = db.prepare(
    'SELECT COUNT(*) AS cnt FROM notifications WHERE user_id = ? AND is_read = 0'
  ).get(req.user.id);
  res.json({ count: row.cnt });
});

app.patch('/api/notifications/read-all', auth, (req, res) => {
  db.prepare('UPDATE notifications SET is_read = 1 WHERE user_id = ?').run(req.user.id);
  res.json({ ok: true });
});

app.patch('/api/notifications/:id/read', auth, (req, res) => {
  db.prepare('UPDATE notifications SET is_read = 1 WHERE id = ? AND user_id = ?')
    .run(req.params.id, req.user.id);
  res.json({ ok: true });
});

app.delete('/api/notifications/:id', auth, (req, res) => {
  db.prepare('DELETE FROM notifications WHERE id = ? AND user_id = ?')
    .run(req.params.id, req.user.id);
  res.json({ ok: true });
});

// ─────────────────── USER SEARCH ───────────────────
app.get('/api/users/search', auth, (req, res) => {
  const q = (req.query.q || '').trim();
  if (q.length < 2) return res.json([]);
  const like = `%${q}%`;
  const users = db.prepare(`
    SELECT id, username, first_name, last_name, institution, avatar_url, role
    FROM users
    WHERE username    LIKE ? COLLATE NOCASE
       OR first_name  LIKE ? COLLATE NOCASE
       OR last_name   LIKE ? COLLATE NOCASE
       OR institution LIKE ? COLLATE NOCASE
    LIMIT 20
  `).all(like, like, like, like);
  res.json(users);
});

// ─────────────────── QUESTION BANKS ───────────────────

// GET /api/banks — my banks + favorited banks
app.get('/api/banks', auth, teacherOnly, (req, res) => {
  const myBanks = db.prepare(`
    SELECT qb.id, qb.name, qb.description, qb.is_public, qb.created_at, qb.updated_at,
           COUNT(DISTINCT bq.id) AS question_count,
           0 AS is_favorite, NULL AS author_username, NULL AS author_first, NULL AS author_last
    FROM question_banks qb
    LEFT JOIN bank_questions bq ON bq.bank_id = qb.id
    WHERE qb.teacher_id = ?
    GROUP BY qb.id
    ORDER BY qb.updated_at DESC
  `).all(req.user.id);

  const favBanks = db.prepare(`
    SELECT qb.id, qb.name, qb.description, qb.is_public, qb.created_at, qb.updated_at,
           COUNT(DISTINCT bq.id) AS question_count,
           1 AS is_favorite, u.username AS author_username,
           u.first_name AS author_first, u.last_name AS author_last,
           u.avatar_url AS author_avatar
    FROM bank_favorites bf
    JOIN question_banks qb ON qb.id = bf.bank_id
    JOIN users u ON u.id = qb.teacher_id
    LEFT JOIN bank_questions bq ON bq.bank_id = qb.id
    WHERE bf.teacher_id = ?
    GROUP BY qb.id
    ORDER BY bf.created_at DESC
  `).all(req.user.id);

  res.json([...myBanks, ...favBanks]);
});

// GET /api/banks/public — public feed (MUST come before /api/banks/:id)
app.get('/api/banks/public', auth, (req, res) => {
  const q      = (req.query.q || '').trim();
  const limit  = Math.min(parseInt(req.query.limit)  || 20, 50);
  const offset = parseInt(req.query.offset) || 0;
  const like   = q ? `%${q}%` : '%';

  const banks = db.prepare(`
    SELECT qb.id, qb.name, qb.description, qb.created_at, qb.updated_at,
           u.id AS teacher_id, u.username, u.first_name, u.last_name,
           u.avatar_url, u.institution,
           COUNT(DISTINCT bq.id) AS question_count
    FROM question_banks qb
    JOIN users u ON u.id = qb.teacher_id
    LEFT JOIN bank_questions bq ON bq.bank_id = qb.id
    WHERE qb.is_public = 1
      AND (qb.name LIKE ? OR qb.description LIKE ? OR u.username LIKE ?)
    GROUP BY qb.id
    ORDER BY qb.updated_at DESC
    LIMIT ? OFFSET ?
  `).all(like, like, like, limit, offset);

  const { cnt: total } = db.prepare(`
    SELECT COUNT(*) AS cnt
    FROM question_banks qb
    JOIN users u ON u.id = qb.teacher_id
    WHERE qb.is_public = 1
      AND (qb.name LIKE ? OR qb.description LIKE ? OR u.username LIKE ?)
  `).get(like, like, like);

  const favSet = new Set(
    db.prepare('SELECT bank_id FROM bank_favorites WHERE teacher_id=?').all(req.user.id).map(r => r.bank_id)
  );
  banks.forEach(b => { b.is_favorited = favSet.has(b.id); });
  res.json({ banks, total, offset, limit });
});

// POST /api/banks — create bank
app.post('/api/banks', auth, teacherOnly, (req, res) => {
  const name        = req.body.name?.trim();
  const description = (req.body.description || '').trim();
  if (!name) return res.status(400).json({ error: 'Укажите название банка' });
  const r = db.prepare(
    'INSERT INTO question_banks (teacher_id, name, description) VALUES (?,?,?)'
  ).run(req.user.id, name, description);
  res.json({ id: r.lastInsertRowid, name, description, is_public: 0, question_count: 0 });
});

// GET /api/banks/:id — get bank with questions
app.get('/api/banks/:id', auth, (req, res) => {
  const bank = db.prepare(`
    SELECT qb.*, u.username, u.first_name, u.last_name, u.avatar_url,
           COUNT(DISTINCT bq.id) AS question_count
    FROM question_banks qb
    JOIN users u ON u.id = qb.teacher_id
    LEFT JOIN bank_questions bq ON bq.bank_id = qb.id
    WHERE qb.id = ?
    GROUP BY qb.id
  `).get(req.params.id);
  if (!bank) return res.status(404).json({ error: 'Банк не найден' });
  const isFav = !!db.prepare('SELECT 1 FROM bank_favorites WHERE teacher_id=? AND bank_id=?').get(req.user.id, bank.id);
  if (!bank.is_public && bank.teacher_id !== req.user.id && !isFav)
    return res.status(403).json({ error: 'Нет доступа' });

  const questions = db.prepare(`
    SELECT id, question_text, question_order
    FROM bank_questions WHERE bank_id = ? ORDER BY question_order, id
  `).all(req.params.id);
  for (const q of questions) {
    q.answers = db.prepare(
      'SELECT id, answer_text, is_correct FROM bank_answers WHERE bank_question_id = ? ORDER BY id'
    ).all(q.id);
  }
  res.json({ ...bank, questions });
});

// PUT /api/banks/:id — update meta
app.put('/api/banks/:id', auth, teacherOnly, (req, res) => {
  const bank = db.prepare('SELECT * FROM question_banks WHERE id=?').get(req.params.id);
  if (!bank) return res.status(404).json({ error: 'Банк не найден' });
  if (bank.teacher_id !== req.user.id) return res.status(403).json({ error: 'Нет доступа' });
  const name        = req.body.name?.trim()        ?? bank.name;
  const description = req.body.description?.trim() ?? bank.description;
  const is_public   = req.body.is_public != null   ? (req.body.is_public ? 1 : 0) : bank.is_public;
  db.prepare(`UPDATE question_banks SET name=?,description=?,is_public=?,updated_at=datetime('now') WHERE id=?`)
    .run(name, description, is_public, req.params.id);
  res.json({ ok: true });
});

// DELETE /api/banks/:id
app.delete('/api/banks/:id', auth, teacherOnly, (req, res) => {
  const bank = db.prepare('SELECT * FROM question_banks WHERE id=?').get(req.params.id);
  if (!bank) return res.status(404).json({ error: 'Банк не найден' });
  if (bank.teacher_id !== req.user.id) return res.status(403).json({ error: 'Нет доступа' });
  db.prepare('DELETE FROM question_banks WHERE id=?').run(req.params.id);
  res.json({ ok: true });
});

// POST /api/banks/:id/favorite — add to favorites
app.post('/api/banks/:id/favorite', auth, teacherOnly, (req, res) => {
  const bank = db.prepare('SELECT id, is_public, teacher_id FROM question_banks WHERE id=?').get(req.params.id);
  if (!bank) return res.status(404).json({ error: 'Банк не найден' });
  if (bank.teacher_id === req.user.id) return res.status(400).json({ error: 'Это ваш банк' });
  if (!bank.is_public) return res.status(403).json({ error: 'Банк не публичный' });
  db.prepare('INSERT OR IGNORE INTO bank_favorites (teacher_id, bank_id) VALUES (?,?)').run(req.user.id, req.params.id);
  res.json({ ok: true });
});

// DELETE /api/banks/:id/favorite — remove from favorites
app.delete('/api/banks/:id/favorite', auth, teacherOnly, (req, res) => {
  db.prepare('DELETE FROM bank_favorites WHERE teacher_id=? AND bank_id=?').run(req.user.id, req.params.id);
  res.json({ ok: true });
});

// POST /api/banks/:id/questions — add questions array {questions:[{question_text, answers:[{answer_text, is_correct}]}]}
app.post('/api/banks/:id/questions', auth, teacherOnly, (req, res) => {
  const bank = db.prepare('SELECT * FROM question_banks WHERE id=?').get(req.params.id);
  if (!bank) return res.status(404).json({ error: 'Банк не найден' });
  if (bank.teacher_id !== req.user.id) return res.status(403).json({ error: 'Нет доступа' });
  const { questions } = req.body;
  if (!Array.isArray(questions) || !questions.length)
    return res.status(400).json({ error: 'Нет вопросов' });

  const insertQ = db.prepare('INSERT INTO bank_questions (bank_id, question_text, question_order) VALUES (?,?,?)');
  const insertA = db.prepare('INSERT INTO bank_answers (bank_question_id, answer_text, is_correct) VALUES (?,?,?)');

  const addMany = db.transaction((qs) => {
    const { next } = db.prepare('SELECT COALESCE(MAX(question_order),0)+1 AS next FROM bank_questions WHERE bank_id=?').get(bank.id);
    let added = 0;
    for (let i = 0; i < qs.length; i++) {
      const q = qs[i];
      if (!q.question_text?.trim()) continue;
      const qr = insertQ.run(bank.id, q.question_text.trim(), next + i);
      for (const a of (q.answers || [])) {
        if (a.answer_text?.trim()) insertA.run(qr.lastInsertRowid, a.answer_text.trim(), a.is_correct ? 1 : 0);
      }
      added++;
    }
    db.prepare(`UPDATE question_banks SET updated_at=datetime('now') WHERE id=?`).run(bank.id);
    return added;
  });

  const added = addMany(questions);
  res.json({ ok: true, added });
});

// DELETE /api/banks/:id/questions/:qid
app.delete('/api/banks/:id/questions/:qid', auth, teacherOnly, (req, res) => {
  const bank = db.prepare('SELECT * FROM question_banks WHERE id=?').get(req.params.id);
  if (!bank) return res.status(404).json({ error: 'Банк не найден' });
  if (bank.teacher_id !== req.user.id) return res.status(403).json({ error: 'Нет доступа' });
  db.prepare('DELETE FROM bank_questions WHERE id=? AND bank_id=?').run(req.params.qid, req.params.id);
  db.prepare(`UPDATE question_banks SET updated_at=datetime('now') WHERE id=?`).run(bank.id);
  res.json({ ok: true });
});

// GET /api/tests/:id/questions-preview — for export modal (questions without answers shown)
app.get('/api/tests/:id/questions-preview', auth, teacherOnly, (req, res) => {
  const test = db.prepare('SELECT id, title FROM tests WHERE id=? AND teacher_id=?').get(req.params.id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  const questions = db.prepare(
    'SELECT id, question_text FROM questions WHERE test_id=? ORDER BY question_order, id'
  ).all(req.params.id);
  res.json({ test, questions });
});

// POST /api/tests/:id/export-to-bank — copy selected test questions into a bank
app.post('/api/tests/:id/export-to-bank', auth, teacherOnly, (req, res) => {
  const test = db.prepare('SELECT * FROM tests WHERE id=? AND teacher_id=?').get(req.params.id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });
  const { bank_id, question_ids } = req.body;
  const bank = db.prepare('SELECT * FROM question_banks WHERE id=? AND teacher_id=?').get(bank_id, req.user.id);
  if (!bank) return res.status(404).json({ error: 'Банк не найден' });

  const qs = (question_ids?.length)
    ? db.prepare(`SELECT * FROM questions WHERE test_id=? AND id IN (${question_ids.map(()=>'?').join(',')})`)
        .all(req.params.id, ...question_ids)
    : db.prepare('SELECT * FROM questions WHERE test_id=?').all(req.params.id);

  const insertQ = db.prepare('INSERT INTO bank_questions (bank_id, question_text, question_order) VALUES (?,?,?)');
  const insertA = db.prepare('INSERT INTO bank_answers (bank_question_id, answer_text, is_correct) VALUES (?,?,?)');

  const doExport = db.transaction(() => {
    const { next } = db.prepare('SELECT COALESCE(MAX(question_order),0)+1 AS next FROM bank_questions WHERE bank_id=?').get(bank_id);
    for (let i = 0; i < qs.length; i++) {
      const q = qs[i];
      const qr = insertQ.run(bank_id, q.question_text, next + i);
      const answers = db.prepare('SELECT * FROM answers WHERE question_id=?').all(q.id);
      for (const a of answers) insertA.run(qr.lastInsertRowid, a.answer_text, a.is_correct);
    }
    db.prepare(`UPDATE question_banks SET updated_at=datetime('now') WHERE id=?`).run(bank_id);
    return qs.length;
  });

  const added = doExport();
  res.json({ ok: true, added });
});


// ─────────────────── FILE UPLOADS ───────────────────

// Question image upload (teacher only)
app.post('/api/upload/question-image', (req, res, next) => {
  const token = req.headers.authorization?.split(' ')[1];
  if (!token) return res.status(401).json({ error: 'Необходима авторизация' });
  try { req.user = jwt.verify(token, JWT_SECRET); } catch { return res.status(401).json({ error: 'Токен недействителен' }); }
  if (req.user.role !== 'teacher') return res.status(403).json({ error: 'Только для преподавателей' });
  next();
}, qImageUpload.single('image'), (req, res) => {
  if (!req.file) return res.status(400).json({ error: 'Файл не загружен' });
  res.json({ url: `/uploads/question-images/${req.file.filename}` });
});

// Student answer file upload (no auth required — students may not be logged in)
app.post('/api/upload/answer-file', answerFileUpload.single('file'), (req, res) => {
  if (!req.file) return res.status(400).json({ error: 'Файл не загружен' });
  const isImage = req.file.mimetype.startsWith('image/');
  res.json({
    url:      `/uploads/answer-files/${req.file.filename}`,
    filename: req.file.originalname,
    is_image: isImage
  });
});

// ─────────────────── SPA fallback ───────────────────
app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(PORT, () => {
  console.log(`\nPolyMaker server running: http://localhost:${PORT}\n`);
});
