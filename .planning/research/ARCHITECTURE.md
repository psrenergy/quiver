# Architecture Research: `db:write_csv` streaming CSV writer

**Domain:** C++20 SQLite wrapper, embedded Lua runner (sol2), Lua-only feature (no C API, no FFI)
**Researched:** 2026-09-16
**Confidence:** HIGH (every claim below is grounded in a specific file/line in this repo, or in
the Lua 5.4 reference manual's documented `lua_close`/GC behavior — flagged inline where the
underlying decision is still open per `.planning/PROJECT.md`)

## Answer to Q1 — Component boundary

**Recommendation: `src/csv_write.h` / `src/csv_write.cpp`, mirroring `src/csv_read.h/.cpp` exactly**
(same Pimpl shape, same "no public header, no `QUIVER_API`, no C API, no FFI" comment block) —
but the two possible STACK outcomes make the *reason* for the split different. Give the plan
author both, since which one applies is not yet decided:

### Outcome A — STACK concludes "hand-roll the writer" (the likely outcome)

`.planning/PROJECT.md`'s own "What the writer cannot reuse" section already rules out
`csv-parser`'s `DelimWriter<OutputStream, Delim, Quote>` for **functional** reasons — compile-time
delimiter/quote template parameters can't take a runtime `separator`, and its `to_string` truncates
floats at 5 decimals — not header-weight reasons. A hand-rolled RFC-4180 writer needs only
`<string>`, `<vector>`, `<fstream>` or `<cstdio>`, and `<charconv>` (already `#include`d in
`lua_runner.cpp` at line 17 for the JSON encoder's `append_number`). **The original Pimpl rationale
for `csv_read::Reader` — keep csv-parser's heavy template headers out of the sol2 translation
unit — does not apply here**, because nothing heavy is being pulled in.

The split into `csv_write.h/.cpp` is still recommended, but as a **stylistic, not load-bearing**,
choice:
- Symmetry with `csv_read.h/.cpp` in the `src/` file map (`src/CLAUDE.md`'s File Map table already
  lists `csv_read.h / csv_read.cpp` as a matched pair; `csv_write.h / csv_write.cpp` slots in next
  to it with an identical one-line description).
- Keeps `lua_runner.cpp` (2000 lines, sol2 template-heavy, already needs `/bigobj` on MSVC) from
  growing by another self-contained ~150-line unit.
- Gives the writer a single-purpose type (`quiver::csv_write::Writer`) that `lua_runner.cpp` can
  unit-drive without sol2 in scope — useful if a future phase ever needs to smoke-test the quoting
  logic outside Lua (none is currently planned; `csv_read::Reader` sets the precedent of having
  *zero* direct C++ core tests, all coverage living in `test_lua_runner_read_csv.cpp` — see Q6).

If this outcome is chosen, it is equally valid (not wrong, just a different tradeoff) to inline the
writer as a private nested class or free functions directly in `lua_runner.cpp`'s anonymous
namespace, next to `is_lua_boolean`/`lua_cell_as`. Recommend the split anyway for the symmetry and
size reasons above, but flag it to the plan author as the one open call in this document that is
genuinely a coin flip.

### Outcome B — STACK concludes some part of csv-parser (or another header-heavy dependency) is reused

If the writer needs `internal/csv_writer.hpp` (or any other heavy templated header) for any reason
— e.g. reusing csv-parser's internal buffering machinery while discarding just `DelimWriter`, or a
different vendored library entirely — **the split is mandatory, not stylistic**, for the identical
reason `csv_read::Reader` is Pimpl'd: `lua_runner.cpp` must never `#include` that header, full stop
(`src/CLAUDE.md`: *"`Reader` is Pimpl'd specifically so csv-parser's headers never have to be
included by `lua_runner.cpp`, which already needs `/bigobj` on MSVC for sol2's template depth."*).
In this outcome `csv_write::Writer` gets the exact same shape as `csv_read::Reader`: a
`struct Impl` holding the heavy library object, `unique_ptr<Impl> impl_`, move-only, public methods
that touch only STL types.

### Either outcome — the public surface is identical

```cpp
// src/csv_write.h
namespace quiver::csv_write {

struct Options {
    char separator = ',';
};

// Wraps one output file. resolved_path/original_path/operation follow the same convention as
// csv_read::Reader's constructor (D-22-style ordering, Pattern 1 messages quoting the caller's
// own path spelling). header is empty when the caller passed none.
class Writer {
public:
    Writer(std::string resolved_path, std::string original_path, std::string operation,
           std::vector<std::string> header, Options options = {});
    ~Writer();  // best-effort flush+close, no throw, no log — see Q2.

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) noexcept;
    Writer& operator=(Writer&&) noexcept;

    // Writes one row. `cells` are pre-formatted, unescaped text (lua_runner.cpp has already done
    // the sol2 type dispatch and number formatting — see Q4); Writer's only job is RFC-4180
    // quoting/escaping and the strict-width check against the stored header/first-row width.
    // Throws Pattern 1 naming `operation`, the row's 1-based ordinal, and both counts on mismatch.
    void write_row(const std::vector<std::string>& cells);

    void close();      // idempotent — mirrors BinaryFile::close() (see Q2)
    bool is_open() const;
    const std::string& path() const;  // original_path, for the "unclosed writer" warning message
};

}  // namespace quiver::csv_write
```

The header/cell-formatting responsibility split (Writer receives already-formatted strings, never
sol2 types) is what lets `csv_write.h` have zero sol2 dependency in *either* outcome — it mirrors
`csv_read::Reader::for_each_row`'s `RowSink`, which hands `lua_runner.cpp` raw
`std::vector<std::string>` cells and lets the Lua layer build the `sol::table`. Same division of
labor, opposite direction.

## Answer to Q2 — Handle lifetime (the hard part)

### What C++ type does the sol2 usertype wrap, and who owns it

**`std::shared_ptr<csv_write::Writer>`**, bound via `lua.new_usertype<csv_write::Writer>(...)` —
**not** `std::unique_ptr`, which is what the existing `open_file`/`BinaryFile` precedent uses
(`bind.set_function("open_file", ... -> std::unique_ptr<BinaryFile> { return
std::make_unique<BinaryFile>(...); })`, `src/lua_runner.cpp:432-441`). sol2 supports both: a
function returning `std::unique_ptr<T>` or `std::shared_ptr<T>` from a bound lambda is stored
inside the userdata block and the userdata becomes the sole (unique_ptr) or a co-owning
(shared_ptr) reference; either way sol2 auto-installs a default `garbage_collect` metamethod that
destroys the pointee when the *last* owning reference drops (for `shared_ptr`, that is refcount
reaching zero, which for a userdata-held `shared_ptr` in practice means "GC collects this
userdata," same as `unique_ptr`).

The reason to deviate from the `unique_ptr` precedent: **two owners are needed**, not one. Lua's
GC owns the userdata slot (the script's `w` local). `LuaRunner::Impl` additionally needs a
**non-owning observer** so it can find and close any writer still open when `run()` returns,
deterministically, without waiting for Lua's garbage collector (see next section for why that
matters). `LuaRunner::Impl` therefore adds:

```cpp
struct LuaRunner::Impl {
    Database& db;
    sol::state lua;
    std::vector<std::weak_ptr<csv_write::Writer>> open_writers;  // NEW
    ...
};
```

`db:write_csv` pushes the freshly-created `shared_ptr` onto `open_writers` before returning it to
Lua:

```cpp
bind.set_function("write_csv", [this](Database& self, const std::string& path,
                                       sol::optional<sol::table> options) -> std::shared_ptr<csv_write::Writer> {
    const auto resolved = resolve_sandboxed_path(self, "write_csv", path);       // sandbox FIRST (Q3)
    auto opts = write_csv_options_from_lua(options, "write_csv");                // options SECOND (Q3)
    auto writer = std::make_shared<csv_write::Writer>(resolved, path, "write_csv", opts.header, opts.csv_options);
    impl_ ... /* i.e. */ open_writers.push_back(writer);
    return writer;
});
```

(`this` here is `Impl`, since `bind_database()` is an `Impl` member per the existing file — see the
`resolve_sandboxed_path`/`save` lambda capturing `[this]` at line 626 for the exact precedent of a
member lambda reaching into `Impl` state.)

### `w:close()` is explicit — what does `w:write_row(...)` do after close

Pattern 1, naming the operation the script actually called (matching the house rule that
`{operation}` is *the public method the user called*, not a delegate):

```
"Cannot write_row: writer is closed"
```

**Not** `BinaryFile`'s `"File is not open: " + path"` (`src/binary/binary_file.cpp:203`,
`validate_file_is_open`) — that message predates the 3-pattern rule and is explicitly called out
in root `CLAUDE.md` as a known, un-fixed exception (*"new code should use the three patterns"*).
New code (this feature) must not copy an inconsistency forward.

`w:close()` itself is **idempotent** — a second call is a silent no-op, mirroring
`BinaryFile::close()` exactly (`src/binary/binary_file.cpp:110-121`: `if (!impl_->io) { return; }`).
`Writer::close()` should follow the identical shape (`if (!open_) { return; } flush(); ...
open_ = false;`).

### Is sol2's default `__gc` sufficient, or is `sol::meta_function::garbage_collect` needed

**Default `__gc` is sufficient** — no explicit override. This is exactly the `BinaryFile`
precedent: sol2 auto-generates a `garbage_collect` metamethod for any usertype whose constructor
function returns an owning smart pointer (`unique_ptr` for `BinaryFile`, `shared_ptr` here), and
that metamethod runs the C++ destructor of the pointee. `BinaryFile`'s own destructor already does
real cleanup (flushes, closes the file, deregisters from the write-registry); `csv_write::Writer`'s
destructor should do the equivalent (flush + close, silently — no throw, no log; see next section
for why no log). No `new_usertype<...>(..., sol::meta_function::garbage_collect, [](...){...}, ...)`
entry is needed anywhere in this file today, and none should be added here either.

### Where does the "flush, close, warn" for an unclosed writer actually happen

**Not in `__gc`, and not relying on Lua's garbage collector timing at all.** This is the key design
decision, and it resolves the destructor-can't-throw / GC-reentrancy hazard the question raises by
sidestepping it entirely:

1. **`csv_write::Writer`'s destructor** does a best-effort flush + close, **silently** — no
   `spdlog` call, no throw (wrap any I/O failure in a local `try/catch` that swallows it). This is
   the *safety net*, not the primary mechanism, and it only fires whenever Lua's GC actually
   collects the object — which, per the Lua 5.4 reference manual, is **not deterministic** except
   at one specific point (`lua_close`, see below).
2. **`LuaRunner::run()` is the deterministic cleanup point.** After `safe_script` returns (success
   *or* the caught-error path that currently does `if (!result.valid()) { ... throw ...; }`),
   `Impl` walks `open_writers`, `.lock()`s each `weak_ptr`, and for every writer that is still
   alive and still open: flush, close, and `logger->warn("Lua script left db:write_csv writer '{}'
   open at script end; flushed and closed automatically", writer->path())`. The vector is then
   cleared regardless of outcome (an already-collected writer's `weak_ptr::lock()` returns
   `nullptr`, meaning its own destructor already did its silent best-effort close, and there is
   nothing left to warn about through this path — see the gap this leaves, below). This must run
   on **both** the success and the thrown-error path of `run()`, so it needs to sit in a small
   scope guard (or a `try { ...safe_script...; cleanup(); } catch (...) { cleanup(); throw; }`
   shape) around the existing body — it is *ordinary C++ code on the C++ call stack*, not code
   invoked from inside a Lua metamethod, so it can log via `spdlog` with zero reentrancy concern.
   This is exactly why the cleanup must live here and not in `__gc`: `run()`'s stack frame is a
   completely normal place to call into `Database`'s per-instance logger; a `__gc` metamethod
   firing during an arbitrary later Lua allocation (or during `lua_close`, possibly after
   `LuaRunner`/`Database` teardown has begun) is not.
3. **Known gap, worth flagging rather than hiding:** if Lua's incremental GC happens to collect the
   writer *before* `run()` reaches its cleanup pass (possible but not guaranteed — Lua's GC can run
   at any allocation point during script execution), the destructor's silent close already ran, and
   the deterministic pass in `run()` finds a dead `weak_ptr` and emits **no warning** for that
   writer, even though it was in fact left unclosed by the script. This is an acceptable, narrow
   gap: the file is still correctly flushed and closed either way (the *data* is always safe), and
   the only thing lost is the diagnostic warning for an already-rare race. `ponytail:` worth a
   one-line code comment noting the ceiling (warning is best-effort, not guaranteed, when GC
   preempts the deterministic pass) rather than building a heavier mechanism (e.g., a raw
   non-owning pointer registry with explicit unregister-on-destroy) to close it.

### Requires a new logging path — this is a real gap, not an assumption

**`LuaRunner` currently has no logger at all.** `LuaRunner::Impl` (line 243-258) holds only
`Database& db` and `sol::state lua`; there is no `logger` member, and `Database`'s own per-instance
`spdlog::logger` (`src/database.cpp`'s `create_database_logger`, described in `src/CLAUDE.md`
"Logging") is a **private** member of `Database::Impl`, with no public accessor. Root `CLAUDE.md`'s
Logging convention is explicit: *"through a per-database logger instance — never the `spdlog::`
global functions."* Two ways to satisfy that for this feature, in order of how little they touch:

- **Minimal (recommended):** add one small forwarding method, e.g.
  `void Database::log_warning(const std::string& message) const;` in `include/quiver/database.h` /
  `src/database.cpp`, that does `impl_->logger->warn(message);`. `LuaRunner::Impl` already holds
  `Database& db`, so this is a one-line call at the cleanup site (`db.log_warning(...)`), reuses the
  *existing* per-database logger instance (no new sink, no new logger object), and keeps the
  "never call `spdlog::` globals" rule intact. This needs to be listed as a **modified** file in Q7
  — it is not optional plumbing, the feature cannot log a warning without it.
- **Rejected:** give `LuaRunner` its own independent `spdlog::logger` (duplicating
  `create_database_logger`'s sink-construction logic). This would be a second logger instance for
  the same conceptual database, going to a different log file/sink than every other message about
  the same `Database`, which is inconsistent with "one logger per database."

### Mid-script error — does the userdata's destructor reliably run? Does `lua_close` collect it?

**No** to "reliably run immediately," **yes** to "guaranteed by `lua_close`" — and the distinction
is exactly why the design above does not rely on `__gc` timing:

- When `safe_script` returns an invalid result (a caught Lua error — sol2 runs the script in
  protected mode), any Lua-local variable holding the writer userdata simply goes out of scope from
  Lua's perspective. **This does not immediately collect the object.** Lua 5.4 uses an incremental
  (or, if configured, generational) collector; an unreferenced object becomes *eligible* for
  collection but is swept on a later GC step triggered by subsequent allocations, not synchronously
  at the point where it became unreachable. There is **no** guarantee `__gc` fires "at the error."
- The one place Lua's reference manual *does* guarantee finalization of every remaining
  collectible object is `lua_close`: *"[lua_close destroys] all objects in the given Lua state
  (calling the corresponding garbage-collection metamethods, if any)"* (Lua 5.4 Reference Manual,
  §4.8, `lua_close`). Every userdata with a `__gc` metamethod still alive when the state closes gets
  it invoked at that point. In this codebase, `lua_close` happens implicitly when `sol::state lua`
  (a member of `LuaRunner::Impl`) is destroyed — i.e., when the owning `LuaRunner` is destroyed
  (`LuaRunner::~LuaRunner() = default;`, line 1962, which default-destroys `impl_`, which
  default-destroys `sol::state`).
- Net effect: a script that opens a writer and then errors leaves the object's fate non-deterministic
  *between* the error and `LuaRunner`'s own destruction — which is precisely the gap the
  `open_writers` registry in `run()` closes. Without that registry, "flush, close, warn" would only
  be guaranteed to happen whenever the *whole `LuaRunner`* is destroyed (which, for a long-lived
  `LuaRunner` used across many `run()` calls — the CLI and every binding's `LuaRunner`/`Quiver`
  Julia-`do`-block pattern do exactly this — could be arbitrarily later than "script end," directly
  contradicting the requirement). This is the concrete reason the registry-in-`run()` design is
  correct and not over-engineering: the milestone's stated behavior ("still open when the script
  ends") means *this `run()` call*, not *whenever `LuaRunner` eventually dies*.

### Multiple writers open at once; two writers on the same path

- **Multiple writers on different paths at once: must work**, no extra design needed —
  `open_writers` is a `vector`, and nothing in `Writer`'s design assumes singleton use.
- **Two writers on the *same* path: allow it, do not detect it.** Recommend **not** mirroring
  `BinaryFile`'s static write-registry (`src/binary/binary_file.cpp`'s
  `static std::unordered_set<std::string> write_registry`, which blocks a second writer or any
  reader on a path already open for writing). Reasons:
  - Not in scope: `.planning/PROJECT.md`'s Target features / Key Decisions say nothing about this
    case, unlike the binary subsystem where the registry exists because a *reader* opening a
    concurrently-written `.qvr` mid-write would read structurally invalid binary data — a real
    correctness hazard particular to that fixed-shape format.
  - A second CSV writer on the same path is a script bug with an obvious, self-contained blast
    radius (the two writers race for OS-level file truncation/append and the output file gets
    interleaved or overwritten — no different from a Python/Node script calling `open(path, "w")`
    twice), not a correctness hazard for *other* code or data.
  - A registry would be new global mutable state (another `static` set, same shape as
    `BinaryFile`'s, "not thread-safe or multi-process safe" per its own doc) purely to guard against
    a self-inflicted footgun that is out of scope. `ponytail:` global-registry ceiling, add a
    same-path guard if a real report of accidental double-open surfaces — until then it is
    speculative.

## Answer to Q3 — Where the sandbox check goes

**Before decoding the options table — same evaluation order as `db:read_csv`, D-22** (root
`CLAUDE.md`'s `csv_read.cpp` decision, mirrored in `read_csv`'s own comment at
`src/lua_runner.cpp:456-458`: *"D-22 evaluation order: sandbox checks... before the options
table, so a bad separator never masks an escaping path."*). `db:write_csv` should read:

```cpp
const auto resolved = resolve_sandboxed_path(self, "write_csv", path);  // FIRST
auto opts = write_csv_options_from_lua(options, "write_csv");            // SECOND
```

### The write-specific wrinkle: `weakly_canonical` on a path that does not yet exist

`resolve_sandboxed_path` (`src/lua_runner.cpp:850-900`) already uses `fs::weakly_canonical`, which
is **exactly** the right primitive for a not-yet-existing target and needs **no change**:
`std::filesystem::weakly_canonical` is documented (cppreference / the standard) to canonicalize the
*leading portion of the path that exists* and append the *remaining, non-existent portion*
unmodified (lexically normalized, but not resolved against the filesystem) — it explicitly does
**not** require the full path to exist, unlike `fs::canonical`. `resolve_sandboxed_path` already
relies on this for the *root* half of the check (the database directory, which does exist) and
simply applies the same call to `candidate` (the target file, which for a write may not exist yet).
The containment check that follows (`candidate.lexically_relative(root)`, rejecting `..`-escapes
and `candidate == root`) is purely lexical/string-based once both paths are canonicalized, so it is
indifferent to whether the file exists.

Concretely: `db:write_csv("out.csv", ...)` on a database whose directory has no `out.csv` yet
resolves fine — `weakly_canonical` walks up from `out.csv` until it finds an existing ancestor (the
database directory itself, which does exist), canonicalizes that, and appends `out.csv` lexically.
The one behavior worth being explicit about for the plan author: **`resolve_sandboxed_path` does
not check existence at all** (that is deliberately left to each caller — `csv_read::Reader`'s
constructor does its own `fs::exists`/`is_directory`/`file_size` checks *after* the shared sandbox
call, per its own comment: *"resolve_sandboxed_path's weakly_canonical does not require the path to
exist, so these three checks cannot be skipped"*, `src/csv_read.cpp:69-71`). For **write**,
`csv_write::Writer`'s constructor is the analogous place to add write-specific precondition checks —
but the set of checks is different and smaller than the reader's, because "does not exist" is the
*success* case for a write, not an error:
- `is_directory` on the resolved path — still an error (Pattern 1, matching the reader's own
  wording: `"Cannot write_csv: path is a directory: <original_path>"`).
- No `fs::exists`/empty-file checks — a write creates or truncates, so non-existence and
  zero-length are both fine.
- The actual file-open call (`std::ofstream(resolved, std::ios::trunc)` or equivalent) is where a
  real failure surfaces (parent directory not writable, disk full at open time is unlikely but a
  permission error is not) — wrap it exactly like the reader wraps `CSVReader`'s constructor:
  *"Cannot write_csv: cannot open file '<original_path>' for writing: <os reason>"*.

No change to `resolve_sandboxed_path` itself is needed for either read or write — it is already
existence-agnostic by design, and both callers layer their own existence expectations on top.

## Answer to Q4 — Data flow for one `write_row` call

```
Lua script:  w:write_row({ 2014, 1114144.5 })
                 │
                 ▼
sol2 usertype method lambda: [](csv_write::Writer& self, const sol::table& row) { ... }
   (bound on lua.new_usertype<csv_write::Writer>("Writer", ..., "write_row", [...](...) {...}))
                 │
                 ▼
1. Determine expected width:
     - If Writer was constructed with a header, expected_width = header.size() (known at
       construction — no per-call lookup needed).
     - If no header (open question, see PROJECT.md — "not yet settled"), expected_width is
       whatever the plan settles on (candidate default: the first write_row call's own cell count
       becomes the fixed width for every subsequent call).
                 │
                 ▼
2. Extract cells 1..expected_width via `row.get<sol::object>(i)` — the SAME call already used by
   `lua_table_to_vector` (`src/lua_runner.cpp` ~line 1067: `t.get<sol::object>(i)`), which
   correctly yields a nil `sol::object` for a hole rather than erroring. Do NOT use `row.size()`
   (`lua_rawlen`) to drive this loop — it is unreliable across holes, the exact caveat PROJECT.md
   already calls out for this feature ("`nil` → empty cell makes `#t` unreliable... row width has
   to come from the declared header").
                 │
                 ▼
3. Detect "too many cells": walk the table's actual keys (a `pairs`/`for_each` walk, the same
   technique `time_series_rows_from_lua` and `collect_group_columns` use for hole-safe extents —
   "Column extents come from a pairs walk, never sol::table::size()") and reject any positive
   integer key > expected_width, or any non-positive/non-integer key. This is the strict-width
   check firing on the "too wide" side; step 1/2 firing implicitly on the "too narrow" side never
   applies (every index up to expected_width is read regardless of whether the table defines it —
   a genuinely missing trailing cell reads as nil, which is legitimate per spec, not an error).
   **Where the strict-width check ultimately fires**, per this design: inside
   `csv_write::Writer::write_row` itself (it is the object that owns `expected_width` and the
   current row ordinal for the error message), not in `lua_runner.cpp` — mirrors why
   `csv_read::Reader` (not `lua_runner.cpp`) owns `variable_columns(KEEP_NON_EMPTY)`: one place
   that can't diverge if a second Lua entry point is ever added.
                 │
                 ▼
4. Per-cell type dispatch (this happens in lua_runner.cpp, BEFORE handing anything to
   csv_write::Writer — see Q5 for the exact idiom):
     nil        → ""
     boolean    → "1" / "0"
     int64      → std::to_chars (shortest round-trip, preserves the Lua integer subtype)
     double     → std::to_chars (shortest round-trip)
     string     → the Lua string bytes, unmodified
     table/function/userdata/other → throw
       "Cannot write_row: cell #<i> has unsupported Lua type"
       (Pattern 1, naming the 1-based cell index — matches the existing "cell #N" idiom from
       lua_cell_as's `what` parameter, e.g. `lua_table_to_vector`'s "cell #" + std::to_string(i))
                 │
                 ▼
5. lua_runner.cpp now holds `std::vector<std::string> formatted_cells` (size == expected_width,
   already comma/quote-agnostic raw text) and calls `self.write_row(formatted_cells)`.
                 │
                 ▼
6. csv_write::Writer::write_row (inside csv_write.cpp, NOT lua_runner.cpp):
     - re-validates cells.size() == expected_width (belt-and-suspenders if the width check moved
       here instead of step 3 — pick ONE place, not both, per the plan)
     - for each cell: RFC-4180 quoting — wrap in `"..."` and double any embedded `"` if the cell
       contains the writer's separator, a `"`, `\r`, or `\n`; otherwise emit verbatim
     - joins cells with the configured separator, appends the emitted header row once (on first
       call, if a header was given) before the first data row
     - appends the row to an internal buffer / writes straight to the ofstream (buffering strategy
       is a STACK-level micro-decision, not an architecture one — either is fine as long as
       `close()` flushes)
                 │
                 ▼
7. Bytes land in the OS file opened by the Writer's constructor at the sandboxed, resolved path.
```

Named sol2 APIs used at each step, all pulled from **existing idioms already in this file** rather
than invented ones: `sol::table` (the row parameter type), `sol::object` (the per-cell type via
`row.get<sol::object>(i)`), `sol::type` / `get_type()` (for the nil/boolean/table checks — the
house convention prefers `get_type()` over `is<T>()` whenever a check needs to distinguish "is
exactly this Lua type" from sol2's looser `is<T>()` coercions, per the `append_json`/`run()`
encoder's own comment about `is<sol::table>()` also accepting userdata), `is<int64_t>()` /
`is<double>()` (numeric subtype dispatch, see Q5), and `sol::table::for_each` (the extra-cell /
extents walk, matching `time_series_rows_from_lua`'s and `read_csv_options_from_lua`'s own
`for_each` usage at `src/lua_runner.cpp:947`).

## Answer to Q5 — Integer vs float dispatch

**Reuse the exact `is<int64_t>()`-then-`is<double>()` ordering already established in this file —
do not invent a new one.** The precise call is `sol::object::is<int64_t>()`, and it is
load-bearing on `SOL_SAFE_NUMERICS=1` (set unconditionally, not gated by build type, in
`src/CMakeLists.txt`): with that flag, sol2's `SOL_NUMBER_PRECISION_CHECKS` makes `is<int64_t>()`
return **false** for a Lua float — this is what lets the integer/float split survive a Lua 5.4
value's actual subtype rather than degrading to "is a number" (`src/CLAUDE.md`: *"`SOL_SAFE_NUMERICS=1`
... is load-bearing for the whole file... Without it that check degrades to 'is a number' in
release, and the file-wide `is<int64_t>()`-before-`is<double>()` ordering would route every float
into the integer branch."*).

This exact idiom already exists twice in the codebase for a heterogeneous per-cell dispatch (the
same shape `write_row` needs — not the homogeneous-array shape `lua_cell_as<T>`/
`lua_table_to_vector<T>` handle):

- `lua_table_to_value_map` (query/row-upsert parameters, `src/lua_runner.cpp:1085-1112`):
  `is_lua_boolean(val)` → `val.is<int64_t>()` → `val.is<double>()` → `val.is<std::string>()` →
  throw.
- `table_to_element` (`src/lua_runner.cpp:1114-1150`, the scalar half): identical ordering.

`write_row`'s per-cell dispatch should copy this chain verbatim (adding the `nil` branch first,
since a hole/`nil` cell is legal here and maps to `""`, unlike `table_to_element`'s attribute-name
keys which never see a bare `nil` value the same way). Do **not** reach for `lua_cell_as<T>` —
that helper is templated on a single target type `T` and is built for converting every cell of an
already-homogeneous array into that one type (`lua_table_to_vector<T>`'s use case); `write_row`'s
cells are heterogeneous by design (a string cell next to a number cell in the same row), which is
exactly the `lua_table_to_value_map`/`table_to_element` shape, not the `lua_cell_as<T>` shape.

## Answer to Q6 — Build order

### What must be built before what

1. **`src/csv_write.h` / `src/csv_write.cpp`** (or the inline-in-`lua_runner.cpp` variant, per Q1's
   outcome) — added to `src/CMakeLists.txt`'s `QUIVER_SOURCES` list, the same list `csv_read.cpp`
   is already in (`src/CMakeLists.txt:13`). This is a standalone, sol2-free unit; nothing else can
   compile against it until it exists.
2. **The `db:write_csv` / `w:write_row` / `w:close` bindings in `src/lua_runner.cpp`** — depends on
   (1), on `resolve_sandboxed_path` (already present), on `Database::log_warning` (the new
   forwarding method from Q2 — must land in the *same* step, since the cleanup pass in `run()`
   can't compile without it), and on the `is<int64_t>()`/`is<double>()` dispatch chain already in
   this file (Q5 — no new helper needed, just a new call site).
3. **`bindings/js/src/lua-api.ts` (`LUA_DB_API_REFERENCE`)** — **must land in the same commit/PR as
   step 2**, not a follow-up. `bindings/js/test/lua-api-sync.test.ts` parses `src/lua_runner.cpp`
   directly and fails the build the moment `db:write_csv`/`w:write_row`/`w:close` exist as bound
   names without a matching `db:write_csv` / `:write_row(` / `:close(` token somewhere in the doc
   string (the test's three checks: *"every db: method appears as the literal token db:<name>"*,
   *"every BinaryFile/BinaryMetadata/Expression method appears as :<name>("* — though `Writer` is
   a new fourth receiver-agnostic type this last check doesn't currently enumerate; verify whether
   the plan needs to extend that test's `["BinaryFile", "BinaryMetadata", "Expression"]` list to
   include `"Writer"`, or whether `write_row`/`close` land purely as `db:`-prefixed tokens instead
   of a receiver method — this is a small but concrete detail the plan author should settle, since
   `close` as a bare method name collides textually with `BinaryFile:close(` already in the doc).
   `.planning/PROJECT.md` already states this ordering as a hard constraint: *"The agent-facing
   reference ... updated in the same phase as the binding, since `lua-api-sync.test.ts` is a hard
   build gate."*
4. **`tests/test_lua_runner_write_csv.cpp`** — new file, registered in `tests/CMakeLists.txt`
   (mirroring `test_lua_runner_read_csv.cpp`'s registration at `tests/CMakeLists.txt:43`). Depends
   on (2) existing and compiling.
5. **`src/CLAUDE.md`, `tests/CLAUDE.md`, root `CLAUDE.md`** — doc updates (the "Self-Updating"
   house rule), can land alongside step 2/4 rather than strictly after.
6. **`CHANGELOG.md`** — an entry under the current unreleased version. This is additive,
   non-breaking, so per root `CLAUDE.md`'s versioning rule (*"A 0.x minor bump signals breaking
   changes, a patch bump does not"*) it is a **patch** bump, not minor — worth flagging since the
   instinctive read of "new feature → minor bump" is backwards for this project's specific
   convention.

### Which of the six test suites can even see this feature

Because `db:write_csv` rides entirely inside `LuaRunner::run()`'s existing, already-bound,
completely generic "run this Lua source string, get JSON back" pathway (`quiver_lua_runner_run` in
the C API, already bound in Julia/Dart/Python/JS — confirmed by grep: `bindings/julia/src/lua_runner.jl`,
`bindings/dart/lib/src/lua_runner.dart`, `bindings/python/src/quiverdb/lua_runner.py`,
`bindings/js/src/lua-runner.ts` all wrap the same generic entry point), **no new C API symbol and
no new binding code is needed at all** — exactly matching PROJECT.md's own framing ("Lua only — no
public C++ header, no C API, no FFI binding. Same rationale as `db:read_csv`."). This has a direct
consequence for which suites are relevant:

| Suite | Sees `write_csv`? | Why |
|-------|--------------------|-----|
| `quiver_tests` (C++ core) | **Yes — primary location** | New `tests/test_lua_runner_write_csv.cpp`, driving `LuaRunner::run()` with scripts that call `db:write_csv`/`w:write_row`/`w:close` directly. This is the only place the feature's actual behavior is exercised. |
| `quiver_c_tests` | No new coverage needed | `test_c_api_lua_runner.cpp` exercises `quiver_lua_runner_run` generically (arbitrary script string in, JSON out); it has no CSV-specific test today for `read_csv` either — `tests/CLAUDE.md` states plainly *"there is no C++ core, C API, or other-binding counterpart to mirror"* for the read side, and the write side is architecturally identical (opaque script string). |
| Julia / Dart / Python / JS binding suites | No new coverage needed | Same reasoning — their `lua_runner` test files exercise `LuaRunner.run`/`db.run` generically; they don't know `db:write_csv` exists any more than they know `db:read_csv` exists today. |
| `bindings/js/test/lua-api-sync.test.ts` | **Yes — hard gate, no functional test** | Parses `lua_runner.cpp` for bound names, does not execute Lua or touch the native library. It is a *documentation* sync check, not a behavior test, and it is the one binding-side file that MUST change (step 3 above). |

Where its tests live, concretely: **only** `tests/test_lua_runner_write_csv.cpp` (new file) plus
the `lua-api-sync.test.ts` doc-sync assertions (no new test *cases* there beyond what the existing
parametrized checks already do once the doc is updated). This precisely mirrors the `read_csv`
precedent (`tests/CLAUDE.md`: *"there is no C++ core, C API, or other-binding counterpart... so
this suite has no sibling elsewhere"*).

### Suggested test cases for `tests/test_lua_runner_write_csv.cpp`

Basic round trip (write then read back via `db:read_csv` — the existing reader is the easiest
correctness oracle available); `separator` option; `header` option plus the strict-width mismatch
(too few / too many cells, asserting the row ordinal and both counts appear in the message); every
cell-type branch (string, integer, float — asserting exact `to_chars` round-trip text including a
value that would lose precision under `%g`/naive formatting, and an integer that must **not**
gain a trailing `.0`, matching "Lua 5.4's integer subtype preserved" — mirrors how
`test_lua_runner_create.cpp`'s mixed-array tests already probe the boolean/int/float boundary),
boolean → `"1"`/`"0"`, `nil` → empty cell (including a *trailing* nil, which is invisible to `#t`
but must still count toward the header width per Q4), table/function/userdata cell → Pattern 1
error naming the cell index (reuse the "unsupported-type tests use a function, not a boolean"
convention from `tests/CLAUDE.md`, since a boolean is now a *valid* cell type here); `write_row`
after `close()` → Pattern 1 `"Cannot write_row: writer is closed"`; double `close()` is a silent
no-op; an unclosed writer at script end is flushed, closed, and produces a logged warning (assert
via the resulting file's contents, since asserting on `spdlog` output directly is not this
project's house style — no test currently captures logger output); sandbox escape (`../`) rejected
before the options table is even decoded (mirrors `read_csv`'s D-22 ordering test); same-path
double-open is allowed, not rejected (a small explicit test pinning the Q2 decision so a future
change doesn't silently start rejecting it, or silently start allowing corruption without anyone
noticing the behavior flipped); `:memory:` database rejects `write_csv` outright (reuses
`LuaSandboxTest` fixture, same shape as every other file-touching Lua operation's negative test).

## Answer to Q7 — New vs modified files

### New files

| File | Why |
|------|-----|
| `src/csv_write.h` | Public-to-`src/`-only (no `include/quiver/` counterpart) declaration of `quiver::csv_write::Writer` and `Options` — mirrors `src/csv_read.h`. Whether it's Pimpl'd depends on the Q1 outcome; the declared surface is identical either way. |
| `src/csv_write.cpp` | Implementation: RFC-4180 quoting/escaping, header emission, strict-width check, file I/O. |
| `tests/test_lua_runner_write_csv.cpp` | The only test file for this feature (see Q6) — registered in `tests/CMakeLists.txt`'s `add_executable(quiver_tests ...)` source list. |

### Modified files

| File | Why |
|------|-----|
| `src/lua_runner.cpp` | Add `bind.set_function("write_csv", ...)` inside `bind_database()` (next to `open_file`/`bin_to_csv`/`read_csv`, ~line 447); add `lua.new_usertype<csv_write::Writer>("Writer" or similar, "write_row", ..., "close", ..., sol::meta_function::garbage_collect left at sol2's default per Q2); add a `write_csv_options_from_lua` decoder mirroring `read_csv_options_from_lua`'s shape (`separator` + `header`, Q3's ordering); add the `open_writers` registry member to `Impl` and the deterministic flush/close/warn pass in `LuaRunner::run()` (Q2). `#include "csv_write.h"` alongside the existing `#include "csv_read.h"` at line 3. |
| `include/quiver/database.h` + `src/database.cpp` | Add the minimal `Database::log_warning(const std::string&) const` forwarding method (Q2) so `LuaRunner` can reach the existing per-database `spdlog` logger without creating a second one or calling `spdlog::` globals. |
| `src/CMakeLists.txt` | Add `csv_write.cpp` to `QUIVER_SOURCES`, next to `csv_read.cpp` (line 13). |
| `tests/CMakeLists.txt` | Register `test_lua_runner_write_csv.cpp` in `quiver_tests`' source list, next to `test_lua_runner_read_csv.cpp` (line 43). |
| `bindings/js/src/lua-api.ts` (`LUA_DB_API_REFERENCE`) | **Same phase as the binding change** — document `db:write_csv`, `w:write_row`, `w:close`, the `separator`/`header` options, the cell-type/nil/boolean rules, and the strict-width behavior, in prose plus a worked Lua example (matching the existing `## CSV file reading` section's style). Hard build gate (`lua-api-sync.test.ts`) fails otherwise. |
| `bindings/js/test/lua-api-sync.test.ts` | **Only if** the new `Writer` usertype needs to be added to the `["BinaryFile", "BinaryMetadata", "Expression"]` list in the *"every ... method appears as `:<name>(`"* check (see Q6, step 3) — verify at plan time whether `write_row`/`close` are documented as `db:`-style free functions instead, which would need no test change. |
| `src/CLAUDE.md` | Extend the File Map (`csv_write.h/.cpp` entry, same shape as the existing `csv_read.h/.cpp` entry at line 48), and extend the `csv_read.h`/`csv_read.cpp` prose block's discussion (lines 74-98) — or add a sibling paragraph — covering the writer's own Pimpl rationale (or lack thereof, per Q1's outcome), the `open_writers` registry, and the `Database::log_warning` addition. Also extend the "LuaRunner" section's bullet list (lines 337-466) with the new binding's conventions (options decoder, cell dispatch reuse). |
| `tests/CLAUDE.md` | Extend the "C++ core tests" section's Lua bullet list and the `test_lua_runner_read_csv.cpp` paragraph (lines 39-62) with a sibling paragraph for `test_lua_runner_write_csv.cpp`, noting the same "no C++ core/C API/other-binding counterpart" scoping. |
| root `CLAUDE.md` | Update the cross-layer "CSV file read" row (`| CSV file read | N/A | N/A | N/A | N/A | \`db:read_csv()\` / \`db:read_csv_stream()\` |`) to add a parallel "CSV file write" row for `db:write_csv()`/`w:write_row()`/`w:close()`; move the "v1.1" Key Decisions rows from "— Pending" to their resolved outcome once implemented; move the relevant "Active" requirements bullets to "Validated". |
| `CHANGELOG.md` | New entry under the current unreleased section (patch bump per this project's specific minor-means-breaking convention — see Q6). |

## Open questions this document deliberately does not resolve

Per the quality gate's instruction to be honest about gaps rather than inventing answers `.planning/PROJECT.md` itself flags as unsettled:

1. **Row width when no `header` is given.** PROJECT.md: *"An open edge for planning... not yet
   settled."* This document's Q4 data flow assumes a header-driven `expected_width`; the no-header
   case needs an explicit decision (candidate: first `write_row` call fixes the width for the rest
   of that writer's life) before `csv_write::Writer`'s constructor signature can be finalized.
2. **STACK's reuse-vs-hand-roll conclusion** (Q1) — this document gives both outcomes; the plan
   should pick one once STACK research lands, since it changes whether the `csv_write.h/.cpp` split
   is load-bearing or a style choice (though the *file layout* is identical either way, so this does
   not block starting the plan).
3. **Whether `bindings/js/test/lua-api-sync.test.ts`'s receiver-agnostic `:<name>(` check needs a
   fourth type name added** (Q6/Q7) — a five-minute check against the actual doc wording once
   written, not an architectural unknown, but flagged so it isn't missed.
