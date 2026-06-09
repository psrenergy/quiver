#!/usr/bin/env python3
"""Bump (or verify) the project version across every version-bearing file in the repo.

Usage:
    python scripts/bump_version.py 0.7.5    # rewrite every file to the new version
    python scripts/bump_version.py --check   # assert every file agrees, print the version

`--check` treats CMakeLists.txt (the first TARGETS entry) as the source of truth and
fails unless every other file matches it; on success it prints the version to stdout so
callers (e.g. the detect-version action) can capture it. TARGETS is the single source of
truth for which files carry the version and how each is parsed — read and write share it.

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
    canonical_path, canonical_pattern, _ = TARGETS[0]
    expected = read_version(canonical_path, canonical_pattern)
    print(f"{canonical_path} (source of truth): {expected}", file=sys.stderr)

    mismatches = []
    for rel_path, pattern, _ in TARGETS[1:]:
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


def bump_file(rel_path: str, pattern: str, replacement_tpl: str, new_version: str) -> None:
    path = REPO_ROOT / rel_path
    text = path.read_text(encoding="utf-8")

    old_version = extract_version(rel_path, text, pattern)
    if old_version == new_version:
        raise SystemExit(f"{rel_path}: already at {new_version}")

    new_text, n = re.compile(pattern).subn(replacement_tpl.format(v=new_version), text, count=1)
    if n != 1:
        raise SystemExit(f"{rel_path}: substitution failed (n={n})")

    path.write_text(new_text, encoding="utf-8")
    print(f"{rel_path}: {old_version} -> {new_version}")


def main() -> int:
    args = sys.argv[1:]
    if args == ["--check"]:
        return check()

    if len(args) != 1 or args[0].startswith("-"):
        print(f"Usage: {sys.argv[0]} <new_version> | --check", file=sys.stderr)
        return 2

    new_version = args[0]
    if not VERSION_RE.match(new_version):
        print(f"Invalid version {new_version!r}: expected MAJOR.MINOR.PATCH", file=sys.stderr)
        return 2

    for rel_path, pattern, replacement in TARGETS:
        bump_file(rel_path, pattern, replacement, new_version)

    return 0


if __name__ == "__main__":
    sys.exit(main())
