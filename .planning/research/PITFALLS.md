# Pitfalls Research — v1.1 CSV writing for the Lua runner (`db:write_csv`)

**Domain:** Adding a streaming file-writer capability to an existing, shipped, sandboxed Lua
scripting surface (C++20 / sol2 / SQLite core).
**Researched:** 2026-09-16
**Confidence:** HIGH for anything cited against this repo's own source or the previous milestone's
audit; MEDIUM for the C++ standard-library claims (`std::to_chars` non-finite behavior — verified
against the quoted `printf`-equivalence clause, not by executing a binary in this sandboxed
environment, which blocked freshly-compiled `.exe` execution outright).

## Critical Pitfalls

### Pitfall 1: The writer can truncate the live database file (or another sandbox file) with no protection

**What goes wrong:**
`db:write_csv("study.db", ...)` — or any path that resolves inside the sandbox and happens to name
the open SQLite file, a `.qvr` binary, a `.toml` sidecar, or a migration `.sql` — opens that file in
truncate mode and destroys it. Nothing in the sandbox stops this: `resolve_sandboxed_path` only
checks that the path stays **inside** the database directory, never that it avoids a file the
database itself depends on. This is qualitatively worse than the read path (which can only fail to
read) and worse than the existing `db:open_file(path, "w")` precedent, because the SQLite connection
in the *same process* may have this exact file memory-mapped or WAL-journaled; an `std::ofstream`
open in truncate mode does not respect SQLite's advisory locks, so on POSIX this can silently
corrupt the live, open database out from under its own connection while the script keeps running
against it. On Windows the SQLite file handle may hold a sharing lock that makes the `ofstream` open
fail outright (a `Failed to write_csv: could not open file` error) — so the *symptom differs by
platform*: silent corruption on POSIX, a loud error on Windows, for the exact same script.

