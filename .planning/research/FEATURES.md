# Feature Landscape

**Domain:** CLI executable for running Lua scripts against a Quiver database
**Researched:** 2026-02-27

## Table Stakes

Features users expect from a CLI script runner. Missing = tool feels broken.

| Feature | Why Expected | Complexity | Dependencies | Notes |
|---------|--------------|------------|--------------|-------|
| Positional script path argument | Every script runner takes a script file as primary argument (`lua script.lua`, `python script.py`, `sqlite3 db.db < file.sql`). Users will not read docs for this -- it must be obvious. | Low | argparse library | `quiver_lua script.lua` pattern |
| Database path argument | Tool is useless without connecting to a database. Must be explicit (not inferred from cwd). sqlite3 pattern: `sqlite3 <db>`. | Low | argparse library | Positional or required flag -- see Design Decisions below |
| Schema/migrations path argument | Quiver databases require schema initialization via `from_schema` or `from_migrations`. Without this, the tool cannot open a database for the first time. | Low | Existing `Database::from_schema()`, `Database::from_migrations()` | Two mutually exclusive modes: `--schema <path>` vs `--migrations <path>` |
| Automatic mode detection (schema vs migrations) | User must specify which factory method to use. Mutually exclusive flags are the standard CLI pattern for this. | Low | argparse mutually exclusive groups | `--schema` and `--migrations` are mutually exclusive |
| Error messages to stderr | POSIX convention: stdout for output, stderr for errors. Every serious CLI tool follows this. Lua script output goes to stdout; tool errors go to stderr. | Low | None (std::cerr) | Follows existing Quiver error message patterns |
| Meaningful exit codes | POSIX convention: 0 = success, nonzero = failure. Callers (scripts, CI) depend on this for automation. | Low | None (return from main) | See Exit Code Design below |
| `--help` / `-h` flag | Universal expectation. argparse generates this automatically. | Free | argparse library | Auto-generated from argument definitions |
| `--version` / `-v` flag | Standard for any shipped executable. argparse supports this via constructor parameter. | Free | argparse library, `PROJECT_VERSION` from CMake | `ArgumentParser("quiver_lua", version_string)` |
| Script file validation | Tool must check that the script file exists and is readable before attempting execution. Clear error message if not found. | Low | `<filesystem>` | "Script file not found: path/to/script.lua" following Quiver error pattern |
| Lua runtime error propagation | When a Lua script throws an error, the tool must report it clearly and exit with nonzero code. LuaRunner::run() already throws std::runtime_error. | Low | Existing `LuaRunner::run()` throws on error | Catch exception, print to stderr, exit with code 1 |

## Differentiators

Features that add value beyond the minimum. Not strictly expected, but useful.

| Feature | Value Proposition | Complexity | Dependencies | Notes |
|---------|-------------------|------------|--------------|-------|
| `--log-level` flag | Control spdlog verbosity (debug/info/warn/error/off). Existing `DatabaseOptions.console_level` already supports this. Useful for debugging scripts. | Low | Existing `quiver_log_level_t` enum, argparse | Map string arg to enum: `--log-level debug` |
| `--read-only` flag | Open database in read-only mode. Existing `DatabaseOptions.read_only` supports this. Safety net for analysis scripts. | Low | Existing `DatabaseOptions.read_only` field | `--read-only` boolean flag |
| Open existing database without schema/migrations | Allow opening a database file that already has its schema applied. `Database(path)` constructor exists for this. When neither `--schema` nor `--migrations` is provided, just open directly. | Low | Existing `Database(path, options)` constructor | Most common case for re-running scripts against existing databases |
| Line number in Lua error messages | When a Lua script fails, include the line number in the error output. sol2/Lua already includes line info in exception messages. | Free | Already in LuaRunner error propagation | Just forward the exception message as-is |
| Script path in error context | Prefix error messages with the script filename for clarity in logs. "script.lua: error on line 5: ..." | Low | String formatting | Helpful when running multiple scripts in automation |

## Anti-Features

