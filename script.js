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

const filters = document.querySelector('.filters');
if (filters) {
  const items = document.querySelectorAll('.post-list > li');
  const empty = document.querySelector('.empty');
  filters.addEventListener('click', (e) => {
    const btn = e.target.closest('button');
    if (!btn) return;
    const tag = btn.dataset.tag;
    filters.querySelectorAll('button').forEach((b) => b.setAttribute('aria-pressed', String(b === btn)));
    let shown = 0;
    items.forEach((li) => {
      const match = !tag || li.dataset.tags.split(',').includes(tag);
      li.hidden = !match;
      if (match) shown++;
    });
    empty.hidden = shown > 0;
  });
}
