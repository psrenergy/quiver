"""Generate Python CFFI bindings from C headers."""

import re
from pathlib import Path

HEADER_DIR = Path(__file__).parent.parent.parent.parent / "include" / "psr" / "c"
OUTPUT_FILE = Path(__file__).parent.parent / "src" / "psr_database" / "_bindings.py"

HEADERS = ["common.h", "database.h", "element.h", "lua_runner.h"]


def parse_header(path: Path) -> str:
    """Extract CFFI-compatible declarations from a C header."""
    content = path.read_text()

    # Remove single-line comments
    content = re.sub(r'//.*$', '', content, flags=re.MULTILINE)

    # Remove multi-line comments
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

    # Remove preprocessor directives
    content = re.sub(r'^\s*#.*$', '', content, flags=re.MULTILINE)

    # Remove extern "C" block (opening and closing)
    content = re.sub(r'extern\s+"C"\s*\{', '', content)

    # Remove PSR_C_API macro
    content = content.replace('PSR_C_API ', '')

    # Remove duplicate empty lines
    content = re.sub(r'\n{3,}', '\n\n', content)

    # Remove lone closing braces (from extern "C")
    content = re.sub(r'^\s*\}\s*$', '', content, flags=re.MULTILINE)

    # Clean up whitespace again
    content = re.sub(r'\n{3,}', '\n\n', content)

    return content.strip()


def generate():
    """Generate _bindings.py from C headers."""
    declarations = []

    for header_name in HEADERS:
        header_path = HEADER_DIR / header_name
        if header_path.exists():
            declarations.append(f"// From {header_name}")
            declarations.append(parse_header(header_path))

    cdef_content = "\n\n".join(declarations)

    output = f'''"""CFFI declarations - GENERATED FILE, DO NOT EDIT.

Regenerate with: generator/generator.bat
"""

CDEF = """
{cdef_content}
"""
'''

    OUTPUT_FILE.write_text(output)
    print(f"Generated {OUTPUT_FILE}")


if __name__ == "__main__":
    generate()
