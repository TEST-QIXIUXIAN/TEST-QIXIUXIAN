#!/usr/bin/env python3
"""Add every oc command missing from seed.json to the "OpenShift 命令大全" category.

Usage:
  python3 oc_commands.py /path/to/oc > oc-commands.json
  python3 sync_oc.py oc-commands.json        # then: python3 ../gen_page.py

Chinese titles come from oc_zh.json (command -> 中文说明); a command missing
there falls back to its English summary and is reported, so add it to
oc_zh.json and run again. Existing entries are never changed or removed:
the app de-duplicates by title + code, so editing one would show up as a
second copy for users who already have it.
"""
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
SEED = HERE.parent / "seed.json"
CATEGORY = "OpenShift 命令大全"
SEP = "："


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    official = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
    zh = json.loads((HERE / "oc_zh.json").read_text(encoding="utf-8"))
    seed = json.loads(SEED.read_text(encoding="utf-8"))

    have = {r["title"].split(SEP, 1)[0] for r in seed if r["category"] == CATEGORY}
    new = []
    for c in official:
        if c["command"] in have:
            continue
        title = zh.get(c["command"])
        if not title:
            print("no Chinese title in oc_zh.json:", c["command"], file=sys.stderr)
            title = c["summary"]
        new.append({
            "category": CATEGORY,
            "title": c["command"] + SEP + title,
            "code": c["examples"] or c["usage"],
            "note": "官方说明：" + c["summary"],
        })

    # keep the category together: insert right after its last entry
    last = max((i for i, r in enumerate(seed) if r["category"] == CATEGORY), default=len(seed) - 1)
    seed[last + 1:last + 1] = new
    SEED.write_text(json.dumps(seed, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    for r in new:
        print("added:", r["title"])
    print("%d new, %d already present" % (len(new), len(official) - len(new)))


if __name__ == "__main__":
    main()
