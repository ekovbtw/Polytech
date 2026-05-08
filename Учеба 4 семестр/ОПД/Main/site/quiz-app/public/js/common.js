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
    if (this.isLoggedIn()) { window.location.href = '/mode-select.html'; }
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
  get(url)           { return this.request('GET', url); },
  post(url, body)    { return this.request('POST', url, body); },
  put(url, body)     { return this.request('PUT', url, body); },
  patch(url, body)   { return this.request('PATCH', url, body); },
  delete(url)        { return this.request('DELETE', url); }
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
      <a href="${user ? '/mode-select.html' : '/'}" class="navbar-logo">
        <img src="/images/logo.png" class="navbar-logo-img" alt="PolyMaker">
        <span class="navbar-logo-text">Poly<span>Maker</span></span>
      </a>
      <nav class="navbar-nav">
        ${user ? `
          <a href="/search.html" class="btn btn-secondary btn-sm" title="Поиск пользователей">🔍</a>
          ${user.role === 'teacher'
            ? '<a href="/manage-classes.html" class="btn btn-secondary btn-sm" id="nav-classes-btn">🎓 Преподавание</a><a href="/my-classes.html" class="btn btn-secondary btn-sm">📚 Обучение</a>'
            : '<a href="/my-classes.html"     class="btn btn-secondary btn-sm" id="nav-classes-btn">📚 Обучение</a>'
          }
          <button class="theme-toggle-btn" id="theme-toggle-btn" onclick="_toggleTheme()" title="Переключить тему">🌙</button>
          <div class="notif-wrap" id="notif-wrap">
            <button class="notif-btn" id="notif-btn" onclick="_toggleNotifPanel(event)" title="Уведомления">
              🔔<span class="notif-badge d-none" id="notif-badge">0</span>
            </button>
            <div class="notif-panel d-none" id="notif-panel">
              <div class="notif-panel-header">
                <span class="fw-bold">Уведомления</span>
                <button class="notif-read-all" onclick="_markAllRead()">Прочитать все</button>
              </div>
              <div id="notif-list"><div class="notif-empty">Нет уведомлений</div></div>
            </div>
          </div>
          <a href="/profile.html" class="navbar-avatar-btn" title="${user.username}" id="nav-profile-btn">
            <div class="navbar-avatar-placeholder" id="nav-avatar-circle">
              ${user.username.slice(0,2).toUpperCase()}
            </div>
          </a>
          <button class="btn btn-secondary btn-sm" onclick="logout()">Выйти</button>
        ` : `
          <button class="theme-toggle-btn" id="theme-toggle-btn" onclick="_toggleTheme()" title="Переключить тему">🌙</button>
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

/* ── Dark mode ──────────────────────────────────────── */
function _applyTheme(theme) {
  if (theme === 'dark') {
    document.documentElement.setAttribute('data-theme', 'dark');
  } else {
    document.documentElement.removeAttribute('data-theme');
  }
  const icon = theme === 'dark' ? '☀️' : '🌙';
  ['theme-toggle-btn', 'theme-btn'].forEach(id => {
    const btn = document.getElementById(id);
    if (btn) btn.textContent = icon;
  });
}

function _toggleTheme() {
  const next = (localStorage.getItem('qm_theme') || 'light') === 'dark' ? 'light' : 'dark';
  localStorage.setItem('qm_theme', next);
  _applyTheme(next);
}

/* ── Navbar avatar / classes async load ──────────────── */
function _loadNavbarExtras() {
  const classesBtn = document.getElementById('nav-classes-btn');
  const profileBtn = document.getElementById('nav-profile-btn');
  if (!classesBtn && !profileBtn) return;
  if (classesBtn) classesBtn.style.display = '';
  if (profileBtn) profileBtn.style.display = '';
  // Load avatar from profile API
  API.get('/api/profile').then(p => {
    const circle = document.getElementById('nav-avatar-circle');
    if (!circle) return;
    if (p.avatar_url) {
      circle.innerHTML = `<img src="${p.avatar_url}" style="width:32px;height:32px;border-radius:50%;object-fit:cover;">`;
    }
  }).catch(() => {});
}

// Auto-run after DOM ready
document.addEventListener('DOMContentLoaded', () => {
  // Sync theme toggle button icon with saved preference
  _applyTheme(localStorage.getItem('qm_theme') || 'light');

  if (Auth.isLoggedIn()) {
    _loadNavbarExtras();
    _loadUnreadCount();
    // Poll for new notifications every 60 seconds
    setInterval(_loadUnreadCount, 60 * 1000);
  }
});

/* ── Notification panel ───────────────────────────── */
function _toggleNotifPanel(e) {
  e.stopPropagation();
  const panel = document.getElementById('notif-panel');
  if (!panel) return;
  const isOpen = !panel.classList.contains('d-none');
  if (isOpen) {
    panel.classList.add('d-none');
  } else {
    panel.classList.remove('d-none');
    _loadNotifications();
  }
}

// Close panel when clicking outside
document.addEventListener('click', function(e) {
  const wrap = document.getElementById('notif-wrap');
  if (wrap && !wrap.contains(e.target)) {
    const panel = document.getElementById('notif-panel');
    if (panel) panel.classList.add('d-none');
  }
});

async function _loadNotifications() {
  const list = document.getElementById('notif-list');
  if (!list) return;
  list.innerHTML = '<div class="notif-empty">Загрузка…</div>';
  try {
    const items = await API.get('/api/notifications');
    if (!items.length) {
      list.innerHTML = '<div class="notif-empty">Нет уведомлений</div>';
      return;
    }
    list.innerHTML = `<div class="notif-list-inner">${items.map(n => {
      const icon = n.type === 'friend_request'      ? '👤'
                 : n.type === 'friend_accepted'      ? '🤝'
                 : n.type === 'class_invite'          ? '🏫'
                 : n.type === 'class_invite_accepted' ? '✅'
                 : n.type?.startsWith('deadline')     ? '⏰'
                 : '🔔';
      const unreadClass = n.is_read ? '' : ' unread';
      const dotHtml    = n.is_read ? '' : '<div class="notif-item-dot"></div>';
      const linkAttr   = n.link_url ? ` data-link="${escHtmlN(n.link_url)}"` : '';
      return `
        <div class="notif-item${unreadClass}" data-id="${n.id}"${linkAttr} onclick="_readNotif(${n.id}, this)">
          <div class="notif-item-icon">${icon}</div>
          <div class="notif-item-body">
            <div class="notif-item-text">${escHtmlN(n.message)}</div>
            <div class="notif-item-time">${fmtDate(n.created_at)}</div>
          </div>
          ${dotHtml}
        </div>`;
    }).join('')}</div>`;
  } catch {
    list.innerHTML = '<div class="notif-empty">Не удалось загрузить</div>';
  }
}

async function _loadUnreadCount() {
  const badge = document.getElementById('notif-badge');
  if (!badge) return;
  try {
    const data = await API.get('/api/notifications/unread-count');
    const cnt = data.count || 0;
    if (cnt > 0) {
      badge.textContent = cnt > 99 ? '99+' : cnt;
      badge.classList.remove('d-none');
    } else {
      badge.classList.add('d-none');
    }
  } catch { /* ignore */ }
}

async function _readNotif(id, el) {
  try {
    if (el.classList.contains('unread')) {
      await API.patch(`/api/notifications/${id}/read`);
      el.classList.remove('unread');
      el.querySelector('.notif-item-dot')?.remove();
      _loadUnreadCount();
    }
    const link = el.dataset.link;
    if (link) window.location.href = link;
  } catch { /* ignore */ }
}

async function _markAllRead() {
  try {
    await API.patch('/api/notifications/read-all');
    _loadNotifications();
    _loadUnreadCount();
  } catch {
    showToast('Не удалось отметить уведомления', 'error');
  }
}

function escHtmlN(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
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
