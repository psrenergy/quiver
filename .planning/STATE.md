# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-22)

**Core value:** Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.
**Current focus:** v0.4 CSV Export -- Defining requirements

## Current Position

Phase: Not started (defining requirements)
Plan: --
Status: Defining requirements
Last activity: 2026-02-22 -- Milestone v0.4 started

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
All v0.3 decisions archived -- see milestones/v0.3-ROADMAP.md for details.

### Key Technical Context

- Existing export_csv/import_csv are empty stubs in src/database_describe.cpp
- C API wrappers exist in src/c/database.cpp but call empty C++ functions
- Julia/Dart bindings already wrap the stubs (will need signature changes)
- Lua binds export_csv/import_csv in lua_runner.cpp (also wraps empty stubs)
- TransactionGuard nest-aware pattern available for write operations

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-02-22
Stopped at: Defining v0.4 requirements
Resume file: None
