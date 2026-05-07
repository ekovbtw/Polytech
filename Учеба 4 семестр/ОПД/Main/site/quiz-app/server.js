const express = require('express');
const Database = require('better-sqlite3');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;
const JWT_SECRET = process.env.JWT_SECRET || 'quizmaster_secret_key_change_in_production';

app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

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
`);

// ─────────────────── Утилиты ───────────────────
function generateCode() {
  const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  let code = '';
  for (let i = 0; i < 6; i++) {
    code += chars[Math.floor(Math.random() * chars.length)];
  }
  return code;
}

function getUniqueCode() {
  let code;
  do { code = generateCode(); }
  while (db.prepare('SELECT id FROM tests WHERE code = ?').get(code));
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

function validateTestBody(body) {
  const { title, questions } = body;
  if (!title || !title.trim()) return 'Название теста обязательно';
  if (!questions || questions.length < 1) return 'Добавьте хотя бы один вопрос';
  for (const q of questions) {
    if (!q.question_text?.trim()) return 'Текст вопроса не может быть пустым';
    if (!q.answers || q.answers.length < 2) return 'Каждый вопрос должен иметь минимум 2 варианта ответа';
    if (!q.answers.some(a => a.is_correct)) return 'Отметьте правильный ответ для каждого вопроса';
  }
  return null;
}

app.post('/api/tests', auth, teacherOnly, (req, res) => {
  const err = validateTestBody(req.body);
  if (err) return res.status(400).json({ error: err });

  const { title, description, questions, password, is_anonymous, show_results, shuffle_questions } = req.body;
  const code = getUniqueCode();

  const save = db.transaction(() => {
    const t = db.prepare(`
      INSERT INTO tests (code, title, description, teacher_id, password, is_anonymous, show_results, shuffle_questions)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    `).run(code, title.trim(), (description||'').trim(), req.user.id,
           password || null, is_anonymous ? 1 : 0,
           show_results !== false ? 1 : 0, shuffle_questions ? 1 : 0);

    const tid = t.lastInsertRowid;
    questions.forEach((q, i) => {
      const qr = db.prepare(
        'INSERT INTO questions (test_id, question_text, question_order) VALUES (?, ?, ?)'
      ).run(tid, q.question_text.trim(), i);
      q.answers.forEach(a =>
        db.prepare('INSERT INTO answers (question_id, answer_text, is_correct) VALUES (?, ?, ?)')
          .run(qr.lastInsertRowid, a.answer_text.trim(), a.is_correct ? 1 : 0));
    });
    db.prepare('UPDATE users SET is_new = 0 WHERE id = ?').run(req.user.id);
    return tid;
  });

  const tid = save();
  res.json(db.prepare('SELECT * FROM tests WHERE id = ?').get(tid));
});

app.put('/api/tests/:id', auth, teacherOnly, (req, res) => {
  const test = db.prepare('SELECT * FROM tests WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });

  const err = validateTestBody(req.body);
  if (err) return res.status(400).json({ error: err });

  const { title, description, questions, password, is_anonymous, show_results, shuffle_questions } = req.body;

  db.transaction(() => {
    db.prepare(`UPDATE tests SET title=?, description=?, password=?, is_anonymous=?,
      show_results=?, shuffle_questions=?, updated_at=CURRENT_TIMESTAMP WHERE id=?`
    ).run(title.trim(), (description||'').trim(), password||null,
          is_anonymous?1:0, show_results!==false?1:0, shuffle_questions?1:0, req.params.id);

    db.prepare('DELETE FROM questions WHERE test_id = ?').run(req.params.id);

    questions.forEach((q, i) => {
      const qr = db.prepare(
        'INSERT INTO questions (test_id, question_text, question_order) VALUES (?, ?, ?)'
      ).run(req.params.id, q.question_text.trim(), i);
      q.answers.forEach(a =>
        db.prepare('INSERT INTO answers (question_id, answer_text, is_correct) VALUES (?, ?, ?)')
          .run(qr.lastInsertRowid, a.answer_text.trim(), a.is_correct ? 1 : 0));
    });
  })();

  res.json(db.prepare('SELECT * FROM tests WHERE id = ?').get(req.params.id));
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
      CASE WHEN password IS NOT NULL AND password != '' THEN 1 ELSE 0 END AS has_password,
      (SELECT COUNT(*) FROM questions WHERE test_id = tests.id) AS question_count
    FROM tests WHERE code = ?
  `).get(req.params.code.toUpperCase());
  if (!test) return res.status(404).json({ error: 'Тест не найден. Проверьте код.' });
  res.json(test);
});

