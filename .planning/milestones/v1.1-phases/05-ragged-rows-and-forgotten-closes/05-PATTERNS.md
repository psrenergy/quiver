# Phase 5: Ragged rows and forgotten closes - Pattern Map

**Mapped:** 2026-09-17
**Files analyzed:** 6
**Analogs found:** 6 / 6 (all are self-analogs — every file already exists and is being *modified*
in place; this phase adds no new file, so the "closest analog" for each file is its own existing
code, immediately adjacent to the edit point)

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|-----------------|---------------|
| `src/lua_runner.cpp` (`CsvWriter` struct, `write_row` lambda) | binding/adapter (Lua↔C++) | file-I/O, request-response | same file's existing FMT-05/FMT-08 checks in the same lambda (`src/lua_runner.cpp:705-720`) | exact |
| `src/lua_runner.cpp` (`LuaRunner::run`) | controller/entry-point | request-response | same function, pre-change (`src/lua_runner.cpp:2165-2179`) | exact |
| `tests/test_lua_runner_write_csv.cpp` (TEST-10, TEST-11) | test | CRUD/round-trip | same file's existing round-trip tests (`WriteRowThenReadCsvRoundTripsPlainStrings`, `IntegerCellRoundTripsExactDigitString`) | exact for TEST-10; **no analog for TEST-11's two-`run()`-call shape** (see below) |
| `bindings/js/src/lua-api.ts` (`LUA_DB_API_REFERENCE`, CSV file writing section) | documentation/config | N/A | same section's existing truncate-at-open paragraph (`bindings/js/src/lua-api.ts:724-728`) | exact |
| `src/CLAUDE.md` (csv_write paragraph + LuaRunner bullet list) | documentation/config | N/A | same file's existing D-34/D-37/D-40 paragraph and `run` bullet list | exact |
| root `CLAUDE.md` (design decisions + cross-layer table) | documentation/config | N/A | same file's existing `db:read_csv` design-decision bullet and "CSV file read" table row | exact |
| `CHANGELOG.md` | documentation/config | N/A | same file's own `## [0.10.7] — unreleased` `### Added` bullet | exact |
| `src/csv_write.cpp` (TEST-12 message-catalogue comment) | comment/catalogue | N/A | same comment block, `write_row`-operation section | exact — confirmed no functional code in this file changes (see note below) |

## Pattern Assignments

### `src/lua_runner.cpp` — `CsvWriter` struct + `write_row` lambda (FMT-07)

**Analog:** itself — the existing `next_row_index` member and the FMT-08 max-integer-key walk in
the same lambda.

**Current struct** (lines 254-264):
```cpp
struct CsvWriter {
    quiver::csv_write::Writer writer;
    std::int64_t next_row_index = 1;

    explicit CsvWriter(quiver::csv_write::Writer w) : writer(std::move(w)) {}
};
```
Add one member, `std::size_t header_width = 0;` (0 = no header decoded = no width check —
D-4-discretion, matches `header={}` already meaning "no header row"), and thread it through the
constructor from `csv_options.header.size()` at the factory call site (lines 690-698):
```cpp
[](Database& self, const std::string& path, sol::object options) -> std::unique_ptr<CsvWriter> {
    const auto resolved = resolve_sandboxed_path(self, "write_csv", path);
    auto csv_options = write_csv_options_from_lua(options, "write_csv");
    return std::make_unique<CsvWriter>(
        quiver::csv_write::Writer(resolved, path, "write_csv", csv_options),
        csv_options.header.size());
});
```

**Core pattern — existing `write_row` lambda body** (lines 705-720), showing exactly where to
splice the pad/throw check (between `csv_row_cells_from_lua` producing `cells` and the call to
`self.writer.write_row`):
```cpp
"write_row",
[](CsvWriter& self, const sol::table& row) {
    if (self.writer.is_closed()) {
        self.writer.write_row({}, "write_row");
        return;
    }
    const auto row_index = self.next_row_index;
    self.writer.write_row(csv_row_cells_from_lua(row, "write_row", row_index), "write_row");
    ++self.next_row_index;
},
```
Insert the pad/throw block right after `csv_row_cells_from_lua(...)` returns and before
`self.writer.write_row(...)` is called — this ordering is load-bearing (Pitfall 3 in RESEARCH.md):
padding must happen before `Writer::write_row`/`append_record` ever sees the vector.