Features to explicitly NOT build in this milestone.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| REPL / interactive mode | Explicitly out of scope per PROJECT.md. Different UX paradigm (readline, history, prompt). Significant complexity for a future milestone. | Defer to future milestone. Focus on script file execution only. |
| Script arguments (`-- arg1 arg2`) | Explicitly out of scope per PROJECT.md. Requires exposing `arg` table in Lua environment. Low value for v0.5. | Defer. Scripts can hardcode values or read from the database itself. |
| Inline script execution (`-e "code"`) | Lua interpreter supports `-e` but it adds complexity (argument escaping, shell quoting). Script files are the primary use case. | Defer. Users write a `.lua` file instead. |
| Config file support (`.quiverrc`, TOML) | Over-engineering for a script runner. Quiver already uses TOML internally but a CLI config file adds discovery/precedence complexity. | Pass all configuration via CLI flags. |
| Multiple script execution | Running several scripts in sequence (`quiver_lua --schema s.sql db.db a.lua b.lua`). Adds argument parsing complexity. | Run the tool multiple times. Shell scripts handle sequencing. |
| Stdin script reading (`cat script.lua \| quiver_lua`) | Adds complexity for pipe detection. Low value when file path works fine. | Defer. Require explicit file path. |
| New Lua API methods | Explicitly out of scope per PROJECT.md. v0.5 uses existing LuaRunner capabilities as-is. | Defer to future milestone. |
| Subcommands (`quiver_lua run`, `quiver_lua check`) | Premature structure. Tool does one thing (run a script). Subcommands add cognitive overhead for no current benefit. | Single command with flags. Reconsider if future features (REPL, check, format) warrant subcommands. |
| Colorized output | Nice-to-have but adds terminal detection complexity and Windows compatibility concerns. | Plain text to stderr. spdlog already handles its own coloring for log output. |

## Feature Dependencies

```
Script file validation --> Lua script execution (must validate before running)
Database path argument --> Schema/migrations argument (schema only needed for new DBs)
--schema flag --> Database::from_schema() (existing)
--migrations flag --> Database::from_migrations() (existing)
--log-level flag --> DatabaseOptions.console_level (existing)
--read-only flag --> DatabaseOptions.read_only (existing)
Script execution --> LuaRunner(db).run(script) (existing)
argparse library --> All argument parsing features (new dependency)
```

## Exit Code Design

Following POSIX conventions and common CLI patterns:

| Code | Meaning | When |
|------|---------|------|
| 0 | Success | Script executed without error |
| 1 | Runtime error | Lua script error, database error, or unexpected exception |
| 2 | Usage error | Invalid arguments, missing required args, mutually exclusive violation |

Note: argparse throws `std::runtime_error` on parse failure, which can be caught and mapped to exit code 2. Lua/database exceptions map to exit code 1.

## Argument Structure Design

After researching sqlite3, lua interpreter, osqueryi, and litecli patterns, the recommended argument structure is flat flags (no subcommands):

```
quiver_lua [options] <database> <script>

Positional arguments:
  database              Path to the SQLite database file
  script                Path to the Lua script to execute

Optional arguments:
  -h, --help            Show help message and exit
  -v, --version         Show version and exit
  --schema PATH         Schema file for database initialization (mutually exclusive with --migrations)
  --migrations PATH     Migrations directory for database initialization (mutually exclusive with --schema)
  --log-level LEVEL     Log verbosity: debug, info, warn, error, off (default: info)
  --read-only           Open database in read-only mode
```

**Rationale for two positional arguments:** sqlite3 uses `sqlite3 DATABASE [SQL]`. The lua interpreter uses `lua [options] [script]`. Combining both patterns: database is the primary resource, script is the action. Both are always required, so positional is cleaner than `--database` and `--script` flags.

**Rationale against subcommands:** The tool does exactly one thing -- run a Lua script against a database. `quiver_lua run ...` would add a mandatory word with zero disambiguation value. If REPL mode is added later, then `quiver_lua run` vs `quiver_lua repl` could be introduced, but that is a future concern.

## MVP Recommendation

Prioritize (in implementation order):

1. **Argument parsing setup** -- argparse integration, positional args (database, script), `--help`, `--version`
2. **Database opening** -- `--schema`/`--migrations` mutually exclusive flags, open-existing fallback
3. **Script loading and execution** -- read file, pass to `LuaRunner::run()`
4. **Error handling and exit codes** -- catch exceptions, stderr output, exit code mapping
5. **Optional flags** -- `--log-level`, `--read-only`

Defer to future milestones:
- **REPL mode**: Requires readline/linenoise, prompt loop, different UX
- **Script arguments**: Requires Lua `arg` table population
- **Inline execution (`-e`)**: Requires shell quoting handling

## Sources

- [p-ranav/argparse - Argument Parser for Modern C++](https://github.com/p-ranav/argparse) -- Header-only, MIT, C++17, supports mutually exclusive groups
- [SQLite CLI Documentation](https://sqlite.org/cli.html) -- `sqlite3 [OPTIONS] FILENAME [SQL]` pattern
- [Lua Standalone Interpreter](https://www.lua.org/manual/5.4/lua.html) -- `lua [options] [script [args]]` pattern
- [POSIX Exit Status Conventions](https://en.wikipedia.org/wiki/Exit_status) -- 0 success, 1 error, 2 usage error
- [CLI Best Practices](https://eng.localytics.com/exploring-cli-best-practices/) -- stderr for errors, explicit file paths over working directory inference
