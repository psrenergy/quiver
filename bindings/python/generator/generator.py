"""CFFI declaration generator for the Quiver C API.

Reads C API headers and produces CFFI-compatible declarations by stripping
preprocessor directives and QUIVER_C_API macros.

Usage:
    python generator/generator.py                              # Print to stdout
    python generator/generator.py --output src/quiverdb/_declarations.py  # Write file
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Headers to process, in dependency order
HEADERS = [
    "include/quiver/c/common.h",
    "include/quiver/c/options.h",
    "include/quiver/c/database.h",
    "include/quiver/c/element.h",
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


def replace_bool_types(declarations: str) -> str:
    """Replace bool/bool* with _Bool/_Bool* for CFFI compatibility.

    CFFI's cdef parser recognizes _Bool as C99's boolean type.
    The C API uses bool* for out-parameters (e.g., quiver_database_in_transaction).
    """
    # Replace bool* but not _Bool*
    declarations = re.sub(r"\bbool\b", "_Bool", declarations)
    return declarations


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

    declarations = "\n".join(sections)
    declarations = replace_bool_types(declarations)
    return declarations


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate CFFI declarations from C API headers")
    parser.add_argument(
        "--output",
        type=str,
        default=None,
        help="Output file path (writes DECLARATIONS constant). If omitted, prints to stdout.",
    )
    args = parser.parse_args()

    root = find_project_root()
    header_paths = [root / h for h in HEADERS]

    for hp in header_paths:
        if not hp.is_file():
            print(f"Error: Header not found: {hp}", file=sys.stderr)
            sys.exit(1)

    declarations = generate(header_paths)

    if args.output:
        output_path = Path(args.output)
        if not output_path.is_absolute():
            # Resolve relative to the bindings/python directory
            output_path = root / "bindings" / "python" / args.output
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(
            f'"""Auto-generated CFFI declarations. Do not edit manually.\n\n'
            f"Regenerate with: python generator/generator.py --output {args.output}\n"
            f'"""\n\n'
            f'DECLARATIONS = """\n{declarations}\n"""\n',
            encoding="utf-8",
        )
        print(f"Written to {output_path}")
    else:
        print(declarations)


if __name__ == "__main__":
    main()
