#!/usr/bin/env python3
"""Check the Markdown documentation of the repository.

- every local link points to an existing file, and its #anchor to an existing
  heading (anchors as GitHub makes them)
- a "## Contents" list names every level 2 and 3 heading after it, in order

Run from anywhere: python3 tools/check_doc_links.py
Exit status 1 and one line per problem if something is wrong.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
LINK = re.compile(r"(?<!!)\[([^\]]*)\]\(([^)\s]+)\)|!\[[^\]]*\]\(([^)\s]+)\)")
FENCE = re.compile(r"^\s*(```|~~~)")


def slug(text):
    """Anchor of a heading as GitHub makes it: links reduced to their text,
    lower case, punctuation (also ` and *) removed, spaces to hyphens."""
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", text).strip().lower()
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


def parse(path):
    """Headings (level, text, anchor, line) and links (target, line)."""
    headings, text, seen = [], [], {}
    in_fence = False
    for no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if FENCE.match(line):
            in_fence = not in_fence
            line = ""
        if in_fence:
            line = ""
        m = HEADING.match(line)
        if m:
            base = slug(m.group(2))
            n = seen.get(base, 0)
            seen[base] = n + 1
            headings.append((len(m.group(1)), m.group(2), base if n == 0 else f"{base}-{n}", no))
        text.append(re.sub(r"`[^`]*`", "", line))
    # links over the whole text: a link text may run over two lines
    text = "\n".join(text)
    links = [(m.group(2) or m.group(3), text.count("\n", 0, m.start()) + 1)
             for m in LINK.finditer(text)]
    return headings, links


def check_links(path, links, anchors_of, problems):
    for target, no in links:
        if re.match(r"[a-z]+:", target):
            continue  # http:, https:, mailto:
        file_part, _, anchor = target.partition("#")
        dest = (path.parent / file_part).resolve() if file_part else path
        where = f"{path.relative_to(ROOT)}:{no}"
        if not dest.exists():
            problems.append(f"{where}: missing file {target}")
        elif anchor:
            if dest.suffix != ".md":
                problems.append(f"{where}: anchor on a file that is not Markdown: {target}")
            elif anchor not in anchors_of(dest):
                problems.append(f"{where}: no heading for #{anchor} in {dest.relative_to(ROOT)}")


def check_contents(path, headings, problems):
    lines = path.read_text(encoding="utf-8").splitlines()
    idx = next((i for i, h in enumerate(headings) if h[0] == 2 and h[1] == "Contents"), None)
    if idx is None:
        return
    start = headings[idx][3]
    end = headings[idx + 1][3] - 1 if idx + 1 < len(headings) else len(lines)
    listed = re.findall(r"\]\(#([^)]+)\)", "\n".join(lines[start:end]))
    wanted = [h[2] for h in headings[idx + 1:] if h[0] in (2, 3)]
    if listed != wanted:
        missing = [a for a in wanted if a not in listed]
        extra = [a for a in listed if a not in wanted]
        detail = []
        if missing:
            detail.append("not listed: " + ", ".join(missing))
        if extra:
            detail.append("not a level 2/3 heading after it: " + ", ".join(extra))
        if not detail:
            detail.append("order differs from the headings")
        problems.append(f"{path.relative_to(ROOT)}:{start}: Contents " + "; ".join(detail))


def main():
    files = sorted(p for p in ROOT.rglob("*.md")
                   if ".git" not in p.parts)
    cache = {}

    def parsed(p):
        if p not in cache:
            cache[p] = parse(p)
        return cache[p]

    def anchors_of(p):
        return {h[2] for h in parsed(p)[0]}

    problems = []
    for path in files:
        headings, links = parsed(path)
        check_links(path, links, anchors_of, problems)
        check_contents(path, headings, problems)
    for p in problems:
        print(p)
    print(f"{len(files)} Markdown files checked, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
