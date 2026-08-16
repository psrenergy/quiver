#!/usr/bin/env python3

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
        "bindings/js/package.json",
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

DIGITS_RE = re.compile(r"\d+\.\d+\.\d+")

BUMPS = {
    "major": lambda major, minor, patch: f"{major + 1}.0.0",
    "minor": lambda major, minor, patch: f"{major}.{minor + 1}.0",
    "patch": lambda major, minor, patch: f"{major}.{minor}.{patch + 1}",
}


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


def check() -> str:
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
    return expected


def bump_file(rel_path: str, pattern: str, replacement_tpl: str, new_version: str) -> None:
    path = REPO_ROOT / rel_path
    text = path.read_text(encoding="utf-8")

    old_version = extract_version(rel_path, text, pattern)
    new_text, n = re.compile(pattern).subn(replacement_tpl.format(v=new_version), text, count=1)
    if n != 1:
        raise SystemExit(f"{rel_path}: substitution failed (n={n})")

    path.write_text(new_text, encoding="utf-8")
    print(f"{rel_path}: {old_version} -> {new_version}", file=sys.stderr)


def bump(part: str) -> str:
    """Rewrite every TARGETS file to the next version and return it."""
    old = check()  # refuse to bump from a skewed state
    new = BUMPS[part](*(int(n) for n in old.split(".")))

    for rel_path, pattern, replacement_tpl in TARGETS:
        bump_file(rel_path, pattern, replacement_tpl, new)

    return check()  # the rewrite is its own regression test


def main() -> int:
    """Only this function writes to stdout: callers capture the version and nothing else."""
    args = sys.argv[1:]
    if not args:
        print(check())
        return 0

    if len(args) == 2 and args[0] == "bump" and args[1] in BUMPS:
        print(bump(args[1]))
        return 0

    print(f"Usage: {sys.argv[0]} [bump {'|'.join(BUMPS)}]", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
