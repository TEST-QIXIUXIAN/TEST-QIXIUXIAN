# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

"博客XIU", a static personal blog themed 灵感与感悟 (author persona 诺尔曼) (Chinese-language, `lang="zh-CN"`): plain HTML + CSS + vanilla JS. No package manager, linter, or test suite; the only build step is `python3 build.py` (stdlib only), whose output is committed.

## Commands

```bash
python3 build.py              # regenerate post list, category filters, per-post tags and feed.xml
python3 -m http.server 8000   # preview at http://localhost:8000 (or open index.html directly)
docker compose up -d --build  # containerized: nginx on http://localhost:8080
```

`Dockerfile` is two-stage: `python:3.12-alpine` runs `build.py`, then `nginx:1.27-alpine` serves an explicit list of files copied into `/usr/share/nginx/html` — a new top-level asset must be added to that `COPY` line or it will 404 in the container (GitHub Pages is unaffected). nginx config is `deploy/default.conf.template`, rendered by the nginx image's envsubst at startup so it listens on `$PORT` (default 80; Render/Railway inject their own). Don't add a `[::]` listen line — nginx fails to start on hosts without IPv6. In the cloud container `dockerd` isn't started by default (`dockerd &`), and Docker Hub pulls may hit 429 rate limits; pull via `mirror.gcr.io/library/<image>` and `docker tag` it to the plain name.

Visual check in the cloud container (no Playwright Python package installed; use the bundled headless shell, which honors narrow widths — full `chrome --headless` clamps small window sizes and crops mobile shots):

```bash
H=$(find /opt/pw-browsers -name headless_shell -type f | head -1)
$H --no-sandbox --hide-scrollbars --window-size=390,1800 --screenshot=out.png file://$PWD/index.html
```

## Structure

- `index.html` — home page; sections `#about`, `#posts`, `#contact` are linked from the sticky nav.
- `posts/*.html` — one standalone page per article, using `../style.css` and `../script.js`. Post metadata lives in `<head>`: `description` (summary), `date` (YYYY-MM-DD), `tags` (comma-separated categories); the title is the first `<h1>`. Nav/header/footer markup is duplicated across pages and must be edited in each.
- `build.py` — reads that metadata and rewrites everything between `<!-- name:start -->`/`<!-- name:end -->` markers (`filters` and `posts` in `index.html`; `og` share-preview meta and `post-tags` in each post) plus `feed.xml`. Never hand-edit inside markers; add a post by copying `posts/hello-world.html`, editing its metadata, then running the build. `SITE_URL` in it must match the Pages URL for RSS and share-preview links to work (the home page's `og:*` tags hardcode the same URL).
- `og.png` (1200×630 share image), `favicon.svg`, `apple-touch-icon.png` — static assets; the PNGs were rendered from HTML with the headless shell, so regenerate them the same way if the name/branding changes.
- `style.css` — all colors are CSS variables on `:root`. Dark theme is defined twice and both copies must stay in sync: under `@media (prefers-color-scheme: dark)` guarded by `:root:not([data-theme="light"])`, and under `:root[data-theme="dark"]`.
- `script.js` — footer year, category filter on the home page (matches button `data-tag` against each `<li data-tags>`), plus the theme toggle: sets `data-theme` on `<html>` and persists it to `localStorage` key `theme` (wrapped in try/catch so it degrades when storage is blocked).
- `container-notes/` — standalone C tool (not part of the site or Docker image): a local web app for collecting container commands, aimed at Windows 11. `container_notes.c` serves the UI embedded from `page.h`; after editing `index.html` or `seed.json` there, run `python3 container-notes/gen_page.py` to regenerate `page.h`. Keep the `.c` file ASCII-only (MSVC code-page issues). Cross-check the Windows build with `x86_64-w64-mingw32-gcc ... -lws2_32 -lshell32` (apt package `gcc-mingw-w64-x86-64`). The page auto-merges new entries from `seed.json` on this repo's branch via raw.githubusercontent.com (`UPDATE_URL` in `index.html`), so never edit or reorder existing seed entries — users de-duplicate by title + code and an edit shows up as a second copy; only append. `tools/sync_oc.py` appends missing `oc` commands from `tools/oc_commands.py` output (build `oc` from github.com/openshift/oc with `GOTOOLCHAIN=auto GOFLAGS=-mod=vendor go build -tags 'include_gcs include_oss containers_image_openpgp' ./cmd/oc`; mirror.openshift.com is blocked here).

## Deployment

Live on GitHub Pages at https://test-qixiuxian.github.io/TEST-QIXIUXIAN/, served from the root of branch `claude/hi-vxtgib`; every push redeploys. `.nojekyll` disables Jekyll processing. `render.yaml` is a Render Blueprint deploying the Docker image (free plan, same branch, auto-deploy on push).
