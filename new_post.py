#!/usr/bin/env python3
"""Create a new post from posts/hello-world.html, then run build.py.

  python3 new_post.py "标题" -t 灵感,随笔 -d "一句话摘要"
  python3 new_post.py "标题" -s my-slug --date 2026-10-01

The file is posts/<slug>.html; the slug defaults to the date (with -2, -3 …
appended if taken). The body is a placeholder paragraph to replace.
"""
import argparse
import html
import re
import subprocess
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).parent
TEMPLATE = ROOT / "posts" / "hello-world.html"


def sub_once(pattern, repl, src):
    fn = repl if callable(repl) else (lambda m: repl)
    out, n = re.subn(pattern, fn, src, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f"template no longer matches: {pattern}")
    return out


def pick_path(slug):
    path = ROOT / "posts" / f"{slug}.html"
    n = 2
    while path.exists():
        path = ROOT / "posts" / f"{slug}-{n}.html"
        n += 1
    return path


def main():
    ap = argparse.ArgumentParser(description="新建一篇博客文章")
    ap.add_argument("title", help="文章标题")
    ap.add_argument("-t", "--tags", default="随笔", help="分类，逗号分隔（默认：随笔）")
    ap.add_argument("-d", "--desc", default="", help="摘要（默认：同标题）")
    ap.add_argument("-s", "--slug", help="文件名（不含 .html，默认：日期）")
    ap.add_argument("--date", default=date.today().isoformat(), help="日期 YYYY-MM-DD（默认：今天）")
    ap.add_argument("--no-build", action="store_true", help="不自动运行 build.py")
    args = ap.parse_args()

    try:
        date.fromisoformat(args.date)
    except ValueError:
        raise SystemExit(f"日期格式应为 YYYY-MM-DD：{args.date}")
    slug = args.slug or args.date
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", slug):
        raise SystemExit(f"文件名只能包含字母、数字、- 和 _：{slug}")
    if args.slug and (ROOT / "posts" / f"{slug}.html").exists():
        raise SystemExit(f"posts/{slug}.html 已存在")

    tags = ",".join(t.strip() for t in re.split(r"[,，]", args.tags) if t.strip()) or "随笔"
    title = html.escape(args.title.strip(), quote=False)
    desc = html.escape(args.desc.strip() or args.title.strip())

    src = TEMPLATE.read_text(encoding="utf-8")
    src = sub_once(r"<title>.*?</title>", f"<title>{title} · 博客XIU</title>", src)
    src = sub_once(r'<meta name="description" content="[^"]*">', f'<meta name="description" content="{desc}">', src)
    src = sub_once(r'<meta name="date" content="[^"]*">', f'<meta name="date" content="{args.date}">', src)
    src = sub_once(r'<meta name="tags" content="[^"]*">', f'<meta name="tags" content="{html.escape(tags)}">', src)
    src = sub_once(r"<h1>.*?</h1>", f"<h1>{title}</h1>", src)
    src = sub_once(r'<time class="post-meta" datetime="[^"]*">[^<]*</time>',
                   f'<time class="post-meta" datetime="{args.date}">{args.date}</time>', src)
    # Replace the template's body (between the tags block and the back link).
    src = sub_once(r'(<!-- post-tags:end -->\n).*?(\n      <a class="back")',
                   lambda m: m.group(1) + "\n      <p>在这里写正文。</p>\n" + m.group(2), src)

    path = pick_path(slug)
    path.write_text(src, encoding="utf-8")
    print(f"已创建 {path.relative_to(ROOT)}")

    if not args.no_build:
        subprocess.run([sys.executable, str(ROOT / "build.py")], check=True)
    print("下一步：编辑正文，然后运行 python3 build.py（改了标题/摘要/分类时）")


if __name__ == "__main__":
    main()
