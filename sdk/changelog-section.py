#!/usr/bin/env python3
"""Print one version's section of CHANGELOG.md, for release notes.

Usage:  python3 sdk/changelog-section.py 1.0.8 [CHANGELOG.md]

Prints the body under "## [1.0.8] - <date>", without the heading, up to the
next "## [" heading.  Exits 1 when the section is missing or empty, so a
release cannot go out without notes (#249).
"""
import re
import sys


def section(text, version):
    head = re.compile(r"^## \[" + re.escape(version) + r"\]", re.M)
    m = head.search(text)
    if not m:
        return None
    start = text.index("\n", m.end()) + 1 if "\n" in text[m.end():] else len(text)
    nxt = re.compile(r"^## \[", re.M).search(text, start)
    return text[start:nxt.start() if nxt else len(text)].strip("\n")


def main():
    if len(sys.argv) not in (2, 3):
        print(__doc__.strip(), file=sys.stderr)
        return 2
    version = sys.argv[1]
    path = sys.argv[2] if len(sys.argv) == 3 else "CHANGELOG.md"
    with open(path, encoding="utf-8") as f:
        body = section(f.read(), version)
    if not body or not body.strip():
        print(f"changelog-section: no section for [{version}] in {path}",
              file=sys.stderr)
        return 1
    print(body)
    return 0


if __name__ == "__main__":
    sys.exit(main())
