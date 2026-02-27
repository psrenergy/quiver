# Phase 1: quiver_lua CLI Executable - Context

**Gathered:** 2026-02-27
**Status:** Ready for planning

<domain>
## Phase Boundary

Ship a standalone `quiver_lua` executable that opens a Quiver database and runs a Lua script against it. The CLI is a thin pass-through layer — all intelligence resides in the C++ layer (libquiver, LuaRunner). One new source file, one new CMake target, one new dependency (argparse).

</domain>

<decisions>
## Implementation Decisions

### Help & version output
- `--help` shows flags only: no program description, no usage examples
- `--version` prints "quiver_lua X.Y.Z" (name + version)
- Version number sourced from CMake variable (single source of truth, injected at build time)

### Error presentation
- No prefix on error messages (no "error: " prefix) — exit code signals the error
- Lua runtime errors show message only, no stack trace
- Usage errors (exit 2) show just the error message, not the full help text
- File-not-found errors use the user's input path, not resolved absolute path
- Database errors pass through from C++/SQLite layer as-is
- Conflicting flags (--schema + --migrations) use simple statement: "Cannot use --schema and --migrations together"
- spdlog output goes to stderr, keeping stdout clean for script output

### Migrations input
- `--migrations` accepts a directory path
- CLI collects all files in the directory (any extension, not just .sql)
- File paths passed directly to `from_migrations()` — no ordering, no validation, no content reading in CLI
- Top-level files only (no recursive subdirectory discovery)
- Empty directory: pass empty list to C++, let it handle errors

### Schema input
- `--schema` accepts a file path, passed directly to `from_schema()` — no file existence check in CLI

### Database modes
- Three modes: positional-only (open existing), `--schema` (from schema), `--migrations` (from migrations)
- One mode must be specified — positional arg selects "open existing" mode
- `--schema` and `--migrations` are mutually exclusive

### Script output behavior
- Lua `print()` goes to stdout
- Silent exit (code 0) on success with no output
- Default log level: warn (as specified in requirements)
- Script file path is optional in argparse (errors with "no script provided" for now — prepares for future REPL mode)

### Claude's Discretion
- argparse configuration details
- CMake target setup and linking
- UTF-8 BOM stripping implementation
- Exact error message wording (within the patterns above)
- Source file location and structure

</decisions>

<specifics>
## Specific Ideas

- CLI is explicitly a "thin layer" — no intelligence, no validation beyond argument parsing. Pass everything to C++ and surface its errors.
- User emphasized multiple times: don't add CLI-level logic for things C++ already handles (file existence, migration ordering, empty inputs).

</specifics>

<deferred>
## Deferred Ideas

- REPL mode when no script is passed (REPL-01 already in REQUIREMENTS.md as future requirement) — script path made optional to prepare for this

</deferred>

---

*Phase: 01-quiver-lua-cli-executable*
*Context gathered: 2026-02-27*
