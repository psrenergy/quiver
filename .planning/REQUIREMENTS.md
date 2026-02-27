# Requirements: Quiver

**Defined:** 2026-02-27
**Core Value:** A single, consistent API surface for reading and writing structured data in SQLite — from any language, with zero boilerplate.

## v0.5 Requirements

Requirements for the Lua CLI executable. Each maps to roadmap phases.

### CLI

- [ ] **CLI-01**: User can run `quiver_lua --help` to see usage information
- [ ] **CLI-02**: User can run `quiver_lua --version` to see the version number
- [ ] **CLI-03**: User receives a usage error (exit code 2) when providing invalid arguments

### Database

- [ ] **DB-01**: User can open an existing database by providing the database path as a positional argument
- [ ] **DB-02**: User can create/open a database with a schema file via `--schema PATH`
- [ ] **DB-03**: User can create/open a database with migrations via `--migrations PATH`
- [ ] **DB-04**: User receives an error when specifying both `--schema` and `--migrations`
- [ ] **DB-05**: User can open a database in read-only mode via `--read-only`

### Script

- [ ] **SCRIPT-01**: User can execute a Lua script by providing the script path as a positional argument
- [ ] **SCRIPT-02**: User receives a clear error when the script file does not exist
- [ ] **SCRIPT-03**: Lua scripts with UTF-8 BOM are handled correctly on Windows

### Error Handling

- [ ] **ERR-01**: User sees Lua runtime errors on stderr with exit code 1
- [ ] **ERR-02**: User sees database errors on stderr with exit code 1
- [ ] **ERR-03**: User can control log verbosity via `--log-level` (debug/info/warn/error/off)

## Future Requirements

Deferred to future milestones. Tracked but not in current roadmap.

### Interactive

- **REPL-01**: User can run Lua interactively against a database (REPL mode)

### Script Arguments

- **ARG-01**: User can pass extra arguments to the Lua script via `-- arg1 arg2`

### Inline Execution

- **INLINE-01**: User can execute inline Lua code via `-e "code"`

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| REPL / interactive mode | Different UX paradigm (readline, history, prompt). Significant complexity. |
| Script arguments (`-- arg1 arg2`) | Requires Lua `arg` table population. Low value for v0.5. |
| Inline execution (`-e "code"`) | Shell quoting complexity. Script files are primary use case. |
| Config file (`.quiverrc`, TOML) | Over-engineering for a script runner. Use CLI flags instead. |
| Multiple script execution | Use shell scripting for sequencing. |
| Stdin script reading (pipe) | Low value when file path works. |
| Subcommands | Tool does one thing. Premature structure. |
| Colorized output | Windows compatibility concerns. spdlog handles its own coloring. |
| New Lua API methods | v0.5 uses existing LuaRunner capabilities as-is. |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CLI-01 | — | Pending |
| CLI-02 | — | Pending |
| CLI-03 | — | Pending |
| DB-01 | — | Pending |
| DB-02 | — | Pending |
| DB-03 | — | Pending |
| DB-04 | — | Pending |
| DB-05 | — | Pending |
| SCRIPT-01 | — | Pending |
| SCRIPT-02 | — | Pending |
| SCRIPT-03 | — | Pending |
| ERR-01 | — | Pending |
| ERR-02 | — | Pending |
| ERR-03 | — | Pending |

**Coverage:**
- v0.5 requirements: 14 total
- Mapped to phases: 0
- Unmapped: 14

---
*Requirements defined: 2026-02-27*
*Last updated: 2026-02-27 after initial definition*
