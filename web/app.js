'use strict';

const API = '';
const TOKEN_KEY = 'epqms.session';
const USER_KEY = 'epqms.user';
const REMEMBERED_USERNAME_KEY = 'epqms.rememberedUsername';
const state = { dashboard: {}, patients: [], queue: [], doctors: [], treatments: [], user: null, loading: false, loadError: '', treatmentTab: 'active' };
const content = document.getElementById('content');
const toastEl = document.getElementById('toast');
const modalRoot = document.getElementById('modalRoot');
const authView = document.getElementById('authView');
const appShell = document.getElementById('appShell');
let toastTimer;
let activeModal = null;
let modalReturnFocus = null;
let actionInProgressCount = 0;

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
}
function showToast(message, type = 'info') {
  toastEl.innerHTML = `<span aria-hidden="true">${type === 'success' ? '✓' : type === 'error' ? '✕' : type === 'warning' ? '⚠' : 'ℹ'}</span><span>${escapeHtml(message)}</span><button class="toast-close" aria-label="Dismiss notification">×</button>`;
  toastEl.className = `toast show ${type}`;
  toastEl.querySelector('.toast-close').onclick = () => { toastEl.className = 'toast'; };
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { toastEl.className = 'toast'; }, 3600);
}
function toast(message, type = 'info') { showToast(message, type); }
function setButtonLoading(button, loading, label) {
  if (!button) return;
  if (loading) {
    button.dataset.originalText = button.innerHTML;
    button.disabled = true;
    button.setAttribute('aria-busy', 'true');
    button.innerHTML = `<span class="spinner" aria-hidden="true"></span>${escapeHtml(label || 'Working…')}`;
  } else {
    button.disabled = false;
    button.removeAttribute('aria-busy');
    button.innerHTML = button.dataset.originalText || button.innerHTML;
    delete button.dataset.originalText;
  }
}
function token() { return sessionStorage.getItem(TOKEN_KEY) || ''; }
function rememberedUsername() {
  try { return localStorage.getItem(REMEMBERED_USERNAME_KEY) || ''; }
  catch (_) { return ''; }
}
function storeRememberedUsername(username) {
  try {
    if (username) localStorage.setItem(REMEMBERED_USERNAME_KEY, username);
    else localStorage.removeItem(REMEMBERED_USERNAME_KEY);
  } catch (_) { /* Remember Me is optional when browser storage is unavailable. */ }
}
async function api(path, options = {}) {
  let response;
  try {
    response = await fetch(`${API}${path}`, {
      ...options,
      headers: { ...(options.body ? { 'Content-Type': 'application/json' } : {}), ...(token() ? { Authorization: `Bearer ${token()}` } : {}), ...options.headers }
    });
  } catch (_) { throw new Error('Server unavailable. Check that the local C++ web server is running.'); }
  const text = await response.text();
  let body = null;
  try { body = text ? JSON.parse(text) : null; } catch (_) { throw new Error('The server returned an invalid response.'); }
  if (!response.ok) {
    if (response.status === 401 && !path.startsWith('/api/auth/')) expireSession();
    throw new Error(body?.error || `Request failed (${response.status})`);
  }
  return body;
}
const post = (path, body = {}) => api(path, { method: 'POST', body: JSON.stringify(body) });
function validIdentity(value) {
  const v = value.trim();
  return /^[A-Za-z0-9_.-]{3,40}$/.test(v) || /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(v);
}
function authBrandingMarkup() {
  return `<aside class="auth-brand-panel" aria-label="Hospital queue system information">
    <div class="auth-brand-content">
      <div class="auth-brand"><span class="auth-brand-mark" aria-hidden="true"><svg viewBox="0 0 24 24"><path d="M10 3h4v7h7v4h-7v7h-4v-7H3v-4h7z"/></svg></span><div><strong>Emergency Patient Queue</strong><span>Management System</span></div></div>
      <div class="auth-brand-copy"><h2>Hospital Queue<br>Management</h2><p>Manage patients, prioritize emergency cases, track treatments and optimize hospital operations.</p></div>
      <ul class="auth-features" aria-label="System features">
        <li><span aria-hidden="true"><svg viewBox="0 0 24 24"><circle cx="9" cy="8" r="3"/><path d="M3.5 19v-1.5A4.5 4.5 0 0 1 8 13h2a4.5 4.5 0 0 1 4.5 4.5V19zM16 5.5a3 3 0 0 1 0 5.8M17 13h.5a4 4 0 0 1 4 4v2H17"/></svg></span>Patient Management</li>
        <li><span aria-hidden="true"><svg viewBox="0 0 24 24"><path d="m13 2-9 12h7l-1 8 10-13h-7z"/></svg></span>Emergency Queue</li>
        <li><span aria-hidden="true"><svg viewBox="0 0 24 24"><path d="M8 5a3 3 0 1 0 0 6h2v2a3 3 0 1 0 6 0v-2h1a3 3 0 1 0 0-6h-1V4a3 3 0 1 0-6 0v1z"/><path d="M12 8v5m-2.5-2.5h5"/></svg></span>Doctor Management</li>
        <li><span aria-hidden="true"><svg viewBox="0 0 24 24"><path d="M4 19V5m0 14h17M7 15l4-4 3 2 6-7"/><path d="M16 6h4v4"/></svg></span>Real-time Statistics</li>
      </ul>
    </div>
    <svg class="auth-hospital-art" viewBox="0 0 520 230" role="img" aria-labelledby="hospitalArtTitle hospitalArtDesc">
      <title id="hospitalArtTitle">Hospital and ambulance</title><desc id="hospitalArtDesc">A small hospital with a medical cross, trees and an ambulance.</desc>
      <ellipse cx="266" cy="210" rx="244" ry="13" fill="#041f37" opacity=".35"/>
      <path d="M0 179c68-27 117-18 163 0s99 19 152-7 122-31 205-1v59H0z" fill="#12507a" opacity=".7"/>
      <g stroke="#b7d9f0" stroke-width="2">
        <path d="M135 208V116l65-39 65 39v92z" fill="#f7fbff"/><path d="M184 208V91h47v117" fill="#fff"/>
        <path d="M198 55h40v37h-40z" fill="#fff"/><path d="M214 61h9v11h11v9h-11v11h-9V81h-11v-9h11z" fill="#f23b4b" stroke="none"/>
        <path d="M145 128h18v19h-18zm35 0h18v19h-18zm35 0h12v19h-12zm-70 35h18v19h-18zm35 0h18v19h-18zm35 0h12v19h-12z" fill="#bce0fa"/>
        <path d="M203 174h26v34h-26z" fill="#7ab5df"/><path d="M215 174v34" fill="none"/>
        <path d="M264 132h44v76h-44z" fill="#e9f5ff"/><path d="M273 142h12v15h-12zm20 0h8v15h-8zm-20 26h12v15h-12zm20 0h8v15h-8z" fill="#bce0fa"/>
      </g>
      <path d="M123 208h195" stroke="#d8edf9" stroke-width="4" stroke-linecap="round"/>
      <g transform="translate(325 153)">
        <path d="M0 23h110v36H0z" fill="#fff" stroke="#b7d9f0" stroke-width="2"/><path d="M74 4h22l14 19H74z" fill="#e9f5ff" stroke="#b7d9f0" stroke-width="2"/><path d="M79 8h15l9 12H79z" fill="#86c1e9"/><path d="M0 23h110v9H0z" fill="#f23b4b"/><path d="M39 36h16v4H39z" fill="#f23b4b"/><circle cx="25" cy="60" r="9" fill="#173e5d"/><circle cx="87" cy="60" r="9" fill="#173e5d"/><circle cx="25" cy="60" r="4" fill="#e7f3fb"/><circle cx="87" cy="60" r="4" fill="#e7f3fb"/><path d="M45 34h6v5h5v5h-5v5h-6v-5h-5v-5h5z" fill="#fff"/>
      </g>
      <g fill="#74bd91"><circle cx="91" cy="190" r="13"/><circle cx="417" cy="195" r="12"/><circle cx="445" cy="189" r="16"/></g><path d="M91 198v12m326-5v5m28-8v8" stroke="#d7ecfa" stroke-width="3"/>
    </svg>
  </aside>`;
}
function authPage(mode = 'login') {
  appShell.hidden = true;
  authView.hidden = false;
  const isRegister = mode === 'register';
  authView.classList.toggle('auth-register', isRegister);
  document.body.classList.add('auth-active');
  document.body.classList.toggle('auth-register-active', isRegister);
  const inputLabel = isRegister ? 'Username or email' : 'Email';
  const submitLabel = isRegister ? 'Create Account' : 'Sign In';
  authView.innerHTML = `<div class="auth-layout">${authBrandingMarkup()}<section class="auth-form-panel" aria-label="${isRegister ? 'Account registration' : 'Staff sign in'}"><div class="auth-card" aria-labelledby="authTitle">
    <div class="auth-form-heading"><span class="auth-lock-mark" aria-hidden="true"><svg viewBox="0 0 24 24"><rect x="5" y="10" width="14" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3m-4 4v3"/></svg></span><h1 id="authTitle">${isRegister ? 'Create your account' : 'Welcome Back'}</h1><p>${isRegister ? 'Register to manage hospital operations' : 'Sign in to your hospital dashboard'}</p></div>
    <form id="authForm" novalidate>${isRegister ? `<div class="field"><label for="authFullName">Full name</label><input id="authFullName" name="fullName" autocomplete="name" maxlength="100" required><small class="field-error" data-error="fullName"></small></div>` : ''}
      <div class="field"><label for="authUsername"><svg class="auth-label-icon" viewBox="0 0 24 24" aria-hidden="true"><rect x="3" y="5" width="18" height="14" rx="2"/><path d="m4 7 8 6 8-6"/></svg>${inputLabel}</label><input id="authUsername" name="username" type="text" autocomplete="username" maxlength="254" placeholder="${isRegister ? 'Choose a username or enter email' : 'Enter your email or username'}" required><small class="field-error" data-error="username"></small></div>
      <div class="field"><label for="authPassword"><svg class="auth-label-icon" viewBox="0 0 24 24" aria-hidden="true"><rect x="5" y="10" width="14" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/></svg>Password</label>${isRegister ? `<input id="authPassword" name="password" type="password" autocomplete="new-password" minlength="8" maxlength="128" required>` : `<div class="auth-password-wrap"><input id="authPassword" name="password" type="password" autocomplete="current-password" minlength="8" maxlength="128" required><button type="button" class="auth-password-toggle" id="togglePassword" aria-label="Show password" aria-pressed="false"><svg viewBox="0 0 24 24" aria-hidden="true" focusable="false"><path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7S2 12 2 12Z"/><circle cx="12" cy="12" r="3"/></svg></button></div>`}<small class="field-error" data-error="password"></small></div>
      ${isRegister ? `<div class="field"><label for="authConfirm">Confirm password</label><input id="authConfirm" name="confirmPassword" type="password" autocomplete="new-password" required><small class="field-error" data-error="confirmPassword"></small></div>` : `<div class="auth-options"><label class="auth-remember" for="rememberMe"><input type="checkbox" id="rememberMe"><span>Remember username</span></label><button type="button" class="auth-link" id="forgotPassword">Forgot password?</button></div>`}
      <div class="auth-error" id="authError" role="alert" aria-live="polite"></div><button class="btn auth-submit" type="submit" id="authSubmit">${isRegister ? 'Create Account' : '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M14 4h5a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2h-5m-4-4 4-4-4-4m4 4H3"/></svg>Sign In'}</button>
    </form><div class="auth-bottom"><p>${isRegister ? 'Already have an account?' : 'Don’t have an account?'} <button type="button" id="authSwitch">${isRegister ? 'Sign in' : 'Register'}</button></p><small class="auth-footnote">Local account · Passwords are stored as salted hashes</small></div>
  </div></section></div>`;
  const usernameInput = document.getElementById('authUsername');
  const passwordInput = document.getElementById('authPassword');
  if (!isRegister) {
    const savedUsername = rememberedUsername();
    usernameInput.value = savedUsername;
    document.getElementById('rememberMe').checked = Boolean(savedUsername);
    document.getElementById('togglePassword').addEventListener('click', event => {
      const button = event.currentTarget;
      const visible = passwordInput.type === 'password';
      passwordInput.type = visible ? 'text' : 'password';
      button.setAttribute('aria-label', visible ? 'Hide password' : 'Show password');
      button.setAttribute('aria-pressed', String(visible));
      button.innerHTML = visible
        ? '<svg viewBox="0 0 24 24" aria-hidden="true" focusable="false"><path d="m3 3 18 18M10.6 10.7a2 2 0 0 0 2.7 2.7"/><path d="M9.9 5.2A10.8 10.8 0 0 1 12 5c6.4 0 10 7 10 7a16 16 0 0 1-3.1 3.8M6.2 6.2C3.5 8 2 12 2 12s3.6 7 10 7a10 10 0 0 0 4-.8"/></svg>'
        : '<svg viewBox="0 0 24 24" aria-hidden="true" focusable="false"><path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7S2 12 2 12Z"/><circle cx="12" cy="12" r="3"/></svg>';
    });
    const rememberMe = document.getElementById('rememberMe');
    rememberMe.addEventListener('change', () => storeRememberedUsername(rememberMe.checked ? usernameInput.value.trim() : ''));
    usernameInput.addEventListener('input', () => {
      if (rememberMe.checked) storeRememberedUsername(usernameInput.value.trim());
    });
    document.getElementById('forgotPassword').addEventListener('click', () => {
      showModal('Password Recovery', '<p class="modal-intro">Password recovery is currently managed by your system administrator. Please contact your administrator to reset your account password.</p><div class="modal-actions"><button type="button" class="btn" id="closeRecoveryDialog">Close</button></div>', null, 'auth-recovery-modal');
      modalRoot.querySelector('#closeRecoveryDialog').onclick = closeModal;
    });
  }
  document.getElementById('authSwitch').onclick = () => authPage(isRegister ? 'login' : 'register');
  const form = document.getElementById('authForm');
  let submitting = false;
  form.onsubmit = async event => {
    event.preventDefault();
    if (submitting) return;
    const fullName = document.getElementById('authFullName')?.value.trim() || '';
    const username = document.getElementById('authUsername').value.trim();
    const password = document.getElementById('authPassword').value;
    const confirm = document.getElementById('authConfirm')?.value;
    const errors = {};
    form.querySelectorAll('.field-error').forEach(target => {
      target.textContent = '';
      target.closest('.field')?.querySelector('input')?.removeAttribute('aria-invalid');
    });
    if (isRegister && !fullName) errors.fullName = 'Enter your full name.';
    if (!username) errors.username = 'Enter your username or email.';
    else if (!validIdentity(username)) errors.username = 'Enter a valid username or email address.';
    if (!password) errors.password = 'Enter your password.';
    else if (password.length < 8 || password.length > 128) errors.password = 'Password must be 8–128 characters.';
    if (isRegister && password !== confirm) errors.confirmPassword = 'Passwords do not match.';
    Object.keys(errors).forEach(key => { const target = form.querySelector(`[data-error="${key}"]`); target.textContent = errors[key]; target.closest('.field')?.querySelector('input')?.setAttribute('aria-invalid', 'true'); });
    if (Object.keys(errors).length) return;
    submitting = true;
    const button = document.getElementById('authSubmit');
    setButtonLoading(button, true, isRegister ? 'Creating account…' : 'Signing in…');
    document.getElementById('authError').textContent = '';
    try {
      if (isRegister) {
        await post('/api/auth/register', { fullName, username, password });
        showToast('Account created successfully. Please sign in.', 'success');
        authPage('login');
        document.getElementById('authUsername').value = username;
      } else {
        const result = await post('/api/auth/login', { username, password });
        storeRememberedUsername(document.getElementById('rememberMe').checked ? username : '');
        sessionStorage.setItem(TOKEN_KEY, result.token);
        sessionStorage.setItem(USER_KEY, JSON.stringify(result.user));
        state.user = result.user;
        showToast('Login successful.', 'success');
        location.hash = 'dashboard';
        await enterApp();
      }
    } catch (error) {
      if (!isRegister && error.message.includes('Invalid username/email')) document.getElementById('authError').textContent = 'Invalid username/email or password.';
      else if (isRegister && error.message.toLowerCase().includes('already registered')) document.querySelector('[data-error="username"]').textContent = 'That username or email is already registered.';
      else document.getElementById('authError').textContent = isRegister ? error.message : 'Unable to sign in right now. Please try again.';
    } finally {
      submitting = false;
      if (document.getElementById('authSubmit') === button) setButtonLoading(button, false);
    }
  };
  document.getElementById(isRegister ? 'authFullName' : 'authUsername').focus();
}
function expireSession() {
  sessionStorage.removeItem(TOKEN_KEY); sessionStorage.removeItem(USER_KEY); state.user = null;
  appShell.hidden = true; location.hash = ''; authPage('login');
}
function redirectToAuthentication() {
  const requested = location.hash;
  if (requested && requested !== '#register') history.replaceState(null, '', `${location.pathname}${location.search}`);
  authPage(requested === '#register' ? 'register' : 'login');
}
async function logout() {
  try { await post('/api/auth/logout'); } catch (_) { /* The local session is always cleared. */ }
  expireSession(); showToast('You have been signed out.', 'info');
}
function normalizePatient(p) {
  return { ...p, id: String(p.id), type: p.type === 'emergency' ? 'Emergency' : 'Regular', wait: p.waitingTime ?? 0,
    status: String(p.status || '').replaceAll('_', ' ').toLowerCase().replace(/\b\w/g, c => c.toUpperCase()) };
}
function normalizeDoctor(d) {
  const status = d.availability === 'available' ? 'Available' : d.availability === 'on_leave' ? 'On Leave' : 'Busy';
  return { ...d, id: String(d.id), spec: d.specialization, status,
    patient: d.currentPatientId == null ? '-' : String(d.currentPatientId) };
}
async function refreshData() {
  state.loading = true;
  state.loadError = '';
  try {
    const [dashboard, patients, queue, doctors, treatments] = await Promise.all([
      api('/api/dashboard'), api('/api/patients'), api('/api/queue'), api('/api/doctors'), api('/api/treatments')
    ]);
    state.dashboard = dashboard; state.patients = patients.map(normalizePatient); state.queue = queue.map(normalizePatient);
    state.doctors = doctors.map(normalizeDoctor); state.treatments = treatments;
  } catch (error) {
    state.loadError = error.message || 'Hospital data could not be loaded.';
    throw error;
  } finally { state.loading = false; }
}
function updateUserUI() {
  const name = state.user?.fullName || 'Staff Member';
  document.getElementById('userName').textContent = name;
  document.getElementById('userInitials').textContent = name.trim().split(/\s+/).slice(0, 2).map(x => x[0]).join('').toUpperCase();
}
async function enterApp() {
  authView.hidden = true; appShell.hidden = false;
  document.body.classList.remove('auth-active', 'auth-register-active');
  try {
    state.user = await api('/api/auth/me');
    sessionStorage.setItem(USER_KEY, JSON.stringify(state.user));
    updateUserUI();
    if (!location.hash || location.hash === '#login' || location.hash === '#register') location.hash = 'dashboard';
    state.loading = true; render();
    await refreshData(); render();
  } catch (error) { if (token()) showToast(error.message, 'error'); expireSession(); }
}
function badge(status) {
  const c = status === 'Waiting' ? 'waiting' : status === 'Completed' ? 'success' : status === 'Under Treatment' ? 'under' :
    status === 'Available' ? 'success' : status === 'Busy' ? 'busy' : status === 'On Leave' ? 'leave' : status === 'Emergency' ? 'critical' : 'regular';
  return `<span class="badge ${c}">${escapeHtml(status)}</span>`;
}
function severityBadge(n) { return `<span class="severity severity-${n}"><span aria-hidden="true">${n >= 5 ? '!' : '●'}</span> ${n >= 5 ? 'Critical' : n >= 3 ? 'Moderate' : 'Mild'} · ${n}/5</span>`; }
function dashboardIcon(name) {
  const paths = {
    patients: '<circle cx="9" cy="8" r="3"/><path d="M3 20v-1a6 6 0 0 1 12 0v1M17 8a3 3 0 1 1 0 6m1 2a4 4 0 0 1 3 4"/>',
    clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
    assigned: '<path d="M9 12h10m-4-4 4 4-4 4"/><circle cx="5" cy="12" r="3"/>',
    care: '<path d="M3 11h18v9H3zM6 11V7a3 3 0 0 1 3-3h6a3 3 0 0 1 3 3v4M8 15h3m-1.5-1.5v3"/>',
    completed: '<circle cx="12" cy="12" r="9"/><path d="m8 12 2.5 2.5L16 9"/>',
    queue: '<path d="M5 4h14v16H5zM8 8h8M8 12h8M8 16h5"/>',
    doctor: '<path d="M7 4v5a5 5 0 0 0 10 0V4M5 4h4m6 0h4m-7 10v2a4 4 0 0 0 8 0v-1"/><circle cx="20" cy="13" r="2"/>',
    add: '<path d="M12 5v14M5 12h14"/>',
    chart: '<path d="M4 20V10m5 10V4m6 16v-7m5 7V7M2 20h20"/>',
  };
  return `<svg class="dashboard-icon" viewBox="0 0 24 24" aria-hidden="true">${paths[name] || paths.patients}</svg>`;
}
function pageTitle(title, sub, action = '') { return `<div class="page-title"><div><h1>${title}</h1><p>${sub}</p></div>${action}</div>`; }
function statCard(label, value, icon, cls) { return `<article class="card stat"><div><div class="label">${label}</div><div class="value">${escapeHtml(value)}</div></div><div class="stat-icon ${cls}" aria-hidden="true">${icon}</div></article>`; }
function emptyState(title, description, action = '') { return `<div class="empty-state"><span class="empty-icon" aria-hidden="true">⌕</span><h3>${title}</h3><p>${description}</p>${action}</div>`; }
function queueTable(rows, actions = true) {
  if (!rows.length) return emptyState('No patients are currently waiting.', 'The emergency queue is clear.');
  return `<div class="table-wrap"><table class="table dashboard-queue-table"><thead><tr><th>Rank</th><th>Patient ID</th><th>Patient name</th><th>Severity</th><th>Priority score</th><th>Waiting time</th><th>Current status</th>${actions ? '<th>Action</th>' : ''}</tr></thead><tbody>${rows.map((p, i) => `<tr class="${p.critical ? 'critical-row' : ''}"><td><span class="queue-rank ${p.critical ? 'rank-critical' : ''}">${i + 1}</span></td><td><b>${escapeHtml(p.id)}</b></td><td>${escapeHtml(p.name)}</td><td>${severityBadge(Number(p.severity))}</td><td><b class="priority-number">${escapeHtml(p.priority)}</b></td><td>${escapeHtml(p.wait)} min</td><td>${badge(p.status)}</td>${actions ? `<td><button class="mini-btn detail" data-id="${escapeHtml(p.id)}">View</button></td>` : ''}</tr>`).join('')}</tbody></table></div>`;
}
function dashboard() {
  const d = state.dashboard;
  const totalDoctors = state.doctors.length;
  const available = state.doctors.filter(doctor => doctor.status === 'Available').length;
  const busy = totalDoctors - available;
  const doctorShare = totalDoctors ? available / totalDoctors * 100 : 0;
  const busyShare = totalDoctors ? busy / totalDoctors * 100 : 0;
  const totalPatients = Number(d.totalPatients || 0);
  const statusRows = [
    ['Waiting', Number(d.waitingPatients || 0), '#1677e8'],
    ['Called', Number(d.calledPatients || 0), '#8c98a8'],
    ['Assigned', Number(d.assignedPatients || 0), '#f4bd45'],
    ['Under Treatment', Number(d.underTreatment || 0), '#a16beb'],
    ['Completed', Number(d.completedPatients || 0), '#22a66a']
  ];
  let angle = 0;
  const statusSegments = statusRows.map(row => {
    const start = angle;
    angle += totalPatients ? row[1] / totalPatients * 100 : 0;
    return `${row[2]} ${start}% ${angle}%`;
  }).join(', ');
  const userName = escapeHtml(state.user?.fullName || 'Hospital Staff');
  return `<section class="dashboard-heading"><div><p class="eyebrow">HOSPITAL OPERATIONS OVERVIEW</p><h1>Welcome back, ${userName}</h1><p>Monitor patient flow, care teams, and current hospital activity.</p></div></section>
    ${state.loadError ? `<div class="dashboard-error" role="alert">${escapeHtml(state.loadError)} <button class="text-btn" id="retryDashboard">Retry</button></div>` : ''}
    <div class="dashboard-stats">
      ${statCard('Total Patients', d.totalPatients ?? 0, dashboardIcon('patients'), 'i-blue')}
      ${statCard('Waiting', d.waitingPatients ?? 0, dashboardIcon('clock'), 'i-orange')}
      ${statCard('Assigned', d.assignedPatients ?? 0, dashboardIcon('assigned'), 'i-purple')}
      ${statCard('Under Treatment', d.underTreatment ?? 0, dashboardIcon('care'), 'i-blue')}
      ${statCard('Completed', d.completedPatients ?? 0, dashboardIcon('completed'), 'i-green')}
    </div>
    <div class="dashboard-primary-grid">
      <section class="panel dashboard-panel"><div class="panel-head"><div class="dashboard-panel-title">${dashboardIcon('queue')}<div><h2>Emergency Queue <span class="muted">· Top 5</span></h2><p>Highest-priority patients waiting for treatment</p></div></div><button class="text-btn" data-go="queue">View Queue <span aria-hidden="true">→</span></button></div>${queueTable(state.queue.slice(0, 5), false)}</section>
      <section class="panel dashboard-panel"><div class="panel-head"><div class="dashboard-panel-title">${dashboardIcon('chart')}<div><h2>Patient Status Overview</h2><p>Current patient distribution</p></div></div></div>${totalPatients ? `<div class="status-overview"><div class="status-donut" style="background:conic-gradient(${statusSegments})"><div><strong>${totalPatients}</strong><span>Total patients</span></div></div><div class="status-legend">${statusRows.map(row => `<div><i style="--legend-color:${row[2]}"></i><span>${row[0]}</span><strong>${row[1]}</strong><small>${Math.round(row[1] / totalPatients * 100)}%</small></div>`).join('')}</div></div>` : emptyState('No patient data yet.', 'Status distribution appears after patients are registered.')}</section>
    </div>
    <div class="dashboard-secondary-grid">
      <section class="panel dashboard-panel"><div class="panel-head"><div class="dashboard-panel-title">${dashboardIcon('doctor')}<div><h2>Doctor Status</h2><p>Current team availability</p></div></div><button class="text-btn" data-go="doctors">View Doctors</button></div>${totalDoctors ? `<div class="doctor-summary dashboard-doctor-summary"><div class="donut" style="--available:${doctorShare}%"><span><b>${totalDoctors}</b><small>Total doctors</small></span></div><div class="legend"><div><i class="dot green"></i><span>Available</span><b>${available}</b><small>${Math.round(doctorShare)}%</small></div><div><i class="dot blue"></i><span>Busy</span><b>${busy}</b><small>${Math.round(busyShare)}%</small></div></div></div>` : emptyState('No doctors registered.', 'Add a doctor to begin assigning patients.')}</section>
      <section class="panel dashboard-panel dashboard-quick-panel"><div class="panel-head"><div class="dashboard-panel-title">${dashboardIcon('assigned')}<div><h2>Quick Actions</h2><p>Common hospital operations</p></div></div></div><div class="dashboard-actions"><button class="dashboard-action action-blue" id="quickAddPatient">${dashboardIcon('patients')}<span>Register Patient</span></button><button class="dashboard-action action-purple" id="quickAddDoctor">${dashboardIcon('doctor')}<span>Add Doctor</span></button><button class="dashboard-action action-green" data-go="queue">${dashboardIcon('assigned')}<span>Manage Queue</span></button><button class="dashboard-action action-orange" data-go="statistics">${dashboardIcon('chart')}<span>View Statistics</span></button></div></section>
    </div>`;
}
function patientsPage() {
  return pageTitle('Patients', 'Search and manage registered patients', '<button class="btn" id="addPatient">＋ Add patient</button>') +
    '<section class="panel"><div class="toolbar"><label class="table-search"><span aria-hidden="true">⌕</span><input id="patientSearch" type="search" placeholder="Search name, ID or condition" aria-label="Search patients"></label><select id="typeFilter" aria-label="Filter by patient type"><option>All types</option><option>Emergency</option><option>Regular</option></select><select id="statusFilter" aria-label="Filter by status"><option>All statuses</option><option>Waiting</option><option>Called</option><option>Assigned</option><option>Under Treatment</option><option>Completed</option></select></div><div id="patientTable" class="table-wrap"></div></section>';
}
function renderPatientTable() {
  const target = document.getElementById('patientTable'); if (!target) return;
  const q = (document.getElementById('patientSearch')?.value || '').toLowerCase();
  const type = document.getElementById('typeFilter')?.value || 'All types'; const status = document.getElementById('statusFilter')?.value || 'All statuses';
  const rows = state.patients.filter(p => (!q || [p.id, p.name, p.condition].some(x => String(x).toLowerCase().includes(q))) && (type === 'All types' || p.type === type) && (status === 'All statuses' || p.status === status));
  target.innerHTML = rows.length ? `<table class="table"><thead><tr><th>Patient ID</th><th>Name</th><th>Age</th><th>Type</th><th>Condition</th><th>Severity</th><th>Waiting time</th><th>Status</th><th>Action</th></tr></thead><tbody>${rows.map(p => `<tr><td><b>${escapeHtml(p.id)}</b></td><td>${escapeHtml(p.name)}</td><td>${escapeHtml(p.age)}</td><td>${badge(p.type)}</td><td>${escapeHtml(p.condition)}</td><td>${severityBadge(Number(p.severity))}</td><td>${escapeHtml(p.wait)} min</td><td>${badge(p.status)}</td><td><button class="mini-btn detail" data-id="${escapeHtml(p.id)}">Details</button></td></tr>`).join('')}</tbody></table>` : emptyState(state.patients.length ? 'No matching patients.' : 'No patients registered yet.', state.patients.length ? 'Try changing your search or filters.' : 'Add a patient to begin managing the queue.', state.patients.length ? '' : '<button class="btn" id="emptyAddPatient">＋ Add patient</button>');
  target.querySelectorAll('.detail').forEach(button => button.onclick = () => openPatientDetails(button.dataset.id));
  target.querySelector('#emptyAddPatient')?.addEventListener('click', openPatientModal);
}
function queuePage() { return pageTitle('Emergency Queue', 'Priority order calculated by the C++ backend', '<button class="btn" id="callNext">⚡ Call next patient</button>') + `<section class="panel">${queueTable(state.queue)}<div class="callout"><b>Priority handling</b><br>Priority scores and queue order are supplied by the hospital backend.</div></section>`; }
function doctorsPage() {
  const rows = state.doctors.map(d => `<tr><td><b>${escapeHtml(d.id)}</b></td><td>${escapeHtml(d.name)}</td><td>${escapeHtml(d.spec)}</td><td>${badge(d.status)}</td><td>${d.patient === '-' ? '<span class="muted">—</span>' : `<button class="text-btn detail" data-id="${escapeHtml(d.patient)}">Patient ${escapeHtml(d.patient)}</button>`}</td></tr>`).join('');
  return pageTitle('Doctors', 'Availability and patient assignments', '<button class="btn" id="addDoctor">＋ Add doctor</button>') + `<section class="panel">${rows ? `<div class="table-wrap"><table class="table"><thead><tr><th>Doctor ID</th><th>Name</th><th>Specialization</th><th>Availability</th><th>Current patient</th></tr></thead><tbody>${rows}</tbody></table></div>` : emptyState('No doctors registered.', 'Add a doctor to begin assigning patients.', '<button class="btn" id="emptyAddDoctor">＋ Add doctor</button>')}</section>`;
}
function treatmentsPage() {
  const active = state.patients.filter(p => p.status === 'Under Treatment');
  const completed = state.treatments.filter(t => t.completed || t.status === 'completed');
  const selected = state.treatmentTab === 'active' ? active : completed;
  const rows = state.treatmentTab === 'active' ? active.map(p => { const doctor = state.doctors.find(d => d.patient === p.id); return `<tr><td><b>${escapeHtml(p.id)}</b></td><td>${escapeHtml(p.name)}</td><td>${escapeHtml(doctor?.name || '—')}</td><td>${badge('Under Treatment')}</td><td><button class="mini-btn completeTreatment" data-id="${escapeHtml(p.id)}">Complete</button></td></tr>`; }).join('') : completed.map(t => { const p = state.patients.find(x => x.id === String(t.patientId)); const d = state.doctors.find(x => x.id === String(t.doctorId)); return `<tr><td><b>${escapeHtml(t.patientId)}</b></td><td>${escapeHtml(p?.name || '—')}</td><td>${escapeHtml(d?.name || t.doctorId)}</td><td>${escapeHtml(t.diagnosis || '—')}</td><td>${badge('Completed')}</td><td>${escapeHtml(t.description || '—')}</td></tr>`; }).join('');
  const head = state.treatmentTab === 'active' ? '<tr><th>Patient ID</th><th>Patient name</th><th>Doctor</th><th>Status</th><th>Action</th></tr>' : '<tr><th>Patient ID</th><th>Patient name</th><th>Doctor</th><th>Diagnosis</th><th>Status</th><th>Treatment details</th></tr>';
  return pageTitle('Treatments', 'Track active care and completed records') + `<section class="panel"><div class="tab-row" role="tablist"><button class="tab ${state.treatmentTab === 'active' ? 'active' : ''}" role="tab" aria-selected="${state.treatmentTab === 'active'}" data-treatment-tab="active">Active <span>${active.length}</span></button><button class="tab ${state.treatmentTab === 'completed' ? 'active' : ''}" role="tab" aria-selected="${state.treatmentTab === 'completed'}" data-treatment-tab="completed">Completed <span>${completed.length}</span></button></div>${selected.length ? `<div class="table-wrap"><table class="table"><thead>${head}</thead><tbody>${rows}</tbody></table></div>` : emptyState(state.treatmentTab === 'active' ? 'No active treatments.' : 'No completed treatments yet.', state.treatmentTab === 'active' ? 'Patients appear here after a doctor starts treatment.' : 'Completed treatment records will appear here.')}</section>`;
}
function statisticsPage() {
  const d = state.dashboard; const n = key => Number(d[key] || 0); const hasPatientData = n('totalPatients') > 0; const types = { emergency: state.patients.filter(p => p.type === 'Emergency').length, regular: state.patients.filter(p => p.type === 'Regular').length };
  const severity = [1, 2, 3, 4, 5].map(level => state.patients.filter(p => Number(p.severity) === level).length);
  const doctorCounts = { available: state.doctors.filter(x => x.status === 'Available').length, busy: state.doctors.filter(x => x.status === 'Busy').length };
  if (!hasPatientData && !state.doctors.length && !state.treatments.length) return pageTitle('Statistics', 'Current hospital activity') + emptyState('No data available.', 'Statistics will appear as patients, doctors, and treatments are added.');
  const maxType = Math.max(types.emergency, types.regular, 1); const maxSeverity = Math.max(...severity, 1); const maxStatus = Math.max(n('waitingPatients'), n('calledPatients'), n('assignedPatients'), n('underTreatment'), n('completedPatients'), 1);
  const bars = (values, max, labels) => `<div class="bar-chart">${values.map((value, i) => `<div class="bar" style="--bar-height:${Math.max(value ? 8 : 0, value / max * 100)}%"><span>${value}</span><label>${labels[i]}</label></div>`).join('')}</div>`;
  return pageTitle('Statistics', 'Metrics calculated from current backend records') + `<div class="cards stats-cards">${statCard('Total patients', n('totalPatients'), '♟', 'i-blue')}${statCard('Total doctors', state.doctors.length, '⚕', 'i-green')}${statCard('Available doctors', doctorCounts.available, '✓', 'i-purple')}${statCard('Busy doctors', doctorCounts.busy, '◷', 'i-orange')}</div><div class="charts"><section class="panel"><div class="panel-head"><h3>Patients by status</h3></div>${hasPatientData ? bars([n('waitingPatients'), n('calledPatients'), n('assignedPatients'), n('underTreatment'), n('completedPatients')], maxStatus, ['Waiting', 'Called', 'Assigned', 'In care', 'Completed']) : emptyState('No data available.', 'No patients are registered.')}</section><section class="panel"><div class="panel-head"><h3>Patient type</h3></div>${hasPatientData ? bars([types.emergency, types.regular], maxType, ['Emergency', 'Regular']) : emptyState('No data available.', 'No patient records to compare.')}</section><section class="panel"><div class="panel-head"><h3>Patients by severity</h3></div>${hasPatientData ? bars(severity, maxSeverity, ['1', '2', '3', '4', '5']) : emptyState('No data available.', 'No severity records to display.')}</section></div><div class="cards stats-cards">${statCard('Average waiting', `${d.averageWaitingTime ?? 0} min`, '◷', 'i-blue')}${statCard('Total treatments', n('totalTreatments'), '▣', 'i-purple')}${statCard('Critical cases', n('criticalPatients'), '⚠', 'i-orange')}${statCard('Doctor availability', `${doctorCounts.available} / ${state.doctors.length}`, '⚕', 'i-green')}</div>`;
}
function dataPage() { return pageTitle('Data management', 'Save and restore hospital records') + `<div class="info-grid"><section class="panel"><div class="panel-head"><h3>Persistence</h3></div><p class="muted">Save patient, doctor, and treatment records through the C++ backend.</p><div class="action-row"><button class="btn" id="saveData">Save data</button><button class="btn secondary" id="loadData">Load data</button></div></section><section class="panel"><h3>Local records</h3><div class="detail-list"><div class="detail-item"><small>Patients</small><b>data/patients.txt</b></div><div class="detail-item"><small>Doctors</small><b>data/doctors.txt</b></div><div class="detail-item"><small>Treatments</small><b>data/treatments.txt</b></div></div></section></div>`; }
function aboutPage() { return `<section class="hero"><div><p class="eyebrow">C++ OOP COURSE PROJECT</p><h2>Emergency Patient Queue Management System</h2><p>A local hospital management application powered by a C++ object-oriented backend and HTTP/JSON web interface.</p></div><div class="hero-art" aria-hidden="true">🏥</div></section><div class="feature-grid">${[['⚡','Priority-based emergency queue'],['♟','Patient management'],['⚕','Doctor management'],['▣','Treatment records'],['▥','Backend statistics'],['▤','File persistence'],['⌘','Authenticated web interface']].map(x => `<article class="card feature"><div class="ficon">${x[0]}</div><h3>${x[1]}</h3></article>`).join('')}</div><section class="panel about-tech"><h3>Technology</h3><p>C++ · HTML · CSS · JavaScript · C++ HTTP backend</p></section>`; }
function searchPage() {
  const query = state.searchQuery || '';
  const q = query.toLowerCase();
  const patientsFound = state.patients.filter(p => [p.id, p.name, p.condition].some(x => String(x).toLowerCase().includes(q)));
  const doctorsFound = state.doctors.filter(d => [d.id, d.name, d.spec].some(x => String(x).toLowerCase().includes(q)));
  return pageTitle('Search results', `Results matching “${escapeHtml(query)}”`) +
    `<div class="info-grid"><section class="panel"><div class="panel-head"><h3>Patients <span class="muted">· ${patientsFound.length}</span></h3></div>${patientsFound.length ? `<div class="table-wrap"><table class="table"><thead><tr><th>Patient</th><th>Condition</th><th>Status</th><th></th></tr></thead><tbody>${patientsFound.map(p => `<tr><td><b>${escapeHtml(p.id)}</b> · ${escapeHtml(p.name)}</td><td>${escapeHtml(p.condition)}</td><td>${badge(p.status)}</td><td><button class="mini-btn" data-search-patient="${escapeHtml(p.id)}">Details</button></td></tr>`).join('')}</tbody></table></div>` : emptyState('No matching patients.', 'No patient record matches this search.')}</section><section class="panel"><div class="panel-head"><h3>Doctors <span class="muted">· ${doctorsFound.length}</span></h3></div>${doctorsFound.length ? `<div class="table-wrap"><table class="table"><thead><tr><th>Doctor</th><th>Specialization</th><th>Status</th></tr></thead><tbody>${doctorsFound.map(d => `<tr><td><b>${escapeHtml(d.name)}</b><br><small>${escapeHtml(d.id)}</small></td><td>${escapeHtml(d.spec)}</td><td>${badge(d.status)}</td></tr>`).join('')}</tbody></table></div>` : emptyState('No matching doctors.', 'No doctor record matches this search.')}</section></div>`;
}
function render() {
  if (!token()) { redirectToAuthentication(); return; }
  const view = location.hash.slice(1).split('?')[0] || 'dashboard';
  const pages = { dashboard, patients: patientsPage, queue: queuePage, doctors: doctorsPage, treatments: treatmentsPage, statistics: statisticsPage, data: dataPage, about: aboutPage, search: searchPage };
  document.querySelectorAll('.nav-item').forEach(x => { const active = x.dataset.view === view; x.classList.toggle('active', active); if (active) x.setAttribute('aria-current', 'page'); else x.removeAttribute('aria-current'); });
  content.innerHTML = state.loading ? '<div class="loading-page"><span class="spinner" aria-hidden="true"></span><span>Loading hospital data…</span></div>' : (pages[view] || dashboard)();
  bindActions();
}
async function refreshAndRender(message, type = 'success') {
  state.loading = true; render();
  try { await refreshData(); if (message) toast(message, type); }
  catch (error) { toast(error.message, 'error'); }
  finally { render(); }
}
function bindActions() {
  document.querySelectorAll('[data-go]').forEach(b => b.onclick = () => { location.hash = b.dataset.go; closeDrawer(); });
  document.querySelectorAll('.nav-item').forEach(b => b.onclick = () => { location.hash = b.dataset.view; closeDrawer(); });
  document.querySelectorAll('.detail').forEach(b => b.onclick = () => openPatientDetails(b.dataset.id));
  document.querySelectorAll('[data-search-patient]').forEach(b => b.onclick = () => openPatientDetails(b.dataset.searchPatient));
  document.getElementById('addPatient')?.addEventListener('click', openPatientModal);
  document.getElementById('emptyAddDoctor')?.addEventListener('click', openDoctorModal);
  document.getElementById('addDoctor')?.addEventListener('click', openDoctorModal);
  document.getElementById('quickAddPatient')?.addEventListener('click', openPatientModal);
  document.getElementById('quickAddDoctor')?.addEventListener('click', openDoctorModal);
  document.getElementById('retryDashboard')?.addEventListener('click', () => refreshAndRender());
  document.getElementById('callNext')?.addEventListener('click', async e => action(e.currentTarget, 'Calling next…', async () => { const p = await post('/api/queue/call-next'); await refreshAndRender(`Patient ${p.id} ${p.name} called.`, 'success'); }));
  document.getElementById('saveData')?.addEventListener('click', e => action(e.currentTarget, 'Saving…', async () => { const r = await post('/api/data/save'); toast(r.message || 'Data saved successfully.', 'success'); }));
  document.getElementById('loadData')?.addEventListener('click', e => action(e.currentTarget, 'Loading…', async () => { await post('/api/data/load'); await refreshAndRender('Data loaded successfully.', 'success'); }));
  document.querySelectorAll('[data-treatment-tab]').forEach(b => b.onclick = () => { state.treatmentTab = b.dataset.treatmentTab; render(); });
  document.getElementById('patientSearch')?.addEventListener('input', renderPatientTable);
  document.getElementById('typeFilter')?.addEventListener('change', renderPatientTable);
  document.getElementById('statusFilter')?.addEventListener('change', renderPatientTable);
  if (document.getElementById('patientTable')) renderPatientTable();
  document.querySelectorAll('.completeTreatment').forEach(b => b.onclick = () => openCompleteTreatmentModal(b.dataset.id));
}
async function action(button, label, callback) {
  if (!button || button.disabled) return;
  actionInProgressCount++;
  setButtonLoading(button, true, label);
  try { await callback(); } catch (error) { toast(error.message, 'error'); }
  finally {
    actionInProgressCount--;
    if (button.isConnected) setButtonLoading(button, false);
  }
}
function showModal(title, body, onSubmit, className = '') {
  closeModal();
  modalReturnFocus = document.activeElement;
  modalRoot.innerHTML = `<div class="modal-backdrop" data-modal-backdrop><section class="modal ${className}" role="dialog" aria-modal="true" aria-labelledby="modalTitle" tabindex="-1"><header class="modal-header"><h2 id="modalTitle">${title}</h2><button type="button" class="icon-btn modal-close" aria-label="Close dialog">×</button></header><div class="modal-body">${body}</div></section></div>`;
  activeModal = modalRoot.querySelector('.modal');
  modalRoot.querySelector('.modal-close').onclick = closeModal;
  modalRoot.querySelector('[data-modal-backdrop]').onclick = e => { if (e.target === e.currentTarget) closeModal(); };
  activeModal.addEventListener('keydown', e => {
    if (e.key === 'Escape') { e.stopPropagation(); closeModal(); }
    if (e.key === 'Tab') {
      const items = [...activeModal.querySelectorAll('button:not(:disabled),input:not(:disabled),select:not(:disabled),textarea:not(:disabled),[tabindex="0"]')];
      if (!items.length) return;
      if (e.shiftKey && document.activeElement === items[0]) { e.preventDefault(); items[items.length - 1].focus(); }
      else if (!e.shiftKey && document.activeElement === items[items.length - 1]) { e.preventDefault(); items[0].focus(); }
    }
  });
  activeModal.querySelector('form')?.addEventListener('submit', e => { e.preventDefault(); onSubmit?.(e); });
  activeModal.querySelector('input,select,textarea,button')?.focus();
  document.body.classList.add('modal-open');
  appShell.inert = true;
}
function closeModal() {
  modalRoot.innerHTML = ''; activeModal = null; document.body.classList.remove('modal-open'); appShell.inert = false;
  if (modalReturnFocus?.isConnected) modalReturnFocus.focus();
  modalReturnFocus = null;
}
function openPatientModal() {
  showModal('Register patient', `<form id="patientForm"><div class="form-grid"><div class="field"><label for="newPatientId">Patient ID</label><input id="newPatientId" type="number" min="1" required></div><div class="field"><label for="newPatientName">Full name</label><input id="newPatientName" maxlength="100" required></div><div class="field"><label for="newPatientAge">Age</label><input id="newPatientAge" type="number" min="0" max="130" required></div><div class="field"><label for="newPatientSeverity">Severity</label><select id="newPatientSeverity"><option value="5">5 · Critical</option><option value="4">4 · High</option><option value="3">3 · Moderate</option><option value="2">2 · Low</option><option value="1">1 · Mild</option></select></div><div class="field full"><label for="newPatientCondition">Condition</label><input id="newPatientCondition" maxlength="200" required></div><div class="field"><label for="newPatientType">Patient type</label><select id="newPatientType"><option value="emergency">Emergency</option><option value="regular">Regular</option></select></div><label class="check-field"><input id="newPatientCritical" type="checkbox"> Critical condition</label></div><div class="modal-actions"><button type="button" class="btn secondary" data-cancel>Cancel</button><button type="submit" class="btn" id="submitPatient">Register patient</button></div></form>` , async () => {
    const button = document.getElementById('submitPatient'); const type = document.getElementById('newPatientType').value;
    await action(button, 'Registering…', async () => { await post('/api/patients', { id: Number(document.getElementById('newPatientId').value), name: document.getElementById('newPatientName').value.trim(), age: Number(document.getElementById('newPatientAge').value), condition: document.getElementById('newPatientCondition').value.trim(), severity: Number(document.getElementById('newPatientSeverity').value), type, critical: type === 'emergency' && document.getElementById('newPatientCritical').checked }); closeModal(); await refreshAndRender('Patient registered successfully.', 'success'); });
  });
  modalRoot.querySelector('[data-cancel]').onclick = closeModal;
}
function openDoctorModal() {
  showModal('Add doctor', `<form id="doctorForm"><div class="form-grid"><div class="field"><label for="doctorId">Doctor ID</label><input id="doctorId" type="number" min="1" required></div><div class="field"><label for="doctorName">Full name</label><input id="doctorName" maxlength="100" required></div><div class="field full"><label for="doctorSpecialization">Specialization</label><select id="doctorSpecialization" required><option value="">Select specialization</option><option>General Medicine</option><option>Cardiology</option><option>Neurology</option><option>Orthopedics</option><option>Pediatrics</option><option>Dermatology</option><option>Gynecology</option><option>Oncology</option><option>Emergency Medicine</option><option>Surgery</option><option value="Other">Other</option></select></div><div class="field full" id="customSpecializationField" hidden><label for="customSpecialization">Custom specialization</label><input id="customSpecialization" maxlength="100"></div></div><div class="modal-actions"><button type="button" class="btn secondary" data-cancel>Cancel</button><button type="submit" class="btn" id="submitDoctor">Add doctor</button></div></form>`, async () => {
    const button = document.getElementById('submitDoctor');
    const selected = document.getElementById('doctorSpecialization').value;
    const custom = document.getElementById('customSpecialization').value.trim();
    const specialization = selected === 'Other' ? custom : selected;
    if (!specialization) {
      const input = selected === 'Other' ? document.getElementById('customSpecialization') : document.getElementById('doctorSpecialization');
      input.setCustomValidity('Please select or enter a specialization.'); input.reportValidity(); input.setCustomValidity('');
      return;
    }
    await action(button, 'Adding…', async () => { await post('/api/doctors', { id: Number(document.getElementById('doctorId').value), name: document.getElementById('doctorName').value.trim(), specialization }); closeModal(); await refreshAndRender('Doctor added successfully.', 'success'); });
  });
  const specializationSelect = modalRoot.querySelector('#doctorSpecialization');
  const customField = modalRoot.querySelector('#customSpecializationField');
  const customInput = modalRoot.querySelector('#customSpecialization');
  specializationSelect.addEventListener('change', () => {
    const showCustom = specializationSelect.value === 'Other';
    customField.hidden = !showCustom;
    customInput.required = showCustom;
    if (!showCustom) customInput.value = '';
  });
  modalRoot.querySelector('[data-cancel]').onclick = closeModal;
}
function openPatientDetails(id) {
  const p = state.patients.find(x => x.id === String(id)); if (!p) return;
  const available = state.doctors.filter(d => d.status === 'Available');
  const canAssign = p.status === 'Called' && available.length > 0;
  const assignedDoctor = state.doctors.find(d => d.patient === p.id);
  showModal(canAssign ? 'Assign doctor' : `Patient ${escapeHtml(p.id)}`, `<div class="patient-detail-grid"><div><span>Name</span><b>${escapeHtml(p.name)}</b></div><div><span>Status</span>${badge(p.status)}</div><div><span>Type</span>${badge(p.type)}</div><div><span>Age</span><b>${escapeHtml(p.age)}</b></div><div><span>Condition</span><b>${escapeHtml(p.condition)}</b></div><div><span>Severity</span>${severityBadge(Number(p.severity))}</div><div><span>Waiting time</span><b>${escapeHtml(p.wait)} min</b></div><div><span>Backend priority</span><b>${escapeHtml(p.priority)}</b></div></div>${canAssign ? `<div class="field modal-followup"><label for="assignDoctor">Available doctor</label><select id="assignDoctor">${available.map(d => `<option value="${escapeHtml(d.id)}">${escapeHtml(d.name)} · ${escapeHtml(d.spec)}</option>`).join('')}</select><button id="assignDoctorButton" class="btn">Assign doctor</button></div>` : ''}${p.status === 'Assigned' && assignedDoctor ? `<div class="modal-followup"><p class="modal-intro">${escapeHtml(assignedDoctor.name)} is assigned. Confirm the treatment start.</p><button id="startTreatmentButton" class="btn">Start treatment</button></div>` : ''}<div class="modal-actions"><button class="btn secondary" data-close>Close</button></div>`);
  modalRoot.querySelector('[data-close]').onclick = closeModal;
  modalRoot.querySelector('#assignDoctorButton')?.addEventListener('click', e => action(e.currentTarget, 'Assigning…', async () => {
    const doctorId = Number(document.getElementById('assignDoctor').value);
    await post(`/api/patients/${encodeURIComponent(p.id)}/assign-doctor`, { doctorId });
    closeModal(); await refreshAndRender('Doctor assigned. Treatment has not started.', 'success');
  }));
  modalRoot.querySelector('#startTreatmentButton')?.addEventListener('click', e => action(e.currentTarget, 'Starting…', async () => {
    await post(`/api/patients/${encodeURIComponent(p.id)}/start-treatment`); closeModal();
    await refreshAndRender('Treatment started.', 'success');
  }));
}
function openCompleteTreatmentModal(patientId) {
  const patient = state.patients.find(p => p.id === String(patientId)); const doctor = state.doctors.find(d => d.patient === String(patientId));
  if (!patient || !doctor) { toast('No assigned doctor was found for this patient.', 'error'); return; }
  showModal('Complete treatment', `<p class="modal-intro">Record the treatment outcome for ${escapeHtml(patient.name)} with ${escapeHtml(doctor.name)}.</p><form><div class="field"><label for="diagnosis">Diagnosis</label><input id="diagnosis" maxlength="300" required></div><div class="field"><label for="treatmentDescription">Treatment details</label><textarea id="treatmentDescription" maxlength="2000" required></textarea></div><div class="modal-actions"><button type="button" class="btn secondary" data-cancel>Cancel</button><button type="submit" class="btn" id="submitTreatment">Complete treatment</button></div></form>`, async () => {
    const button = document.getElementById('submitTreatment');
    await action(button, 'Completing…', async () => { await post(`/api/patients/${encodeURIComponent(patientId)}/complete-treatment`, { doctorId: Number(doctor.id), diagnosis: document.getElementById('diagnosis').value.trim(), description: document.getElementById('treatmentDescription').value.trim() }); closeModal(); await refreshAndRender('Treatment completed successfully.', 'success'); });
  });
  modalRoot.querySelector('[data-cancel]').onclick = closeModal;
}
function closeDrawer() { document.getElementById('sidebar').classList.remove('drawer-open'); document.getElementById('drawerBackdrop').hidden = true; document.getElementById('menuToggle').setAttribute('aria-expanded', 'false'); }
document.getElementById('menuToggle').onclick = () => {
  const sidebar = document.getElementById('sidebar'); const open = !sidebar.classList.contains('drawer-open'); sidebar.classList.toggle('drawer-open', open);
  document.getElementById('drawerBackdrop').hidden = !open; document.getElementById('menuToggle').setAttribute('aria-expanded', String(open));
};
document.getElementById('drawerBackdrop').onclick = closeDrawer;
document.getElementById('profileToggle').onclick = () => { const menu = document.getElementById('profileDropdown'); menu.hidden = !menu.hidden; document.getElementById('profileToggle').setAttribute('aria-expanded', String(!menu.hidden)); };
document.getElementById('logoutButton').onclick = logout;
document.getElementById('profileAction').onclick = () => {
  const user = state.user || {};
  showModal('Profile', `<div class="patient-detail-grid"><div><span>Full name</span><b>${escapeHtml(user.fullName || '—')}</b></div><div><span>Username or email</span><b>${escapeHtml(user.username || '—')}</b></div><div><span>Account type</span><b>Hospital Staff</b></div></div><div class="modal-actions"><button class="btn" data-close>Done</button></div>`);
  modalRoot.querySelector('[data-close]').onclick = closeModal;
};
document.addEventListener('click', event => { if (!event.target.closest('.profile-menu')) { document.getElementById('profileDropdown').hidden = true; document.getElementById('profileToggle').setAttribute('aria-expanded', 'false'); } });
document.addEventListener('keydown', event => {
  if (event.key !== 'Escape') return;
  if (activeModal) closeModal();
  else if (document.getElementById('profileDropdown').hidden === false) {
    document.getElementById('profileDropdown').hidden = true;
    document.getElementById('profileToggle').setAttribute('aria-expanded', 'false');
    document.getElementById('profileToggle').focus();
  } else closeDrawer();
});
document.getElementById('globalSearch').addEventListener('keydown', event => {
  if (event.key !== 'Enter' || !state.user) return;
  state.searchQuery = event.currentTarget.value.trim();
  if (state.searchQuery) location.hash = 'search';
  else showToast('Enter a patient or doctor name, ID, or specialization.', 'info');
  if (location.hash === '#search') render();
});
window.addEventListener('hashchange', () => {
  if (!token()) { redirectToAuthentication(); return; }
  if (!location.hash || location.hash === '#login' || location.hash === '#register') { location.hash = 'dashboard'; return; }
  if (!appShell.hidden) render();
});
let periodicRefreshInProgress = false;
window.setInterval(async () => {
  const view = location.hash.slice(1).split('?')[0] || 'dashboard';
  if (!token() || appShell.hidden || document.hidden || activeModal || state.loading ||
      actionInProgressCount > 0 || periodicRefreshInProgress ||
      !['dashboard', 'patients', 'queue', 'statistics'].includes(view)) return;
  periodicRefreshInProgress = true;
  const patientFilters = view === 'patients' ? {
    query: document.getElementById('patientSearch')?.value || '',
    type: document.getElementById('typeFilter')?.value || 'All types',
    status: document.getElementById('statusFilter')?.value || 'All statuses'
  } : null;
  try {
    await refreshData();
    render();
    if (patientFilters) {
      const search = document.getElementById('patientSearch');
      const type = document.getElementById('typeFilter');
      const status = document.getElementById('statusFilter');
      if (search) search.value = patientFilters.query;
      if (type) type.value = patientFilters.type;
      if (status) status.value = patientFilters.status;
      renderPatientTable();
    }
  } catch (_) {
    if (view === 'dashboard') render();
  } finally { periodicRefreshInProgress = false; }
}, 60000);
(async function initialize() {
  if (!token()) { redirectToAuthentication(); return; }
  try { state.user = JSON.parse(sessionStorage.getItem(USER_KEY) || 'null'); await enterApp(); }
  catch (_) { expireSession(); }
})();
