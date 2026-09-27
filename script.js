document.getElementById('year').textContent = new Date().getFullYear();

const root = document.documentElement;
const THEME_KEY = 'theme';

try {
  const saved = localStorage.getItem(THEME_KEY);
  if (saved) root.dataset.theme = saved;
} catch (e) {}

document.getElementById('theme-toggle').addEventListener('click', () => {
  const current = root.dataset.theme
    || (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
  const next = current === 'dark' ? 'light' : 'dark';
  root.dataset.theme = next;
  try { localStorage.setItem(THEME_KEY, next); } catch (e) {}
});
