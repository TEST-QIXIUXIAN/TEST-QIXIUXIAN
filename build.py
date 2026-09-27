#!/usr/bin/env python3
"""Regenerate the post list, category filters and feed.xml from posts/*.html.

Each post declares its metadata in <head>:
  <meta name="description" content="...">   summary
  <meta name="date" content="YYYY-MM-DD">
  <meta name="tags" content="灵感,感悟">
and its title in the first <h1>.

Run after adding or editing a post:  python3 build.py
"""
import html
import re
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).parent
SITE_URL = "https://test-qixiuxian.github.io/TEST-QIXIUXIAN/"
SITE_TITLE = "博客XIU"
SITE_DESC = "灵感与感悟的诺尔曼"


def meta(src, name):
    m = re.search(rf'<meta name="{name}" content="([^"]*)"', src)
    if not m:
        raise SystemExit(f'missing <meta name="{name}">')
    return html.unescape(m.group(1))


def load_posts():
    posts = []
    for path in sorted((ROOT / "posts").glob("*.html")):
        src = path.read_text(encoding="utf-8")
        title = re.search(r"<h1>(.*?)</h1>", src, re.S)
        if not title:
            raise SystemExit(f"{path}: missing <h1>")
        try:
            posts.append({
                "path": f"posts/{path.name}",
                "file": path,
                "title": html.unescape(title.group(1).strip()),
                "summary": meta(src, "description"),
                "date": meta(src, "date"),
                "tags": [t.strip() for t in meta(src, "tags").split(",") if t.strip()],
            })
        except SystemExit as e:
            raise SystemExit(f"{path}: {e}")
    posts.sort(key=lambda p: (p["date"], p["path"]), reverse=True)
    return posts


def replace_block(src, name, body, indent):
    pattern = re.compile(rf"(<!-- {name}:start -->\n)(?:.*?\n)?({indent}<!-- {name}:end -->)", re.S)
    if not pattern.search(src):
        raise SystemExit(f"missing <!-- {name}:start/end --> markers")
    return pattern.sub(lambda m: m.group(1) + body + "\n" + m.group(2), src)


def tag_spans(tags):
    return "".join(f'<span class="post-tag">{html.escape(t)}</span>' for t in tags)


def build_index(posts):
    tags = []
    for p in posts:
        tags += [t for t in p["tags"] if t not in tags]

    filters = "\n".join(
        [f'        <button type="button" data-tag="" aria-pressed="true">全部</button>']
        + [f'        <button type="button" data-tag="{html.escape(t)}" aria-pressed="false">{html.escape(t)}</button>'
           for t in tags]
    )

    items = []
    for p in posts:
        items.append(f'''        <li data-tags="{html.escape(",".join(p["tags"]))}">
          <a class="post-link" href="{p["path"]}">
            <time datetime="{p["date"]}">{p["date"]}</time>
            <span class="post-title">{html.escape(p["title"])}</span>
            <span class="post-summary">{html.escape(p["summary"])}</span>
            <span class="post-tags">{tag_spans(p["tags"])}</span>
          </a>
        </li>''')

    index = ROOT / "index.html"
    src = index.read_text(encoding="utf-8")
    src = replace_block(src, "filters", filters, "        ")
    src = replace_block(src, "posts", "\n".join(items), "        ")
    index.write_text(src, encoding="utf-8")


def og_tags(p):
    esc = html.escape
    return "\n".join([
        '  <meta property="og:type" content="article">',
        f'  <meta property="og:site_name" content="{esc(SITE_TITLE)}">',
        f'  <meta property="og:title" content="{esc(p["title"])}">',
        f'  <meta property="og:description" content="{esc(p["summary"])}">',
        f'  <meta property="og:url" content="{SITE_URL}{p["path"]}">',
        f'  <meta property="og:image" content="{SITE_URL}og.png">',
        f'  <meta property="article:published_time" content="{p["date"]}">',
        '  <meta name="twitter:card" content="summary_large_image">',
    ])


def build_posts(posts):
    for p in posts:
        src = p["file"].read_text(encoding="utf-8")
        src = replace_block(src, "og", og_tags(p), "  ")
        src = replace_block(src, "post-tags", f"      <span class=\"post-tags\">{tag_spans(p['tags'])}</span>", "      ")
        p["file"].write_text(src, encoding="utf-8")


def rfc822(date):
    d = datetime.strptime(date, "%Y-%m-%d").replace(tzinfo=timezone.utc)
    return d.strftime("%a, %d %b %Y 00:00:00 +0000")


def build_feed(posts):
    esc = html.escape
    items = []
    for p in posts:
        url = SITE_URL + p["path"]
        cats = "".join(f"\n      <category>{esc(t)}</category>" for t in p["tags"])
        items.append(f"""    <item>
      <title>{esc(p["title"])}</title>
      <link>{url}</link>
      <guid>{url}</guid>
      <pubDate>{rfc822(p["date"])}</pubDate>
      <description>{esc(p["summary"])}</description>{cats}
    </item>""")
    feed = f"""<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
  <channel>
    <title>{esc(SITE_TITLE)}</title>
    <link>{SITE_URL}</link>
    <atom:link href="{SITE_URL}feed.xml" rel="self" type="application/rss+xml"/>
    <description>{esc(SITE_DESC)}</description>
    <language>zh-CN</language>
{chr(10).join(items)}
  </channel>
</rss>
"""
    (ROOT / "feed.xml").write_text(feed, encoding="utf-8")


if __name__ == "__main__":
    posts = load_posts()
    build_index(posts)
    build_posts(posts)
    build_feed(posts)
    print(f"built {len(posts)} post(s)")
