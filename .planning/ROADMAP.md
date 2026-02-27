# Roadmap: Quiver v0.5 Lua CLI

## Overview

Milestone v0.5 delivers a standalone `quiver_lua` executable that opens a Quiver database and runs a Lua script against it. All core capabilities (Database, LuaRunner, error handling, logging) already exist in `libquiver`. The work is a single CLI shell: one new file, one new CMake target, one new dependency (argparse). A single phase is correct because all 14 requirements compose one indivisible deliverable.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [ ] **Phase 1: quiver_lua CLI Executable** - Ship a working command-line tool that runs Lua scripts against Quiver databases

## Phase Details

### Phase 1: quiver_lua CLI Executable
**Goal**: Users can invoke `quiver_lua` from the command line to open a Quiver database and execute Lua scripts against it, with clear error reporting and standard CLI conventions
**Depends on**: Nothing (first phase; all prerequisites exist in libquiver)
**Requirements**: CLI-01, CLI-02, CLI-03, DB-01, DB-02, DB-03, DB-04, DB-05, SCRIPT-01, SCRIPT-02, SCRIPT-03, ERR-01, ERR-02, ERR-03
**Success Criteria** (what must be TRUE):
  1. User can run `quiver_lua database.db script.lua` and see the script's output, or `--help`/`--version` for usage and version information
  2. User can open a database in three modes: existing file (positional arg only), new from schema (`--schema`), or new from migrations (`--migrations`), and receives an error when combining `--schema` and `--migrations`
  3. User can pass `--read-only` to prevent writes and `--log-level` to control verbosity, with sensible defaults (warn level, read-write)
  4. User sees clear error messages on stderr with appropriate exit codes: exit 0 for success, exit 1 for runtime/database errors, exit 2 for usage errors (bad arguments, missing files)
  5. Lua scripts saved with UTF-8 BOM from Windows editors execute without errors
**Plans**: TBD

Plans:
- [ ] 01-01: TBD
- [ ] 01-02: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. quiver_lua CLI Executable | 0/? | Not started | - |
