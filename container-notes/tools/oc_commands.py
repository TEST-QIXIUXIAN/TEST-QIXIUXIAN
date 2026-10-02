#!/usr/bin/env python3
"""List every oc (OpenShift CLI) command with its official description and examples.

Walks `oc <cmd> --help` recursively and prints JSON:
  [{"command": "oc adm drain", "summary": "...", "usage": "...", "examples": "..."}, ...]

Usage: python3 oc_commands.py /path/to/oc > oc-commands.json

Used to keep the "OpenShift 命令大全" category in seed.json complete: any
command printed here but missing from seed.json is new in the official CLI.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

SKIP = {"help"}
CMD_LINE = re.compile(r"^  ([a-z][\w-]*)\s{2,}(.*)$")


def run_help(oc, path):
    env = dict(os.environ, KUBECONFIG=os.path.join(tempfile.gettempdir(), "oc-help-none"))
    out = subprocess.run([oc] + path + ["--help"], capture_output=True, text=True, env=env, timeout=60)
    return out.stdout


def parse(text):
    """Returns (description, examples, usage, [(subcommand, summary)])."""
    desc, examples, usage, subs = [], [], [], []
    section = None
    for line in text.splitlines():
        if line and not line[0].isspace() and line.rstrip().endswith(":"):
            section = line.rstrip()[:-1]
            continue
        if section is None:
            desc.append(line)
        elif section == "Examples":
            examples.append(line[2:] if line.startswith("  ") else line)
        elif section == "Usage" and line.strip():
            usage.append(line.strip())
        elif section.endswith("Commands"):
            m = CMD_LINE.match(line)
            if m:
                subs.append((m.group(1), m.group(2).strip()))
    return "\n".join(desc).strip(), "\n".join(examples).strip(), usage[0] if usage else "", subs


def walk(oc, path, summary, out):
    desc, examples, usage, subs = parse(run_help(oc, path))
    if path and (examples or not subs):
        out.append({"command": " ".join(["oc"] + path), "summary": summary or desc.split("\n")[0],
                    "usage": usage, "examples": examples})
    for name, sub_summary in subs:
        if name not in SKIP:
            walk(oc, path + [name], sub_summary, out)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = []
    walk(sys.argv[1], [], "", out)
    out.sort(key=lambda r: r["command"])
    json.dump(out, sys.stdout, ensure_ascii=False, indent=1)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
