#!/usr/bin/env python3
"""Verify the project version is consistent across every version-bearing file.

Usage:
    python scripts/bump_version.py --check   # assert every file agrees, print the version

`--check` treats CMakeLists.txt (the first TARGETS entry) as the source of truth and fails
unless every other file matches it; on success it prints the version to stdout so callers
(the detect-version action and the publish workflows' version jobs) can capture it. TARGETS is
the single source of truth for which files carry the version and how each is parsed.

Each regex must match exactly once; a zero- or multi-match outcome is fatal so that silent
drift (e.g. a file format change) fails loudly in CI.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

TARGETS: list[tuple[str, str]] = [
    ("CMakeLists.txt", r"(?m)^(\s+)VERSION \d+\.\d+\.\d+$"),
    ("bindings/python/pyproject.toml", r'(?m)^version = "\d+\.\d+\.\d+"$'),
    ("bindings/js/package.json", r'"version":\s*"\d+\.\d+\.\d+"'),
    ("bindings/dart/pubspec.yaml", r"(?m)^version:\s*\d+\.\d+\.\d+$"),
    ("bindings/julia/Project.toml", r'(?m)^version = "\d+\.\d+\.\d+"$'),
]

DIGITS_RE = re.compile(r"\d+\.\d+\.\d+")


def extract_version(rel_path: str, text: str, pattern: str) -> str:
    """Return the MAJOR.MINOR.PATCH carried by `text`, enforcing exactly one match."""
    matches = list(re.compile(pattern).finditer(text))
    if len(matches) != 1:
        raise SystemExit(
            f"{rel_path}: expected exactly 1 match for /{pattern}/, found {len(matches)}"
        )
    return DIGITS_RE.search(matches[0].group(0)).group(0)


def read_version(rel_path: str, pattern: str) -> str:
    return extract_version(rel_path, (REPO_ROOT / rel_path).read_text(encoding="utf-8"), pattern)


def check() -> int:
    """Assert every file agrees with CMakeLists.txt (the canonical first TARGETS entry)."""
    canonical_path, canonical_pattern = TARGETS[0]
    expected = read_version(canonical_path, canonical_pattern)
    print(f"{canonical_path} (source of truth): {expected}", file=sys.stderr)

    mismatches = []
    for rel_path, pattern in TARGETS[1:]:
        current = read_version(rel_path, pattern)
        print(f"  {rel_path}: {current}", file=sys.stderr)
        if current != expected:
            mismatches.append(f"{rel_path}={current}")

    if mismatches:
        raise SystemExit(
            f"{canonical_path} at {expected} but other files disagree: {' '.join(mismatches)}"
        )

    print(f"All project files at {expected}", file=sys.stderr)
    print(expected)  # stdout: the version, for capture by callers
    return 0


def main() -> int:
    if sys.argv[1:] == ["--check"]:
        return check()
    print(f"Usage: {sys.argv[0]} --check", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