app.post('/api/public/tests/:id/questions', (req, res) => {
  const { password } = req.body;
  const test = db.prepare('SELECT * FROM tests WHERE id = ?').get(req.params.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });

  if (test.password && test.password !== '') {
    if (!password || password !== test.password)
      return res.status(403).json({ error: 'Неверный пароль к тесту' });
  }

  let questions = db.prepare(
    'SELECT id, question_text, question_order FROM questions WHERE test_id = ? ORDER BY question_order'
  ).all(req.params.id);

  if (test.shuffle_questions) {
    for (let i = questions.length - 1; i > 0; i--) {
      const j = Math.floor(Math.random() * (i + 1));
      [questions[i], questions[j]] = [questions[j], questions[i]];
    }
  }

  questions = questions.map(q => ({
    ...q,
    answers: db.prepare('SELECT id, answer_text FROM answers WHERE question_id = ?').all(q.id)
  }));

  res.json({ test: { id: test.id, title: test.title, is_anonymous: test.is_anonymous, show_results: test.show_results }, questions });
});

// ─────────────────── SUBMIT ───────────────────
app.post('/api/submissions', (req, res) => {
  const { test_id, answers, password, student_name, user_id } = req.body;
  if (!test_id) return res.status(400).json({ error: 'Не указан тест' });

  const test = db.prepare('SELECT * FROM tests WHERE id = ?').get(test_id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });

  if (test.password && test.password !== '') {
    if (!password || password !== test.password)
      return res.status(403).json({ error: 'Неверный пароль' });
  }

  const questions = db.prepare('SELECT * FROM questions WHERE test_id = ?').all(test_id);
  let score = 0;

  const subId = db.transaction(() => {
    const sr = db.prepare(`
      INSERT INTO submissions (test_id, user_id, student_name, total_questions)
      VALUES (?, ?, ?, ?)
    `).run(test_id, user_id || null, student_name || 'Аноним', questions.length);

    const sid = sr.lastInsertRowid;
    for (const q of questions) {
      const aid = answers?.[q.id] || null;
      db.prepare(
        'INSERT INTO submission_answers (submission_id, question_id, answer_id) VALUES (?, ?, ?)'
      ).run(sid, q.id, aid);
      if (aid) {
        const a = db.prepare('SELECT is_correct FROM answers WHERE id = ? AND question_id = ?').get(aid, q.id);
        if (a?.is_correct) score++;
      }
    }
    db.prepare('UPDATE submissions SET score = ? WHERE id = ?').run(score, sid);
    return sid;
  })();

  res.json({ submission_id: subId, score, total: questions.length, show_results: test.show_results === 1 });
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
    SELECT sa.question_id, sa.answer_id,
      q.question_text,
      a.answer_text AS selected_answer,
      a.is_correct  AS selected_is_correct,
      (SELECT answer_text FROM answers WHERE question_id = q.id AND is_correct = 1 LIMIT 1) AS correct_answer
    FROM submission_answers sa
    JOIN questions q ON sa.question_id = q.id
    LEFT JOIN answers a ON sa.answer_id = a.id
    WHERE sa.submission_id = ?
  `).all(req.params.id);

  res.json({ ...sub, answers: detail });
});

// ─────────────────── RESULTS (teacher) ───────────────────
app.get('/api/tests/:id/results', auth, teacherOnly, (req, res) => {
  const test = db.prepare('SELECT * FROM tests WHERE id = ? AND teacher_id = ?').get(req.params.id, req.user.id);
  if (!test) return res.status(404).json({ error: 'Тест не найден' });

  const subs = db.prepare(`
    SELECT s.*, u.username
    FROM submissions s
    LEFT JOIN users u ON s.user_id = u.id
    WHERE s.test_id = ?
    ORDER BY s.completed_at DESC
  `).all(req.params.id);

  res.json({ test, submissions: subs });
});

// SPA fallback
app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(PORT, () => {
  console.log(`\n🎓 QuizMaster запущен: http://localhost:${PORT}\n`);
});