**Why it happens:**
The project's own precedent (`BinaryFile::open(mode='w', ...)`, `src/binary/binary_file.cpp:83-103`)
has *never* had this protection — it only refuses a second writer to the same path via
`write_registry` (an in-process guard against two `BinaryFile` writers racing), not a refusal to
overwrite an unrelated, important file. Reusing that same "no overwrite protection" posture for a
CSV writer looks consistent with house style, but the failure mode is new: `BinaryFile` writes are
almost always to a fresh `.qvr` the script itself created; a CSV writer's whole *purpose* per the
milestone shape is writing model-input files into the same case folder where the `.db`/`.qvr`/`.sql`
files already live, so the collision surface is much larger and much more likely to be hit by an
LLM-authored path (a typo, a relative-path miscalculation, or an agent literally trying to "export
the database to CSV" and guessing the db's own filename).

**How to avoid:**
Add one cheap, explicit check in the `db:write_csv` binding (not in the shared
`resolve_sandboxed_path` gate — that gate is reused by seven *other* operations, including
`export_csv`, which has carried the same gap unfixed for a full prior milestone; special-casing it
there risks an unrelated behavior change to already-shipped operations that are out of this
milestone's scope): compare the writer's resolved, canonicalized path against
`weakly_canonical(db.path())` and refuse with a Pattern 1 message
(`"Cannot write_csv: path '<p>' is the open database file"`) on an exact match. This mirrors the
existing `resolve_sandboxed_path` idiom of rejecting `candidate == root` for the same reason (a
derived path colliding with something load-bearing). Whether to extend the same refusal to `.qvr`/
`.toml`/migration files is a real design question to settle explicitly in the plan, not silently —
`BinaryFile`'s own write-registry check (canonicalized path collision) is directly reusable if the
decision is "yes."

**Warning signs:**
A plan or executor that treats `db:write_csv` as "just like `db:open_file(path, 'w')`" and carries
that mode's overwrite posture forward unexamined. A test suite that never points the writer at the
sandbox test fixture's own `.db` file.

**Phase to address:**
The phase that implements the writer handle itself (writer mechanics/sandbox phase) — this is a
Pattern-1-level precondition, cheap to add at the same call site as the sandbox check, and must not
be deferred to a later "hardening" phase since the destructive window exists from the very first
working version.

---

### Pitfall 2: A raw `std::to_chars` call silently writes `"inf"`/`"nan"` text into a data cell

**What goes wrong:**
`std::to_chars` for a floating-point value with no `fmt` argument is specified (cppreference,
quoted verbatim below) as converting the value "as if by `std::printf`." `printf`'s `%f`/`%e`
conversion of a non-finite double is well-defined and produces literal text (`inf`, `-inf`, `nan`,
or a platform-specific variant) — `to_chars` does **not** throw, does **not** set an error code, and
does **not** refuse for `+Infinity`, `-Infinity`, or `NaN`. A Lua script that divides by zero, or
computes `0/0`, and passes the result straight to `w:write_row` gets a CSV cell containing the raw
text `inf` or `nan` with no error raised anywhere in the pipeline (the strict-row-width check only
counts cells, it never inspects their content). That text is not a number to Excel, R, or a naive
CSV loader in any other language — it either becomes a string column (silently defeating whatever
this file was being produced for) or a hard parse error downstream, far from where the bad value was
actually produced.

**Why it happens:**
This is the *same bug class* the project has already hit twice — `%g` truncating floats to 6
significant digits in `database_csv_export.cpp` (fixed by switching to `std::to_chars`), and
csv-parser's own `DECIMAL_PLACES = 5` writer truncation (the reason csv-parser's writer is rejected
for this milestone) — except this third occurrence is the opposite failure mode: switching to
`std::to_chars` *fixes* the precision bug but does **not**, by itself, fix the non-finite case, and
it is easy to assume "we already solved numeric formatting" and skip re-deriving the non-finite
guard for the new call site. The guard already exists exactly once in this codebase, in
`lua_runner.cpp`'s own `append_json_double`, three lines above where a new author would naturally be
reading for a `to_chars` example:
```cpp
void append_json_double(double value, std::string& out) {
    if (!std::isfinite(value)) { out += "null"; return; }
    append_number(value, out);
}
```
JSON has an escape hatch (`null`) that CSV does not — there is no universally-parseable CSV token
for "not a number," so the right behavior for the writer is almost certainly to **throw** (Pattern 1,
naming the cell) rather than silently emit `inf`/`nan` text, which is the opposite policy from the
JSON encoder and worth stating explicitly rather than copy-pasting the JSON encoder's substitution.

**How to avoid:**
Route every numeric cell through an explicit `std::isfinite` check before calling `to_chars`, and
throw a Pattern 1 error naming the row and cell index for a non-finite value — do not reuse
`append_json_double`'s "substitute a token" policy verbatim. Pin this with a test: `w:write_row({0/0,
1})` and `w:write_row({1/0, 1})` must throw, not produce a file.

**Warning signs:**
A code review that sees `std::to_chars` being used and assumes "we're safe, that's the house idiom"
without checking whether the non-finite guard was carried over. A test suite with number-formatting
tests that only cover ordinary finite values (the existing `export_csv` tests and this milestone's
own worked example both use ordinary numbers).

**Phase to address:**
The phase that implements cell-value marshaling (numeric formatting phase) — same phase as Pitfall 3
below, since both are the same code path.

---

### Pitfall 3: `std::to_string` looks equivalent to `std::to_chars` and silently reintroduces the precision bug

**What goes wrong:**
`std::to_string(double)` is specified to behave like `sprintf` with a fixed format (`%f`-equivalent,
6 digits after the decimal point) — it is **not** shortest-round-trip and **not** what
`database_csv_export.cpp` switched to after the `%g` bug. An implementer reaching for "the obvious
stdlib string-from-number function" for the writer, rather than the `to_chars`-based
`append_number<T>` template already sitting in the same translation unit
(`src/lua_runner.cpp:128-135`), reintroduces the exact `%g`-class bug under a different name:
`1234567.891234` becomes `"1234567.891234"` under `to_chars` but `"1234567.891234"` truncated to six
decimals under `to_string` — for values needing more than 6 fractional digits to round-trip exactly,
data is silently lost on write, invisible until a downstream consumer does the math and gets a
slightly wrong answer.

**Why it happens:**
`std::to_string` is the first thing autocomplete/muscle memory reaches for, it compiles cleanly, and
it "looks right" on any test using values under 6 decimal digits — which is most hand-written test
fixtures, including this milestone's own worked example (`1114144.5` round-trips fine under either
function, so a spot-check would not catch the regression).

**How to avoid:**
Reuse `append_number<T>` (or an equivalent `to_chars`-only helper) — do not introduce a second
number-to-string path in the writer. A single shared helper for both the JSON return-value encoder
and the CSV writer means a future fix only has one call site to find.

**Warning signs:**
Grep for `std::to_string` anywhere near the new writer code during review — it is never correct for
a numeric CSV cell in this codebase.

**Phase to address:**
Numeric formatting phase, same as Pitfall 2.

---

### Pitfall 4: A round-trippable double can still lose its "this was a float" identity (whole-valued floats look like integers)

**What goes wrong:**
`std::to_chars`'s own documented contract is: "the smallest number of characters such that there is
at least one digit before the radix point (**if present**) and parsing the representation ...
recovers value exactly." For a Lua float whose value happens to be a whole number — `2014.0`, or any
arithmetic result that lands on an integer (a common occurrence with energy/volume data, e.g. an
evenly-divisible average) — the shortest round-trippable text has *no* radix point at all: `to_chars`
emits `"2014"`, not `"2014.0"`. That text is **numerically identical** to what a genuine Lua integer
`2014` would produce (the milestone's own decision requires the integer subtype to also produce
`"2014"`), so the two are indistinguishable in the output file. The *value* round-trips exactly
(this is not the Pitfall 2/3 data-loss class), but the *type signal* a downstream tool infers by
sniffing the column (pandas `read_csv`, R, Excel) is lost: a column of genuinely-float values that
happen to be whole numbers will be typed as an integer column by anything that infers dtype from the
text, which is silently wrong for a column the schema or the script's intent says is a float (e.g. a
`"mw"` column that should stay `float64` downstream).

**Why it happens:**
This is a direct, previously-undocumented consequence of "shortest round-trip" formatting applied to
CSV instead of JSON. It is not a bug in the numeric value at all, which is why it is easy to miss in
review — a test that only checks "does the value read back correctly" (round-trip by value) will
never catch this, only a test that checks the literal *text* of a whole-valued float cell.

**How to avoid:**
Decide explicitly, and document the decision: this is very likely **acceptable** (the value round-
trips exactly, and neither Lua nor `to_chars` has any concept of "preserve a trailing `.0` for a
whole float" without extra bookkeeping the milestone's two-option, no-extra-flags design does not
want), but it must be a documented, tested behavior rather than an unexamined side effect. Pin it
with a test asserting the literal text `w:write_row({2014.0, 1})` produces for the first cell — so a
future change to the number formatter cannot silently flip this without a test noticing.

**Warning signs:**
No test asserts the literal string content of a whole-valued float cell (only asserts it parses back
to the right number). A user report that "my float column came back typed as int in pandas."

**Phase to address:**
Numeric formatting phase — as a documented, tested decision, not a fix (there may be nothing to fix,
only something to make explicit).

---

### Pitfall 5: Lua's `/` operator silently converts a large exact integer to an imprecise float

**What goes wrong:**
Lua 5.4's division operator (`/`) **always** produces a float result, even `10 / 1`. A script that
does anything resembling `id / 1` (a common "just make sure it's a float" or "normalize" idiom, or
an accidental division inside a formula) on an `int64` value larger than 2^53 silently converts an
exact integer into an imprecise `double` — the classic float64-loses-precision-above-2^53 problem,
except triggered by ordinary Lua arithmetic rather than anything writer-specific. Once that happens,
the writer (correctly, per Pitfall 4's contract) round-trips the *resulting double* exactly — the
precision was already lost one step earlier, in Lua, before the writer ever saw the value.

**Why it happens:**
This is not a writer bug at all — it is a pre-existing Lua-language sharp edge that becomes
observable for the first time in this milestone because, for the first time, a Lua number is
converted to *persisted text* rather than staying inside the Lua VM or being marshaled through a
typed column write (where the schema's declared column type, not Lua's runtime subtype, decides
`int64_t` vs `double` on the C++ side).

**How to avoid:**
Nothing to fix in the writer — this is Lua's number model working as specified. Worth a one-line
note in the agent-facing reference (`LUA_DB_API_REFERENCE`) if large-integer columns (e.g. raw
external IDs) are a plausible input, since an LLM-authored script is exactly the kind of author that
reaches for `x / 1` as a "make it a number" idiom without knowing Lua's float-division rule.

**Warning signs:**
None specific to code review; this is a documentation/prompt-payload risk, not a correctness bug —
list it explicitly so it is a conscious decision to skip, not an unexamined gap.

**Phase to address:**
Docs/prompt-payload phase (optional — low priority, but cheap to note once discovered).

---

### Pitfall 6: A destructor that throws (or logs) during Lua GC is undefined behavior / a lifetime hazard

**What goes wrong:**
The milestone's own decided shape requires "a writer still open when the script ends is flushed and
closed, with a warning logged." If this cleanup lives in the writer userdata's C++ destructor
(invoked as sol2's generated `__gc` metamethod, or during `lua_close()`'s final GC sweep when
`LuaRunner::Impl`'s `sol::state` member is torn down), two independent hazards stack:
1. **A throwing destructor here is not safely recoverable.** Regular sol2-bound member functions
   (everything else in this file, e.g. `write_row`) get sol2's automatic exception-to-Lua-error
   translation because they run through sol2's normal protected-call wrapper. The `__gc` path
   invokes the C++ destructor directly as part of Lua's own (C-compiled) garbage-collection sweep;
   an exception escaping there is not a Lua error, it is undefined behavior that will likely
   `std::terminate` the whole host process — turning "my script forgot to close a writer" into a
   process crash.
2. **There is no `Database` logger reachable from here to satisfy "log a warning."** `spdlog::logger`
   lives on the *private* `Database::Impl` (`src/database_impl.h:62`) — `include/quiver/database.h`
   exposes no accessor for it at all (confirmed: zero matches for `logger` in the public header), and
   `src/lua_runner.cpp` currently makes **zero** logging calls anywhere — this would be the first.
   `src/CLAUDE.md`'s logging rule additionally forbids the `spdlog::` global functions, which is the
   tempting workaround. A plan that assumes `db.logger->warn(...)` "just works" will not compile;
   a plan that reaches for `spdlog::warn(...)` as a shortcut violates the house per-database-logger
   rule.

**Why it happens:**
The shape of the requirement ("flush, close, warn when the script ends") reads like a destructor's
job, and the project's own `BinaryFile::Impl::~Impl()` precedent (`src/binary/binary_file.cpp:35-42`)
*does* do cleanup in a destructor — but that precedent is instructive by what it deliberately does
**not** do: it flushes and unregisters, and nothing else. It never throws (an unchecked `flush()`
on a stream with no exception mask set just marks the stream's failbit, silently) and it never logs.
Extending that precedent to "also log a warning" is where the new risk enters.

**How to avoid:**
Do not put the primary cleanup-and-warn logic in the destructor at all. Instead, track open writers
explicitly in `LuaRunner::Impl` (a small registry — e.g. a `std::vector<CsvWriter*>` of raw,
non-owning pointers populated when `db:write_csv` creates a handle, matching the spirit of
`BinaryFile`'s own `write_registry`) and, at the end of `LuaRunner::run()` — a normal C++ scope where
`db` (a `Database&`) is still guaranteed alive and reachable, and where an ordinary `try`/`catch`
*can* safely run — explicitly close and warn on every writer still open, wrapping each one in its
own `try { ... } catch (...) {}` so one bad writer's flush failure cannot skip the rest. Reserve the
C++ destructor itself as a silent, no-throw, no-log safety net (idempotent `close()`, matching
`BinaryFile::close()`'s `if (!impl_->io) return;` re-entrancy guard) for the truly unreachable case
(e.g. a Lua panic that skips normal script-end cleanup). This also sidesteps needing a `Database`
logger accessor from GC-time code entirely, since the warning is logged from inside `run()`, which
already has one available — though `Database` still needs *some* way to expose logging to
`lua_runner.cpp` (an internal, non-public accessor, or a small logging shim), which is a design
decision this phase must make explicitly rather than discover mid-implementation.

**Warning signs:**
A plan that puts `close()`+`spdlog` calls directly in `~CsvWriter()`. A test that asserts the warning
is logged but never actually forces a real Lua GC cycle to fire the destructor (Lua's GC is not
immediately deterministic the way C++ RAII is at scope-exit — a test that leaves a writer as a local
Lua variable and simply ends the script relies on `lua_close()`'s final full collection, which is a
different code path from an incremental GC pass mid-script).

**Phase to address:**
The lifecycle/hardening phase — this is the single highest-complexity, highest-risk piece of the
whole feature and deserves its own dedicated review pass, separate from "does write_row produce
correct bytes."

---

### Pitfall 7: The writer must not hold a `Database&`/`db` pointer — it doesn't need one, and holding one reintroduces a dangling-reference risk

**What goes wrong:**
`LuaRunner::Impl` holds `Database& db` **by reference** (documented as borrowed, not owned — root
`CLAUDE.md`: "a `LuaRunner` borrows its `Database` ... and must not outlive the block"). If the new
writer userdata is given its own `Database&`/`db` member (the natural thing to reach for if Pitfall
6's logging problem is "solved" by stashing a reference at construction time instead of routing
through `run()`), it inherits the same lifetime constraint *at the object level* — but the writer is
a Lua-GC-owned object whose destruction timing is not tied to the same scope as `LuaRunner` itself.
If a caller keeps the `LuaRunner` (and its borrowed `Database&`) alive only as long as one `run()`
call, but a script somehow lets a writer handle escape that call's lifetime (not currently possible
via the JSON-only return channel, but easy to introduce by accident in a future change, or via a
coroutine that yields with the writer captured), the writer's `Database&` can dangle independently
of `LuaRunner`'s own reference.

**Why it happens:**
It is the "obvious" fix for Pitfall 6's missing-logger problem — reach for `db` because it is right
there in the enclosing lambda's capture list already (every other binding in this file captures
`Database&` for its lambdas). The difference is that every *other* binding is a plain function call
that completes within one Lua C-call frame; a writer userdata *persists* across many calls.

**How to avoid:**
The writer's state should be self-contained: an open file handle/stream, the declared header (for
row-width validation), a row counter, and nothing that reaches back into `Database`. This is also
exactly what the recommended fix for Pitfall 6 produces for free — since the flush-and-warn logic
lives in `LuaRunner::run()`'s own scope (which already has `db`), the writer object itself never
needs to store it.

**Warning signs:**
A `CsvWriter` (or equivalent) class/struct with a `Database&` or `Database*` member.

**Phase to address:**
Same lifecycle/hardening phase as Pitfall 6 — these are two faces of the same design choice.

---

### Pitfall 8: The build gate will not actually catch an undocumented `w:write_row`/`w:close` unless the new usertype name is added to a hardcoded array

**What goes wrong:**
`bindings/js/test/lua-api-sync.test.ts` looks like a comprehensive "every bound Lua name must be
documented" gate, but reading its actual matching logic shows a real, specific blind spot for this
milestone. There are three passes:
1. `bind.set_function("name", ...)` calls on the `Database` usertype → become `db:name` checks
   (this **does** cover `db:write_csv` itself, the factory method, exactly like `db:read_csv`).
2. `ns.set_function("name", ...)` calls on the `quiver` namespace table → `quiver.name` checks (not
   relevant here).
3. `new_usertype<T>("T", "name", lambda, ...)` variadic pairs, attributed by scanning for the
   preceding `new_usertype<(\w+)>` line — this is how `BinaryFile`, `BinaryMetadata`, and
   `Expression` methods are discovered. **Only `usertypeMethods.get("Database")` entries get merged
   into the `db:`-checked set (line 46)** — every other usertype's methods are collected into the
   `usertypeMethods` map but are **only actually asserted against the doc by the fourth test**, which
   iterates a **hardcoded literal array**: `for (const type of ["BinaryFile", "BinaryMetadata",
   "Expression"])`. A new usertype for the writer handle (whatever it is named — e.g. `CsvWriter`)
   will have its `write_row`/`close` methods correctly parsed into `usertypeMethods.get("CsvWriter")`
   by the existing regex, but **that map entry is never consulted by any assertion** unless the new
   type's name is manually added to that array. The build will pass green with `w:write_row`
   completely undocumented in `LUA_DB_API_REFERENCE` — the one scenario the sync test exists
   specifically to prevent.

**Why it happens:**
The test's own comment ("receiver-agnostic ... no fixed token is available") explains *why* it uses
a hardcoded array instead of a fully generic "every usertype found" loop: usertype methods are
documented in the reference with ad-hoc receiver names (`f:`, `md:`, `e:`), so there is no mechanical
way to derive the exact expected token the way `db:name`/`quiver.name` can. That is a reasonable
design for the *existing* three types, but it makes the gate **not future-proof by construction** —
adding a fourth usertype is a two-file change (the binding **and** this test file), not a one-file
change, and nothing fails loudly if the second file is forgotten.

**How to avoid:**
Add the writer's usertype name to the literal array in `bindings/js/test/lua-api-sync.test.ts`
(`for (const type of ["BinaryFile", "BinaryMetadata", "Expression", "<WriterTypeName>"])`) in the
**same commit** that adds the usertype in `src/lua_runner.cpp`, and confirm by temporarily *removing*
the doc entry for `w:close(` and re-running the test suite to see it actually go red (the same
mutation-check discipline the phase 1 verifier used for the sandbox-gate fix in v1.0 — see Pitfall 12
below) before trusting it is wired correctly.

**Warning signs:**
`bun test test/lua-api-sync.test.ts` passing despite a known-undocumented method — verify this
negatively, don't just trust a green run.

**Phase to address:**
The documentation-sync phase (the phase that also updates `LUA_DB_API_REFERENCE`) — but the *array
edit* itself must land in the same commit as the C++ binding, not deferred, since an intervening
commit would ship with a silently-blind gate.

---

### Pitfall 9: Weak, string-search CSV tests are a known, named, twice-occurring trap in this codebase — a writer's test suite is at high risk of repeating it a third time

**What goes wrong:**
`PROJECT.md`'s own "Known issues found while scoping" section names this exact failure mode already
in effect for `export_csv`: *"There are 118 CSV tests and none assert import-side quoting; the
RFC-4180 tests are export-side string searches that never re-import."* A writer test suite that
follows the same natural pattern — write a row, then `EXPECT_THAT(file_contents, HasSubstr("2014,
1114144.5"))` — inherits the identical blind spot: it cannot detect a quoting rule that fires when it
shouldn't (or fails to fire when it should) for any value the fixture doesn't happen to already
contain a separator/quote/newline in, it cannot detect a regression in `std::to_chars` usage for
values the *other* tests don't happen to number-check, and it proves nothing about whether the file
the writer just produced is actually **valid input to this project's own `db:read_csv`** — the one
consumer this milestone explicitly exists to feed data into and out of, per the "input files are
dirty and arbitrary" framing of v1.0.

**Why it happens:**
A string-search assertion is faster to write and easier to read in a diff than a full round-trip,
and it is the path of least resistance once a `Reader`-side test suite (this project's `csv_read`
tests) already exists as a model to imitate for the "happy path" shape — but those tests read
*committed fixture bytes*, they do not test a writer at all.

**How to avoid:**
Make round-tripping through the project's own reader the *primary* correctness lever for the writer,
not a nice-to-have: `w:write_row(...)` a set of rows including at least one cell containing the
declared separator, one containing a double-quote character, one containing an embedded newline, one
empty (`nil`) cell, and the boundary numeric cases from Pitfalls 2-4 — `close()` — then
`db:read_csv()` (or `_stream`) the same file back and assert the recovered rows equal what was
written (accounting for the documented string-only nature of the reader — every cell comes back as a
string, so the assertion compares against the writer's *own* string serialization of each input, not
the original Lua value). This single test pattern is a universal detector for the quoting-correctness
risk noted below (Pitfall 10) without needing to hand-audit a hand-rolled quoter's every branch.

**Warning signs:**
A CSV-writer test file where every assertion is `HasSubstr`/`Contains` against raw file text and
`db:read_csv`/`read_csv_stream` never appears anywhere in that file.

**Phase to address:**
Every phase that touches writer correctness should include at least one round-trip test by its own
verification step, not deferred to a final "polish" phase — but a dedicated round-trip regression
suite (mirroring `test_lua_runner_read_csv.cpp`'s dirty-input fixtures) belongs in whichever phase
is this milestone's analogue of v1.0's Phase 2 ("the dirty files parse correctly").

---

### Pitfall 10: csv-parser's `DelimWriter` cannot be reused for a runtime separator — a hand-rolled quoting decision is a new place to get RFC-4180 subtly wrong

**What goes wrong:**
Per `PROJECT.md`'s own scoping notes, `csv::DelimWriter<OutputStream, Delim, Quote>` takes the
delimiter and quote character as **compile-time template parameters**, which does not fit a runtime
`separator` option — and its numeric formatter is separately disqualified by the 5-decimal
truncation bug (Pitfall 2/3's sibling in the vendored dependency). Whichever way this is resolved
(a switch over a small set of pre-instantiated delimiters, or a fully hand-rolled writer), the actual
**quoting decision** — does this cell need to be wrapped in `"..."` because it contains the
separator, a `"` character (which must also be doubled inside the quotes), or a line break — has to
be reimplemented rather than reused wholesale from the vendored library's tested state machine. A
hand-rolled quoting predicate that misses one of those three trigger conditions, or fails to double
an internal quote, produces a file this project's **own**, already-verified `db:read_csv` cannot
parse back correctly (or worse, mis-parses without erroring, silently shifting cells).

**Why it happens:**
RFC-4180 quoting has exactly the shape of rule that is easy to state ("quote if it contains the
delimiter, a quote character, or a newline") and easy to get subtly wrong in a first pass — the
project's own reader side needed multiple pinned decisions (`D-12`, `D-13`, `D-20`, `D-22`, per the
comments in `src/csv_read.cpp`/`src/CLAUDE.md`) to get right on the read side of the exact same
format, over an entire phase of a prior milestone.

**How to avoid:**
This is exactly what Pitfall 9's round-trip test catches for free — write a cell containing the
active separator, one containing an embedded quote character, and one containing an embedded
newline, close, then read the file back with `db:read_csv` and assert the cell content matches
exactly. No amount of code review substitutes for actually parsing the writer's own output back
through the project's verified reader.

**Warning signs:**
A writer implementation with no cell containing the separator/quote/newline anywhere in its own test
fixtures.

**Phase to address:**
The phase implementing the writer's cell-serialization logic; verified by the round-trip suite from
Pitfall 9.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|--------------------|-----------------|------------------|
| Cleanup/warn logic lives in the C++ destructor (`__gc`) instead of an explicit end-of-`run()` sweep | Less code, feels idiomatic RAII | Undefined behavior if it ever throws; no safe path to a `Database` logger; GC timing is nondeterministic and hard to test | Never for the *primary* mechanism — acceptable only as a silent, no-throw, no-log last-resort net |
| Reusing `resolve_sandboxed_path`'s existing `candidate == root` idiom as the *only* destructive-overwrite guard, without a db-path-collision check | No new code path | Leaves the exact "wrote over the open database" scenario in Pitfall 1 unguarded | Never — this is the milestone's single highest-severity gap if skipped |
| String-search (`HasSubstr`) assertions for writer output, mirroring the existing `export_csv` test style | Fast to write, matches house style | Repeats the named, already-admitted weak-test trap a third time; proves nothing about read-back validity | Acceptable *only* as a supplement to at least one round-trip test per feature, never as the sole coverage |
| Skipping the destructive-overwrite check for `.qvr`/`.toml`/migration files, protecting only the exact `db.path()` match | Smaller diff | An agent that overwrites a `.qvr` binary or migration file mid-case-folder is still fully unprotected | Acceptable for v1.1 if explicitly documented as a scoping decision, not silently dropped |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|-----------------|-------------------|
| `bindings/js/test/lua-api-sync.test.ts` | Assuming the sync test auto-covers a new usertype's methods the same way it covers `Database`'s | Manually add the new usertype's name to the hardcoded `["BinaryFile", "BinaryMetadata", "Expression"]` array in the same commit; verify by temporarily deleting a doc line and confirming the test goes red |
| `Database`'s private per-database `spdlog::logger` | Assuming `db.logger` (or `spdlog::warn`) is reachable from `lua_runner.cpp` | It is not currently exposed publicly at all, and `src/CLAUDE.md` forbids the `spdlog::` globals — this needs an explicit, reviewed accessor decision, not an ad-hoc workaround |
| `LuaRunner::Impl`'s borrowed `Database& db` | Giving the new writer userdata its own `Database&`/`db` member for convenience | Keep the writer self-contained (file handle, header, row count only); route any `db`-dependent cleanup through `LuaRunner::run()`'s own scope, which already holds a valid reference |
| csv-parser's `DelimWriter` | Assuming the vendored writer can be reused with a runtime-configured separator | It cannot (compile-time template parameter) and its numeric formatter independently truncates at 5 decimals — reimplement quoting, verify by round-trip against `db:read_csv`, not by inspection |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|------------------|
| Per-row `std::ofstream::flush()` (if added defensively against Pitfall 6's data-loss concern) | Streaming write throughput collapses on a large script-generated file | Flush only on `close()` (and the end-of-`run()` safety sweep), not per `write_row` — the OS/stream buffer already handles this; this milestone explicitly excludes the GB-scale write path from scope, so this is a minor trap, not a blocker | Only matters if a script writes many thousands of rows in one call, which is plausible for "dump intermediate state for inspection" |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Treating `resolve_sandboxed_path`'s existing containment check as sufficient for a *creation* path | A symlink or directory structure that does not yet exist at check time is not the same risk surface as the read case (which requires the target to already exist) — but this project's own `weakly_canonical` head/tail semantics (canonicalizes the longest *existing* prefix, appends the rest unresolved) mean an *existing* symlinked directory inside the sandbox pointing outside it is still caught, since `canonical()` resolves it before the containment check runs — verified against cppreference's documented head/tail behavior | No code change needed here — `resolve_sandboxed_path` is reused unchanged and remains correct for the write case for symlinks that already exist; but add one explicit test exercising a write through a pre-existing symlinked subdirectory pointing outside the sandbox, since today's suite (`tests/CLAUDE.md`) only exercises this shape on the read side |
| No refusal to overwrite the live database file | Silent corruption of the open, in-use SQLite file (POSIX) or a confusing open-failure (Windows) for the identical script | See Pitfall 1 |
| Non-existent parent directory silently accepted by the sandbox check, then failing (or being silently auto-created) at actual file-open time | `weakly_canonical`'s head/tail split does **not** require the full path to exist — a path like `newdir/out.csv` where `newdir` does not yet exist passes the containment check trivially (it cannot escape a directory that isn't there), deferring the real failure to the file-open call, with no `mkdir -p` decided for this feature (unlike `export_csv`, which does `fs::create_directories(parent)` before writing) | Decide explicitly in the plan: does `db:write_csv` auto-create missing parent directories (matching `export_csv`'s existing behavior) or throw a clear "directory does not exist" Pattern 1 error? Either is acceptable; leaving it undecided is not — an LLM-authored path with an extra directory segment is a very plausible occurrence |

## "Looks Done But Isn't" Checklist

- [ ] **Non-finite guard:** `w:write_row({0/0})` and `w:write_row({1/0})` throw rather than writing
  `nan`/`inf` text — verify with an explicit test, not by inspection of the formatter code.
- [ ] **Destructive-overwrite guard:** `db:write_csv("<the open db's own filename>", ...)` throws —
  verify against the actual sandbox test fixture's `.db` file, not a synthetic path.
- [ ] **Round-trip correctness:** at least one test writes a separator-containing, quote-containing,
  and newline-containing cell, closes, and reads the file back via `db:read_csv`/`read_csv_stream`
  to confirm exact recovery — not just a string-search over the raw file.
- [ ] **Sync-test coverage:** the writer's usertype name appears in
  `bindings/js/test/lua-api-sync.test.ts`'s hardcoded method-coverage array, verified by a
  temporarily-broken doc entry actually failing the test.
- [ ] **Double-close / write-after-close:** `w:close(); w:close()` does not throw (idempotent,
  matching `BinaryFile::close()`); `w:close(); w:write_row({1})` throws a clear Pattern 1 error.
- [ ] **Unclosed-writer-at-script-end path:** a script that opens a writer, writes rows, and returns
  without calling `close()` still produces a complete, valid, readable file on disk, and this is
  exercised by an *actual* end-of-`run()` test (not merely a destructor unit test), since the
  requirement is about script-end behavior specifically.
- [ ] **Mid-script error leaves a well-formed partial file:** a script that writes N good rows and
  then errors on row N+1 (strict-width mismatch) leaves exactly N complete data rows on disk, not a
  truncated final row — verified by reading the file back afterward.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|----------------|------------------|
| Pitfall 1 (destructive overwrite shipped without the guard) | HIGH | This is data loss against a live database file — no in-process recovery; requires a backup/restore story outside this feature entirely. Strongly prefer catching this in review/testing before ship over any "recovery." |
| Pitfall 2/3 (non-finite or truncated numeric text shipped) | LOW | Add the missing guard/switch the formatter call, add the regression test that would have caught it, no data migration needed (the bug is in newly-written files only) |
| Pitfall 6 (destructor throws / crashes process) | MEDIUM | Move the flush/close/warn logic out of the destructor into an explicit end-of-`run()` sweep in `LuaRunner::Impl`; add the missing logger accessor |
| Pitfall 8 (sync test blind to the new usertype) | LOW | One-line array edit in `lua-api-sync.test.ts`; no production code change |

## Pitfall-to-Phase Mapping

Suggested phase themes (this milestone's roadmap is not yet written; naming follows v1.0's own
three-phase shape — mechanics, correctness-on-dirty/edge input, agent-facing correctness):

| Pitfall | Suggested Phase | Verification |
|---------|-------------------|----------------|
| 1 — destructive overwrite of the live db / sandbox files | Phase 1 — writer mechanics + sandbox | Test pointing `db:write_csv` at the sandbox fixture's own `.db` path asserts a throw, not a truncated file |
| 7 — writer must not hold `Database&` | Phase 1 — writer mechanics | Code review / static check: the writer type has no `Database`-typed member |
| 5 — non-existent parent directory behavior undecided | Phase 1 — writer mechanics | Explicit test for both the chosen behavior (auto-create or throw) |
| 2, 3, 4 — numeric formatting (non-finite, `to_string` trap, whole-float identity) | Phase 2 — numeric & quoting correctness | Dedicated numeric-formatting test file covering non-finite, large int64, whole-valued float text, and the int/float-subtype boundary |
| 9, 10 — weak tests / hand-rolled quoting correctness | Phase 2 — numeric & quoting correctness | Round-trip suite: write dirty cells, `close()`, read back via `db:read_csv`, assert exact recovery |
| 6 — GC/destructor safety, missing logger accessor | Phase 3 — lifecycle & hardening | Test that forces a real end-of-`run()` unclosed-writer path (not just a C++ unit test of the destructor) and asserts both file completeness and a logged warning |
| 8 — sync-test blind spot for the new usertype | Phase 3 (or whichever phase updates `LUA_DB_API_REFERENCE`) | Mutation check: temporarily remove the doc entry, confirm the sync test fails, then restore it |
| Lua-language number-division footgun (item 5 in the question, "Pitfall 5" above) | Docs phase | One-line mention in the agent-facing reference if large-integer columns are a realistic input |

## Sources

- This repository's own source, read directly and cited by path/line above: `src/lua_runner.cpp`,
  `src/csv_read.h`/`.cpp`, `src/database_csv_export.cpp`, `src/binary/binary_file.cpp`,
  `include/quiver/database.h`, `src/database_impl.h`, `bindings/js/test/lua-api-sync.test.ts`,
  `src/CLAUDE.md`, `bindings/js/CLAUDE.md`, root `CLAUDE.md`, `tests/CLAUDE.md`.
- `.planning/PROJECT.md` (v1.1 milestone scoping — the "cannot reuse" and "known issues found while
  scoping" sections, which independently named several of the pitfalls above before this research).
- `.planning/v1.0-MILESTONE-AUDIT.md` and the archived `01-VERIFICATION.md` under
  `.planning/milestones/v1.0-phases/01-a-lua-script-reads-a-csv-file/` — the precedent for
  Windows-vs-POSIX OS-level test levers (`CreateFileW`/`dwShareMode=0` vs `chmod 000`), and for the
  "human_needed" items being closed by disproving "no portable trigger exists" rather than deferred.
- cppreference `std::to_chars` (fetched directly, quoted verbatim): the no-`fmt` overload's Effects
  clause ("as if by `std::printf` in the default (\"C\") locale ... chosen according to the
  requirement for a shortest representation").
- cppreference `std::filesystem::weakly_canonical`: documented head/tail behavior for a
  partially-nonexistent path (`canonical(head)/tail`, tail lexically appended unresolved).
- `build/_deps/csv_parser-src/include/internal/csv_writer.hpp` (vendored dependency, read directly):
  `DECIMAL_PLACES = 5` and the compile-time `DelimWriter<OutputStream, Delim, Quote>` template
  signature that rules out reuse for a runtime separator.
- `std::to_chars`' non-finite behavior (Pitfall 2) is reasoned from the quoted "as if by `printf`"
  equivalence clause plus `printf`'s well-established C-standard behavior for non-finite doubles;
  this environment's sandboxing blocked execution of a freshly-compiled verification binary (every
  attempt to run a newly-linked `.exe` was denied with "the process cannot access the file," even
  after the compiling process had exited), so this claim is standards-text-verified rather than
  empirically executed here — flagged MEDIUM confidence and worth a quick empirical spot-check
  (`std::to_chars` on `0.0/0.0` and `1.0/0.0`) as the first thing done in the numeric-formatting
  phase, before relying on it further.

---
*Pitfalls research for: Quiver v1.1 — CSV writing for the Lua runner*
*Researched: 2026-09-16*
