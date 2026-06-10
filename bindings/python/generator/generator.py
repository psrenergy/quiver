"""CFFI declaration generator for the Quiver C API.

Reads C API headers and prints CFFI-compatible declarations to stdout by
stripping preprocessor directives and QUIVER_C_API macros. Used as a diff
aid when hand-updating src/quiverdb/_c_api.py.

Usage:
    python generator/generator.py
"""

from __future__ import annotations

import sys
from pathlib import Path

# Headers to process, in dependency order
HEADERS = [
    "include/quiver/c/common.h",
    "include/quiver/c/options.h",
    "include/quiver/c/database.h",
    "include/quiver/c/element.h",
    "include/quiver/c/lua_runner.h",
]


def find_project_root() -> Path:
    """Walk up from this file until finding CMakeLists.txt."""
    current = Path(__file__).resolve().parent
    while current != current.parent:
        if (current / "CMakeLists.txt").is_file():
            return current
        current = current.parent
    raise FileNotFoundError("Could not find project root (no CMakeLists.txt found)")


def strip_header(content: str) -> str:
    """Strip preprocessor directives and QUIVER_C_API from a header file."""
    lines: list[str] = []

    for line in content.splitlines():
        stripped = line.strip()

        # Skip preprocessor directives
        if stripped.startswith("#"):
            continue

        # Skip extern "C" braces
        if stripped in ('extern "C" {', "}"):
            continue

        # Skip empty lines at the start
        if not lines and not stripped:
            continue

        # Strip QUIVER_C_API from function declarations
        line = line.replace("QUIVER_C_API ", "")

        lines.append(line)

    # Remove trailing blank lines
    while lines and not lines[-1].strip():
        lines.pop()

    return "\n".join(lines)


def generate(headers: list[Path]) -> str:
    """Parse headers and produce combined CFFI declarations."""
    sections: list[str] = []

    for header_path in headers:
        content = header_path.read_text(encoding="utf-8")
        stripped = strip_header(content)
        if stripped.strip():
            # Add header name as comment
            relative = header_path.name
            sections.append(f"// {relative}")
            sections.append(stripped)
            sections.append("")

    return "\n".join(sections)


def main() -> None:
    root = find_project_root()
    header_paths = [root / h for h in HEADERS]

    for hp in header_paths:
        if not hp.is_file():
            print(f"Error: Header not found: {hp}", file=sys.stderr)
            sys.exit(1)

    print(generate(header_paths))


if __name__ == "__main__":
    main()
