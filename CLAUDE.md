# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

"博客XIU", a static personal blog (Chinese-language, `lang="zh-CN"`): plain HTML + CSS + vanilla JS. No build step, package manager, linter, or test suite.

## Commands

```bash
python3 -m http.server 8000   # preview at http://localhost:8000 (or open index.html directly)
```

Visual check in the cloud container (no Playwright Python package installed; use the bundled headless shell, which honors narrow widths — full `chrome --headless` clamps small window sizes and crops mobile shots):

```bash
H=$(find /opt/pw-browsers -name headless_shell -type f | head -1)
$H --no-sandbox --hide-scrollbars --window-size=390,1800 --screenshot=out.png file://$PWD/index.html
```

## Structure

- `index.html` — home page; sections `#about`, `#posts`, `#contact` are linked from the sticky nav. The about text, skill tags and `you@example.com` are still placeholders.
- `posts/*.html` — one standalone page per article, using `../style.css` and `../script.js` and linking back to `../index.html#…`. There is no generator: adding a post means copying `posts/hello-world.html` and adding a matching `<li>` (newest first) to the `#posts` list in `index.html`; nav/header/footer markup is duplicated across pages and must be edited in each.
- `style.css` — all colors are CSS variables on `:root`. Dark theme is defined twice and both copies must stay in sync: under `@media (prefers-color-scheme: dark)` guarded by `:root:not([data-theme="light"])`, and under `:root[data-theme="dark"]`.
- `script.js` — footer year, plus the theme toggle: sets `data-theme` on `<html>` and persists it to `localStorage` key `theme` (wrapped in try/catch so it degrades when storage is blocked).

## Deployment

Intended for GitHub Pages (Settings → Pages, serve from branch root).