**Error handling pattern to copy (Pattern 1, root CLAUDE.md):** mirror the FMT-05 non-finite-number
throw already in `csv_row_cells_from_lua` (same file, a few lines above — not re-read here since
its shape is already documented in RESEARCH.md Q2/Q3): `"Cannot {operation}: {reason}"`, reusing
`row_index` for the ordinal, e.g.
`"Cannot write_row: row " + std::to_string(row_index) + " has " + std::to_string(cells.size()) +
" cells but header declares " + std::to_string(self.header_width)`.

**Do NOT touch:** `csv_write::Writer`'s header (`src/csv_write.h:59-70`) or `.cpp` — zero interface
change there (Q2's Option B, locked decision 3).

---

### `src/lua_runner.cpp` — `LuaRunner::run` (WRITE-06)

**Analog:** itself, pre-change (lines 2165-2179).

**Current shape:**
```cpp
std::string LuaRunner::run(const std::string& script) {
    auto result = impl_->lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Failed to run Lua script: ") + err.what());
    }
    if (result.return_count() == 0) {
        return {};
    }
    std::string out;
    append_json(result.get<sol::object>(0), out, 0);
    return out;
}
```

**Pattern to add (RAII scope guard, locked decision 1):** declare a small local struct whose
destructor calls `impl_->lua.collect_garbage()`, constructed before `safe_script` so it covers the
throw path, the empty-return path, and the JSON-encode-and-return path uniformly — one guard, not
three duplicated calls:
```cpp
std::string LuaRunner::run(const std::string& script) {
    struct GcGuard {
        sol::state& lua;
        ~GcGuard() { lua.collect_garbage(); }
    } gc_guard{impl_->lua};

    auto result = impl_->lua.safe_script(script, sol::script_pass_on_error);
    // ... unchanged body below ...
}
```
No existing analog for a scope-guard destructor pattern elsewhere in this file — this is new
machinery, but minimal (one struct, one member, one-line destructor), matching the project's
existing RAII conventions (`TransactionGuard` in `database_impl.h`, described in `src/CLAUDE.md`
"Transactions" section, is the same shape: constructor-acquires/destructor-releases, no-op-aware).

---

### `tests/test_lua_runner_write_csv.cpp` — TEST-10 (padding/throw round trip)

**Analog:** `WriteRowThenReadCsvRoundTripsPlainStrings` (same file, ~line 103-122) and
`IntegerCellRoundTripsExactDigitString` (~line 130+) — both already establish the fixture
(`LuaSandboxTest`, `VALID_SCHEMA("basic.sql")`, `quiver::LuaRunner lua(db);`, one `lua.run(...)`
doing write+close+read-and-assert in a single script).

**Fixture pattern to copy:**
```cpp
class LuaRunner_WriteCsv : public LuaSandboxTest {};

TEST_F(LuaRunner_WriteCsv, ShortRowPadsWithEmptyCells) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    const auto path = lp((sandbox / "out.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" + path + R"(", { header = { "a", "b", "c" } })
        w:write_row({ "x", "y" })   -- short: 2 cells against a 3-name header
        w:close()

        local csv = db:read_csv(")" + path + R"(")
        assert(csv.rows[1][3] == "", "expected padded empty cell, got " .. tostring(csv.rows[1][3]))
    )");
}
```
The too-long-row throw case should follow the existing error-assertion helper already present at
the top of this file (the `expected_prefix`/`reason_substring` checker visible immediately above
`namespace {}` close, lines ~90-95) — read that helper directly when writing the throw-side
assertion rather than re-deriving the pattern.

---

### `tests/test_lua_runner_write_csv.cpp` — TEST-11 (unclosed-writer flush)

**No analog exists in this suite or any other test file in the repo for the two-separate-`.run()`-
calls-on-one-live-`LuaRunner` shape.** RESEARCH.md Q4 confirms this explicitly after spot-checking
the suite: every existing test does write + `close()` + read-back inside **one** `lua.run(...)`
call. State that plainly rather than inventing a false analog. The nearest *partial* precedent is
structural only — the fixture setup (`LuaSandboxTest`, `db_path()`, `sandbox`, `lp(...)`) is
identical to every other test in the file — but the two-call sequencing itself is new:
```cpp
TEST_F(LuaRunner_WriteCsv, UnclosedWriterIsFlushedWhenRunReturns) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    const auto path = lp((sandbox / "unclosed.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" + path + R"(", { header = { "a" } })
        w:write_row({ "x" })
        -- deliberately no w:close()
    )");

    lua.run(R"(
        local csv = db:read_csv(")" + path + R"(")
        assert(#csv.rows == 1, "expected flushed row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "x")
    )");
}
```
Per Q4's RED-first caveat, keep the fixture tiny (one column, one short row) and confirm this is
genuinely RED (empty/truncated file) against unmodified Phase-4 code before adding the `GcGuard` fix.

---

### `bindings/js/src/lua-api.ts` — `LUA_DB_API_REFERENCE` CSV writing section (DOC-06, locked decision 2)

**Analog:** the file's own existing truncate-at-open paragraph in the same "CSV file writing"
section (lines 724-728):
```
Opening `db:write_csv` **truncates** an existing file at the target path — there is no overwrite
guard, so a script can destroy an existing file in the case folder (including the database file
itself) by writing to its path. This is documented behaviour, not a bug: reopening the same path
always starts a fresh file.

`write_row` after `close` throws; `close` is idempotent (a second call is a no-op, not an error).
```
Add two paragraphs of the same declarative, script-author-facing voice, right after (or folded
into) the existing header-option sentence ("its only two keys are `separator` ... and `header`
..."):
1. State the pad-short/throw-long rule: with a `header`, its length is the row width — a shorter
   `write_row` call pads with empty cells, a longer one throws.
2. State the flush-without-close guarantee: a writer never explicitly `close()`d is still flushed
   when the script's `run()` call returns, so the file is complete and re-readable even without an
   explicit `close()` — no warning is emitted.

**Do NOT touch:** `lua-api-sync.test.ts`'s parser only checks that bound `db:`/`quiver.*` **names**
match `lua_runner.cpp` — no new names are introduced here, so the build gate needs no change.

---

### `src/CLAUDE.md` — csv_write paragraph + LuaRunner bullet list (DOC-06)

**Analog:** the file's own existing `csv_write.h`/`csv_write.cpp` paragraph (D-34/D-37/D-40) and
the `## LuaRunner` bullet list's existing `run` bullet describing the two encoder caps.

Add one or two sentences to the `csv_write` paragraph naming where FMT-07's width enforcement lives
(the Lua-layer `CsvWriter`, not `Writer` — per Q2/locked decision 3), and one bullet near the
existing `run` bullet describing the `collect_garbage()`-at-end-of-`run()` guarantee (WRITE-06),
in the same "verified read this session, cite exact behavior" voice the rest of the file uses.

---

### root `CLAUDE.md` — design decisions + cross-layer table (DOC-06)

**Analog:** the file's own existing `db:read_csv` design-decision bullet (ends "...writing
(`db:write_csv`) is not exposed; a script's parsed rows go through the existing group writers.")
— this sentence is now stale (Phase 4 shipped `db:write_csv`) and must be corrected, then extended
with FMT-07/WRITE-06's decisions. Also add a "CSV file write" row to the cross-layer table next to
the existing "CSV file read" row (`db:read_csv()` / `db:read_csv_stream()`), following that row's
exact column shape (Category / C++ / C API / Julia / Dart / Lua, with N/A in every non-Lua column
since this feature is Lua-only).

---

### `CHANGELOG.md` (DOC-06)

**Analog:** the file's own `## [0.10.7] — unreleased` section, `### Added` bullet — currently reads
"...reading is the only direction, `db:write_csv` is not exposed", also stale. Correct it, add a
new `### Added` bullet for Phase 4's writer itself (per Q6: streaming-only, two options,
truncate-at-open, hand-rolled/no-new-dependency, `to_chars` formatting, `nil`/`""`
indistinguishability) since apparently no bullet for it exists yet, and one more for Phase 5's own
additions (header-as-width-authority pad/throw, unclosed-writer flush). **Do not touch version
numbers** — the `0.10.7`/`0.10.6` mismatch is a carried, already-accepted decision (see
RESEARCH.md's version-mismatch note); this phase edits content only, following the existing bullet
style already in the file (short present-tense clauses, no wrapping in code fences except for
identifiers).

---

### `src/csv_write.cpp` — TEST-12 message-catalogue comment (confirmed scope)

**Analog:** the same comment block, `write_row`-operation section (already read in full above):
```cpp
// Raised here, in Writer::write_row (operation is always "write_row"):
//   "Cannot write_row: writer for '<original_path>' is already closed"                        (WRITE-05)
//   "Cannot write_row: failed to write to file '<original_path>'"          (data record write failure)
```
followed a few lines down by the `lua_runner.cpp`-raised section, already containing the FMT-05/
FMT-08 lines:
```cpp
// Raised in src/lua_runner.cpp's cell formatter and row/option decoders (operation is always the
// Lua method that received the bad value...):
//   "Cannot write_row: row <N> cell #<M> is not a finite number"                               (FMT-05)
//   "Cannot write_row: cell #<M> has unsupported Lua type"                    (table/function/userdata)
//   "Cannot write_row: row key must be a positive integer"
```
**Confirmed: only this comment block changes in `src/csv_write.cpp`** — add one new line to the
`lua_runner.cpp`-raised section for FMT-07's too-long-row message (exact wording per Open Question
1 / A1, planner/executor's to pin), e.g.:
```
//   "Cannot write_row: row <N> has <M> cells but header declares <W> cells"                     (FMT-07)
```
No functional code in this file changes — Q2/locked decision 3 keeps `Writer::write_row`'s
signature and body untouched; this file's only edit is the catalogue comment line, kept in sync
with the actual new throw site in `lua_runner.cpp` per this file's own stated rule ("do NOT reword
any of these without updating tests/test_lua_runner_write_csv.cpp in the same change").

## Shared Patterns

### Pattern 1 error messages (root CLAUDE.md)
**Source:** existing FMT-05 throw in `csv_row_cells_from_lua` (`src/lua_runner.cpp`, same file as
the new FMT-07 check)
**Apply to:** the new too-long-row throw in the `write_row` lambda
```cpp
throw std::runtime_error("Cannot " + operation + ": " + reason);
```
Shape: `"Cannot {operation}: {reason}"`, operation is always the literal string `"write_row"` here
(D-36, already threaded throughout this feature).

### RAII scope guard (root CLAUDE.md "Transactions" section pattern, applied to GC)
**Source:** `Impl::TransactionGuard` (`database_impl.h`, described in `src/CLAUDE.md`)
**Apply to:** the new `GcGuard` in `LuaRunner::run` — constructor-acquires-nothing/
destructor-releases shape, same spirit (deterministic cleanup on every exit path via C++ stack
unwinding), though `GcGuard` is simpler (no nesting/no-op awareness needed — `collect_garbage()` is
always safe to call unconditionally, per Q1's probe).

### Documentation edit voice (all four .md files)
**Source:** existing paragraphs in each target file (see per-file sections above)
**Apply to:** every DOC-06 edit — match the declarative, present-tense, already-verified-by-reading
voice each file already uses; do not introduce a new documentation style for this phase's two small
additions.

## No Analog Found

| File/Test | Role | Data Flow | Reason |
|------|------|-----------|--------|
| TEST-11 (two-`run()`-calls-on-one-`LuaRunner` shape) | test | CRUD/round-trip across two calls | RESEARCH.md Q4 explicitly confirms no existing test in the suite calls `.run()` twice on the same live `LuaRunner`; write it from the fixture conventions of neighboring tests but invent the sequencing fresh, and verify RED-before-fix per Q4's caveat |
| `GcGuard` RAII destructor calling `collect_garbage()` | resource-cleanup guard | event-driven (GC finalizer trigger) | No prior code in this repo drives Lua GC directly from a scope guard; nearest structural precedent (`TransactionGuard`) is a different resource (SQL transactions), cited above as a spirit-only analog, not a copy-source |

## Metadata

**Analog search scope:** `src/lua_runner.cpp`, `src/csv_write.h`/`.cpp`, `tests/test_lua_runner_write_csv.cpp`,
`tests/test_lua_runner.h`, `bindings/js/src/lua-api.ts`, `src/CLAUDE.md`, root `CLAUDE.md`,
`CHANGELOG.md` — all read directly this session (no Glob/Grep-based broad search was needed; every
file to modify was already named exactly, with line numbers, by RESEARCH.md).
**Files scanned:** 8 (all targets; no additional candidate analogs needed — every file's own
existing adjacent code is the strongest available match, consistent with "prefer files the change
already touches over unrelated codebase-wide search" for a phase this narrow).
**Pattern extraction date:** 2026-09-17
