# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

"博客XIU", a static personal blog themed 灵感与感悟 (author persona 诺尔曼) (Chinese-language, `lang="zh-CN"`): plain HTML + CSS + vanilla JS. No package manager, linter, or test suite; the only build step is `python3 build.py` (stdlib only), whose output is committed.

## Commands

```bash
python3 build.py              # regenerate post list, category filters, per-post tags and feed.xml
python3 -m http.server 8000   # preview at http://localhost:8000 (or open index.html directly)
```

Visual check in the cloud container (no Playwright Python package installed; use the bundled headless shell, which honors narrow widths — full `chrome --headless` clamps small window sizes and crops mobile shots):

```bash
H=$(find /opt/pw-browsers -name headless_shell -type f | head -1)
$H --no-sandbox --hide-scrollbars --window-size=390,1800 --screenshot=out.png file://$PWD/index.html
```

## Structure

- `index.html` — home page; sections `#about`, `#posts`, `#contact` are linked from the sticky nav.
- `posts/*.html` — one standalone page per article, using `../style.css` and `../script.js`. Post metadata lives in `<head>`: `description` (summary), `date` (YYYY-MM-DD), `tags` (comma-separated categories); the title is the first `<h1>`. Nav/header/footer markup is duplicated across pages and must be edited in each.
- `build.py` — reads that metadata and rewrites everything between `<!-- name:start -->`/`<!-- name:end -->` markers (`filters` and `posts` in `index.html`, `post-tags` in each post) plus `feed.xml`. Never hand-edit inside markers; add a post by copying `posts/hello-world.html`, editing its metadata, then running the build. `SITE_URL` in it must match the Pages URL for RSS links to work.
- `style.css` — all colors are CSS variables on `:root`. Dark theme is defined twice and both copies must stay in sync: under `@media (prefers-color-scheme: dark)` guarded by `:root:not([data-theme="light"])`, and under `:root[data-theme="dark"]`.
- `script.js` — footer year, category filter on the home page (matches button `data-tag` against each `<li data-tags>`), plus the theme toggle: sets `data-theme` on `<html>` and persists it to `localStorage` key `theme` (wrapped in try/catch so it degrades when storage is blocked).

## Deployment

Intended for GitHub Pages (Settings → Pages, serve from branch root).
