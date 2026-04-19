#!/usr/bin/env python3
"""Bump the project version across every version-bearing file in the repo.

Usage:
    python scripts/bump_version.py 0.7.5

Each regex must match exactly once; a zero- or multi-match outcome is fatal so
that silent drift (e.g. a file format change) fails loudly in CI.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

TARGETS: list[tuple[str, str, str]] = [
    (
        "CMakeLists.txt",
        r"(?m)^(\s+)VERSION \d+\.\d+\.\d+$",
        r"\1VERSION {v}",
    ),
    (
        "bindings/python/pyproject.toml",
        r'(?m)^version = "\d+\.\d+\.\d+"$',
        r'version = "{v}"',
    ),
    (
        "bindings/js/deno.json",
        r'"version":\s*"\d+\.\d+\.\d+"',
        r'"version": "{v}"',
    ),
    (
        "bindings/dart/pubspec.yaml",
        r"(?m)^version:\s*\d+\.\d+\.\d+$",
        r"version: {v}",
    ),
    (
        "bindings/julia/Project.toml",
        r'(?m)^version = "\d+\.\d+\.\d+"$',
        r'version = "{v}"',
    ),
]

VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")
DIGITS_RE = re.compile(r"\d+\.\d+\.\d+")


def bump_file(rel_path: str, pattern: str, replacement_tpl: str, new_version: str) -> None:
    path = REPO_ROOT / rel_path
    text = path.read_text(encoding="utf-8")

    compiled = re.compile(pattern)
    matches = list(compiled.finditer(text))
    if len(matches) != 1:
        raise SystemExit(
            f"{rel_path}: expected exactly 1 match for /{pattern}/, found {len(matches)}"
        )

    old_version = DIGITS_RE.search(matches[0].group(0)).group(0)
    if old_version == new_version:
        raise SystemExit(f"{rel_path}: already at {new_version}")

    new_text, n = compiled.subn(replacement_tpl.format(v=new_version), text, count=1)
    if n != 1:
        raise SystemExit(f"{rel_path}: substitution failed (n={n})")

    path.write_text(new_text, encoding="utf-8")
    print(f"{rel_path}: {old_version} -> {new_version}")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <new_version>", file=sys.stderr)
        return 2

    new_version = sys.argv[1]
    if not VERSION_RE.match(new_version):
        print(f"Invalid version {new_version!r}: expected MAJOR.MINOR.PATCH", file=sys.stderr)
        return 2

    for rel_path, pattern, replacement in TARGETS:
        bump_file(rel_path, pattern, replacement, new_version)

    return 0


if __name__ == "__main__":
    sys.exit(main())
