/* ── Auth ─────────────────────────────────────────── */
const Auth = {
  getToken() { return localStorage.getItem('qm_token'); },
  getUser()  { return JSON.parse(localStorage.getItem('qm_user') || 'null'); },
  save(token, user) {
    localStorage.setItem('qm_token', token);
    localStorage.setItem('qm_user', JSON.stringify(user));
  },
  clear() {
    localStorage.removeItem('qm_token');
    localStorage.removeItem('qm_user');
  },
  isLoggedIn() { return !!this.getToken(); },
  requireAuth(role) {
    if (!this.isLoggedIn()) { window.location.href = '/auth.html'; return false; }
    if (role && this.getUser()?.role !== role) {
      window.location.href = '/dashboard.html'; return false;
    }
    return true;
  },
  redirectIfLoggedIn() {
    if (this.isLoggedIn()) { window.location.href = '/dashboard.html'; }
  }
};

/* ── API ──────────────────────────────────────────── */
const API = {
  async request(method, url, body) {
    const opts = {
      method,
      headers: { 'Content-Type': 'application/json' }
    };
    const token = Auth.getToken();
    if (token) opts.headers['Authorization'] = `Bearer ${token}`;
    if (body)  opts.body = JSON.stringify(body);

    const res  = await fetch(url, opts);
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || 'Ошибка сервера');
    return data;
  },
  get(url)          { return this.request('GET', url); },
  post(url, body)   { return this.request('POST', url, body); },
  put(url, body)    { return this.request('PUT', url, body); },
  delete(url)       { return this.request('DELETE', url); }
};

/* ── Toast ────────────────────────────────────────── */
let toastTimer;
function showToast(msg, type = '') {
  let toast = document.getElementById('global-toast');
  if (!toast) {
    toast = document.createElement('div');
    toast.id = 'global-toast';
    toast.className = 'toast';
    document.body.appendChild(toast);
  }
  toast.className = `toast ${type}`;
  toast.innerHTML = `${type === 'success' ? '✅' : type === 'error' ? '❌' : 'ℹ️'} ${msg}`;
  toast.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove('show'), 3000);
}

/* ── Alert helpers ────────────────────────────────── */
function showAlert(el, msg, type = 'error') {
  if (!el) return;
  el.className = `alert alert-${type} visible`;
  el.innerHTML = `${type === 'error' ? '⚠️' : '✅'} ${msg}`;
}
function hideAlert(el) {
  if (el) el.className = 'alert';
}

/* ── Password visibility toggle (event delegation) ── */
// Один обработчик на весь документ — работает для любых кнопок,
// в том числе добавленных динамически. Не нужно вызывать повторно.
document.addEventListener('click', function (e) {
  const btn = e.target.closest('.password-toggle');
  if (!btn) return;
  const input = btn.closest('.password-wrapper')?.querySelector('input');
  if (!input) return;
  if (input.type === 'password') {
    input.type = 'text';
    btn.textContent = '🙈';
  } else {
    input.type = 'password';
    btn.textContent = '👁️';
  }
});

// Оставляем функцию как no-op для обратной совместимости с вызовами на страницах
function initPasswordToggles() { /* handled by delegation above */ }

/* ── Navbar ───────────────────────────────────────── */
function renderNavbar() {
  const nav = document.getElementById('navbar');
  if (!nav) return;

  const user = Auth.getUser();
  nav.innerHTML = `
    <div class="navbar-inner container">
      <a href="${user ? '/dashboard.html' : '/'}" class="navbar-logo">
        <img src="/images/logo.png" class="navbar-logo-img" alt="PolyTest">
        <span class="navbar-logo-text">Poly<span>Test</span></span>
      </a>
      <nav class="navbar-nav">
        ${user ? `
          <div class="navbar-user">
            <span class="role-badge ${user.role === 'student' ? 'student' : ''}">
              ${user.role === 'teacher' ? '👨‍🏫 Преподаватель' : '🎓 Студент'}
            </span>
            <span>${user.username}</span>
          </div>
          <button class="btn btn-secondary btn-sm" onclick="logout()">Выйти</button>
        ` : `
          <a href="/auth.html" class="btn btn-secondary btn-sm">Войти</a>
          <a href="/auth.html" class="btn btn-primary btn-sm">Регистрация</a>
        `}
      </nav>
    </div>`;
}

function logout() {
  Auth.clear();
  window.location.href = '/';
}

/* ── Копирование ──────────────────────────────────── */
async function copyToClipboard(text, msg = 'Скопировано!') {
  try {
    await navigator.clipboard.writeText(text);
    showToast(msg, 'success');
  } catch {
    showToast('Не удалось скопировать', 'error');
  }
}

/* ── Date format ──────────────────────────────────── */
function fmtDate(str) {
  if (!str) return '—';
  const d = new Date(str);
  return d.toLocaleDateString('ru-RU', { day: '2-digit', month: '2-digit', year: 'numeric' }) +
         ' ' + d.toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
}

/* ── Score color ──────────────────────────────────── */
function scorePillClass(score, total) {
  const pct = total ? (score / total) * 100 : 0;
  return pct >= 70 ? 'high' : pct >= 40 ? 'mid' : 'low';
}
