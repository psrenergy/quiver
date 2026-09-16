# Stack Research

**Domain:** C++20 embedded-Lua CSV writer (streaming, RFC-4180) for `db:write_csv`
**Researched:** 2026-09-16
**Confidence:** HIGH

## Recommendation (one line)

**Add nothing. Hand-roll a ~60-90 line RFC-4180 writer against `std::ofstream` +
`std::to_chars`, header-only or a thin `.cpp`, sibling to `csv_read.h`/`.cpp`.** Every rung above
"write the code" fails for a concrete, verified reason (below). This is a `csv_read.h` situation
in reverse: last milestone the reader had a verified-correct vendored state machine worth wrapping;
this milestone the vendored writer has two disqualifying defects and the correct part
(quoting/escaping) is a well-understood ~20-line algorithm not worth a dependency to get.

## Why not reuse `csv::DelimWriter` (verified against the vendored source)

Read directly at `build/_deps/csv_parser-src/include/internal/csv_writer.hpp` (csv-parser 5.3.0,
the version already pinned in `cmake/Dependencies.cmake`):

1. **Delimiter and quote are compile-time template parameters**, not runtime state —
   `template<class OutputStream, char Delim, char Quote> class DelimWriter` (line 254);
   `CSVWriter<OutputStream> = DelimWriter<OutputStream, ',', '"'>` and
   `TSVWriter<OutputStream> = DelimWriter<OutputStream, '\t', '"'>` (lines 664, 675). The only
   runtime constructor parameter is `bool _quote_minimal` (line 259). Quiver's `separator` option
   is a **runtime** `char` with no restricted value set — verified in `lua_runner.cpp`
   (`read_csv_options_from_lua`, lines 950-973): any single-character string is accepted,
   including one that collides with the quote character. `DelimWriter` cannot represent that
   without a `switch` instantiating one `DelimWriter<std::ofstream, C, '"'>` per accepted `char` —
   256 instantiations to be exhaustive, or a silent behavior gap (reader accepts a separator the
   writer rejects/mis-templates) for anything not enumerated. Four instantiations (`,` `;` `\t`
   `|`) would quietly narrow the writer's accepted alphabet below the reader's, which is the kind
   of asymmetry this project's own design decisions explicitly avoid elsewhere (read/write
   symmetry is called out by name in root `CLAUDE.md` for CSV export/import FK handling).
2. **The float formatter is disqualifying on its own.** `internals::to_string<floating_point>`
   (lines 135-186) hand-rolls digit extraction via `pow10`/`std::modf`/`std::fmod` against a
   module-level `static int DECIMAL_PLACES = 5` (line 25), truncating (now rounding, per 3.5.1's
   changelog — see Version Compatibility) every float to 5 decimal places. This is the *exact* bug
   class `database_csv_export.cpp` already hit and fixed with `std::to_chars` (`PROJECT.md`
   context, `%g` truncating to 6 significant digits). Quiver's `write_csv` requirement is explicit:
   "Numbers written with `std::to_chars` shortest round-trip." So even in an Option-A world, every
   cell would have to be pre-stringified by Quiver's own formatter before reaching `DelimWriter`,
   which reduces `DelimWriter` to a thin `write_field(std::string)` wrapper around code Quiver
   still owns and tests — buying nothing.
3. **The quoting/escaping state machine is correct RFC-4180** — confirmed by reading
   `write_escaped_field` (line 555) and `write_quoted_field` (line 572): quote iff the field
   contains `Quote`, `Delim`, `\r`, or `\n` (`find_first_special_for_writer`, line 480), and double
   every internal quote character while copying the field in chunks around each one. This is also
   the one part of `csv_writer.hpp` too small to be worth vendoring for: it is ~20 lines of logic
   once the compile-time-delimiter problem forces a hand roll anyway (see below).

**Net: reusing `DelimWriter` buys back only the one correct third**, and paying for it costs either
an instantiation switch that narrows the accepted separator alphabet, or defeats itself by needing
Quiver's own numeric layer in front of it regardless.

### What including `csv_writer.hpp` would cost, and does it revive the `/bigobj` risk

`csv_writer.hpp` pulls `basic_csv_parser_simd.hpp` (172 lines), `common.hpp` (492 lines), and
`csv_exceptions.hpp` (170 lines) — all confirmed via its own `#include` block (lines 19-21). These
are template-heavy but modest compared to sol2 (which is *why* `lua_runner.cpp` needs `/bigobj` —
confirmed in `src/CMakeLists.txt` line 65's comment and the `set_source_files_properties` on line
68, scoped to that one file only, not project-wide).

The existing pattern (`csv_read.h`/`csv_read.cpp`) already neutralizes this: `Reader` is Pimpl'd
specifically "so csv-parser's headers never have to be included by `lua_runner.cpp`" (confirmed —
`csv_read.h` lines 6-11, and `src/CLAUDE.md`'s File Map section repeats the rationale verbatim).
`csv_read.cpp` is its own translation unit and does **not** carry `/bigobj` in
`src/CMakeLists.txt` (only `lua_runner.cpp` does, lines 65-70) — confirming the Pimpl already
isolates the reader from that cost. If Option A were pursued despite the two disqualifying issues
above, the same Pimpl-into-its-own-`.cpp` pattern would isolate `csv_writer.hpp` from
`lua_runner.cpp` too, so `/bigobj` is not actually a decisive argument against Option A. It just
doesn't need to be raised, because Option A is already rejected on points 1 and 2.

## Recommended approach: hand-rolled writer

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| `std::ofstream` | C++20 stdlib | Owns the output file handle | Already the project's only file-write primitive; no wrapper needed |
| `std::to_chars` (`<charconv>`) | C++20 stdlib | Shortest-round-trip float/int formatting | Already used for exactly this in `database_csv_export.cpp` and in `lua_runner.cpp`'s own JSON encoder (`append_number`, line 129) — same file the writer binds into, so the pattern is a copy-paste, not a new one |
| Hand-rolled RFC-4180 quote/escape function | project code | Field quoting/escaping | ~20-line, well-understood algorithm (quote iff field contains separator/quote/CR/LF; double internal quotes) — smaller and more auditable than pulling three third-party headers to get the same 20 lines plus 250 lines of unrelated reader/SIMD/exception machinery |

No new FetchContent dependency. No CMake changes beyond adding the new source file(s) to
`QUIVER_SOURCES` in `src/CMakeLists.txt` (the same one-line addition `csv_read.cpp` already
required) — **no change to `cmake/Dependencies.cmake`, no new `FetchContent_Declare`, no new
link target.**

### Supporting Libraries

None. This is the "add nothing" case the quality gate asks to state plainly: the standard library
(`<fstream>`, `<charconv>`, `<string>`) is sufficient, and csv-parser — already vendored for the
reader — is unsuitable for the writer for the two verified reasons above, not merely "already
available so use it anyway."

### Development Tools

No change. Existing `format.bat`/`tidy.bat`/`.clang-format` cover new `.cpp`/`.h` files
automatically.

## Code volume estimate

Roughly **60-90 lines** for the writer core, comparable in shape (not lines) to `csv_read.h`
(69 lines) — smaller than `csv_read.cpp` (189 lines) because there is no ragged-row policy,
header-row heuristics, or past-EOF detection to replicate; a writer has no analogous ambiguity.
Breakdown:

- Field quote/escape decision + doubling (~20 lines) — the one piece of logic that must exactly
  match RFC-4180: quote iff the field contains the separator, the quote character (`"`), `\r`, or
  `\n`; double every internal `"`.
- Row assembly: join quoted/escaped cells with the runtime separator, terminate with `\n`
  (~10 lines).
- Numeric-to-string: **reuse**, don't reimplement — `append_number`/`append_json_double`
  (`lua_runner.cpp` lines 129-143) already do `std::to_chars` shortest-round-trip formatting with
  a `std::array<char, N>` stack buffer and non-finite → sentinel handling; the writer needs the
  string form, not JSON, so it is a small variant of the same helper, not new design.
- Handle/lifecycle glue: open, `write_row`, `close`, flush-on-destroy (~20-30 lines, mostly the
  sol2 usertype registration described below, not writer logic).

### RFC-4180 write-side edge cases a hand-rolled writer must get right

These are the ones easy to drop, cross-checked against what the *reader* already treats as
significant (`csv_read.cpp`'s own comments name several of these from the read side):

| Edge case | Correct write-side behavior |
|-----------|------------------------------|
| Field contains the separator | Quote the whole field |
| Field contains the quote character | Quote the whole field, double every internal quote (`"` → `""`) |
| Field contains CR or LF | Quote the whole field (unquoted embedded newlines break every downstream parser, including Quiver's own reader) |
| Field is empty string vs. `nil` | Both currently map to an empty, unquoted cell per `PROJECT.md`'s `nil` → empty-cell rule — there is no `""`-vs-empty distinction to preserve on the way out, only on the way in. Do not special-case an empty string into `""`; that would change round-trip shape for no reason and no test would justify it. |
| Field is exactly one quote character (`"`) | Must become `""""` (opening quote, doubled quote, closing quote) — an off-by-one in the doubling loop is the classic bug here |
| Separator is itself the quote character | `read_csv` already permits this combination (no cross-field validation between `separator` and the hardcoded `"` quote character); the writer's "does this field need quoting" check must still test for `"` even when `separator == '"'`, since the two purposes (delimiting vs. escaping) don't collapse into one just because the characters match |
| Separator is CR or LF | Same as above — accept it (matching the reader's permissiveness) but still quote any field containing it, since an unquoted separator-as-newline would be ambiguous on reread |
| Embedded NUL byte | `std::string`/`std::ofstream::write` are NUL-clean (length-prefixed, not C-string terminated) — no special handling needed as long as the writer never routes a cell through a C-string API (`strlen`, `%s`, etc.); Lua strings themselves are NUL-clean too, so this only breaks if someone reaches for `c_str()` semantics |
| Non-UTF-8 bytes | RFC 4180 is byte-oriented, not encoding-aware; a CSV *writer* has no reason to validate UTF-8 (contrast `lua_runner.cpp`'s JSON encoder, which validates UTF-8 because JSON itself requires it — CSV has no such requirement). Do not import that check by habit. |
| Leading/trailing whitespace in a field | RFC 4180 does not strip it, and neither should the writer — quoting is about the four special characters only, not whitespace. (Note this is the CSV writer's own contract, independent of Quiver's *DATE_TIME* trim policy elsewhere, which is a different string and a different layer.) |

## Buffered streaming: `std::ofstream`'s own buffering is sufficient

`std::ofstream` already buffers internally through its `std::filebuf` (an implementation-defined
buffer, commonly a few KB) as long as the code never forces a flush per row. The only two ways to
accidentally defeat that buffering are calling `.flush()` per row or using `std::endl` (which
flushes) instead of `'\n'` as the row terminator. Given that constraint is respected:

- **Build one row into a small, reused `std::string`** (row-local scratch buffer, cleared not
  reallocated between calls — same technique `lua_runner.cpp`'s JSON encoder already uses for
  `out`), then write it in one `ofstream::write(data, size)` call. This avoids the per-token
  `operator<<` sentry/locale overhead `std::ofstream::operator<<` carries (irrelevant for
  `std::string` payloads that are already fully formatted, but real for numeric `operator<<`,
  which is why `std::to_chars` into a local buffer, not `ofstream << double`, is the numeric path
  regardless).
- **Do not reimplement csv-parser's manual `batch_buffer_` (64 KB threshold, confirmed at line
  651)** on top of that. It exists in `DelimWriter` because `DelimWriter` can be constructed over
  an arbitrary `OutputStream` (including `std::stringstream`, which has no OS-level buffering of
  its own) — a generality Quiver's writer doesn't need, since it is always backed by a real file.
  Standard rule of thumb: syscall overhead (the thing buffering amortizes) becomes visible once
  a program issues on the order of thousands of unbuffered writes per second; `std::filebuf`'s
  default buffer already absorbs that for any per-row write in the tens-of-bytes-to-low-KB range,
  which is every realistic CSV cell/row here. The "GB-scale write path" is explicitly out of
  scope for this milestone (`PROJECT.md`, Out of Scope) — there is no performance target that
  would justify hand-rolling what the standard library already provides for free. If profiling
  ever shows the filebuf-default insufficient, `pubsetbuf` with a larger explicit buffer is a
  one-line escalation from the same `std::ofstream`, not a redesign.

This is the same "already covered" answer for I/O buffering that this document gives for quoting
and numeric formatting: reach for the standard library's built-in behavior before adding a manual
layer to replicate it.

## sol2 handle idiom: `w:close()` + `__gc` + LuaRunner teardown

Verified against the **existing precedent in this exact file** — `db:open_file` already returns a
`std::unique_ptr<BinaryFile>` from a `Database`-bound lambda (`lua_runner.cpp` lines 431-441), and
`BinaryFile` is registered `sol::no_constructor` with an explicit `"close"` method (lines 565-579)
and **no** explicit `sol::meta_function::garbage_collect` entry. That absence is the answer to
"is sol2's default `__gc` calling the destructor sufficient": **yes** — sol2 wraps the returned
`std::unique_ptr<T>` in userdata whose default `__gc` metamethod destroys the owned object exactly
once, whether that happens via an explicit `w:close()` path that nulls internal state first, or
via garbage collection / Lua-state teardown. `BinaryFile`'s own destructor already relies on this
same mechanism to flush/unregister a still-open write handle (`src/binary/binary_file.cpp`,
`~BinaryFile() = default`, with cleanup living in `close()` rather than the destructor for a
documented move-assignment reason — not relevant to the writer, which has no such reentrancy
concern).

The writer should follow the identical shape:

- `db:write_csv(path, opts)` is bound on the `Database` usertype (same place as `read_csv`/
  `read_csv_stream`, lines 452-525) and returns `std::unique_ptr<Writer>` (mirroring `open_file`'s
  `std::unique_ptr<BinaryFile>` return, line 434).
- The `Writer` class itself is registered via `lua.new_usertype<Writer>("Writer", sol::no_constructor,
  "write_row", ..., "close", ...)` — same `sol::no_constructor` pattern as `BinaryFile` (line 567)
  and `Expression` (line 624): the type is only ever constructed C++-side and handed to Lua as an
  opaque handle, never built from a Lua-side call.
- **No `sol::meta_function::garbage_collect` entry needed.** Rely on the class's own C++
  destructor (RAII: flush the buffer, close the `std::ofstream`) exactly as `BinaryFile` does —
  sol2's default `__gc` for a `unique_ptr`-owned userdata calls it. This satisfies the requirement
  "a writer still open when the script ends is flushed and closed" for free, for both the
  explicit-`close()`-forgotten case (script ends, Lua GC destroys the userdata, destructor runs)
  and the **LuaRunner teardown case** — `LuaRunner`'s owned `sol::state` closing at the end of
  `run()` (or on `LuaRunner`'s own destruction) triggers `lua_close`, which runs every live
  userdata's `__gc`, including any writer the script never explicitly closed. No separate teardown
  hook in `LuaRunner` is required beyond what already exists for `BinaryFile`.
- The one piece of *new* behavior beyond the `BinaryFile` precedent is the **warning log** the
  requirement asks for ("flushed and closed, with a warning logged") when a writer is destroyed
  still open. `BinaryFile`'s destructor has no such log today. That is a small, additive difference
  from the precedent (an `if (still_open) logger->warn(...)` guard in the destructor or in
  `close()`'s fallback path) — a phase-planning/implementation detail, not a stack decision; it
  needs the per-database `spdlog` logger (`root CLAUDE.md`'s Logging section, per-database
  instance, never the `spdlog::` globals) threaded to wherever the `Writer` is constructed, the
  same way every other `Database`-bound operation already has access to `self` (the `Database&`)
  in its lambda.
- API surface named in the requirement, concretely: `sol::usertype<Writer>` via `new_usertype`,
  `sol::no_constructor` (construction only from `db:write_csv`), no `sol::meta_function::
  garbage_collect` override (destructor suffices), destructor does flush+close, `"close"` method
  does the same thing idempotently (a second `close()` call, or the destructor running after an
  explicit `close()`, must not double-flush/double-close — `BinaryFile::close()`'s idempotency,
  guarded by its Pimpl's own open/closed state, is the model to copy).

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|--------------|
| `csv::DelimWriter<OutputStream, Delim, Quote>` / `csv::CSVWriter` / `csv::TSVWriter` (from the already-vendored csv-parser) | Delimiter/quote are compile-time template params — cannot represent Quiver's runtime `separator` without an instantiation switch that silently narrows the accepted alphabet below what `read_csv` already accepts | Hand-rolled quote/escape function taking `char separator` as a runtime value |
| `csv::internals::to_string<T>` for floating-point | Truncates/rounds to a hardcoded `DECIMAL_PLACES = 5` — the exact bug class already fixed once in `database_csv_export.cpp` | `std::to_chars` — already used for this in the same file (`lua_runner.cpp`'s `append_number`/`append_json_double`) |
| A new FetchContent dependency (any CSV-writer library) | Nothing on the market does less than what a ~20-line quote/escape routine already does correctly for RFC-4180's write side; every library adds a licence surface, a build-cost surface, and a versioning surface for code this small | Standard library (`<fstream>`, `<charconv>`, `<string>`) plus project code |
| A manual `batch_buffer_`-style accumulate-then-flush layer on top of `std::ofstream` | `std::filebuf`'s own internal buffering already amortizes syscall cost for row-sized writes; the manual version exists in `DelimWriter` only because it supports non-file `OutputStream`s Quiver doesn't need | `std::ofstream::write()` per row, never per-row `flush()`/`std::endl` |
| `pubsetbuf` tuning, non-default buffer sizes | No evidence of a real throughput requirement — "GB-scale write path" is explicitly out of scope this milestone | Default `std::ofstream` buffering; revisit only if profiling shows a concrete gap |
| An explicit `sol::meta_function::garbage_collect` override | Sol2's default `__gc` for a `unique_ptr`-owned userdata already calls the C++ destructor — the exact mechanism `BinaryFile` already relies on in this codebase | RAII destructor that flushes and closes |

## Version Compatibility

No new packages, so no new compatibility matrix. One fact worth recording for whoever revisits
this later: csv-parser has had **no release since 5.3.0** (verified against the GitHub releases
page — 5.3.0 is the newest tag) and the writer's compile-time-delimiter design and
`DECIMAL_PLACES`-based float formatter are unchanged in the version already vendored — there is no
newer tag to re-pin to that would resolve either issue. (The 3.5.1 changelog note "float
serialization: truncation replaced with correct rounding" predates 5.3.0 and only changes
*rounding direction* at the same fixed 5-decimal-place limit — it does not touch the disqualifying
part, the fixed precision itself.)

## Integration Points (for the plan-phase author)

- **`cmake/Dependencies.cmake`: no change.** No new `FetchContent_Declare`.
- **`src/CMakeLists.txt`: one addition** — a new source file (mirroring the `csv_read.cpp` line 13
  entry in `QUIVER_SOURCES`) if the writer lands in its own `.cpp`; if it lands header-only
  (plausible, since — unlike the reader — there is no third-party header to hide from
  `lua_runner.cpp`, so the Pimpl-for-header-isolation rationale behind `csv_read.h`/`.cpp` doesn't
  automatically apply), no new source-file entry at all, just a new header alongside
  `utils/string.h`/`utils/datetime.h`. Either way this is a file-organization call for plan-phase,
  not a stack decision — flagged here because `csv_read.h`'s Pimpl was for a reason (hide
  csv-parser headers) that a hand-rolled writer with zero third-party includes does not share.
- **No `/bigobj` interaction either way** — verified `/bigobj` is scoped to `lua_runner.cpp` alone
  via `set_source_files_properties` (`src/CMakeLists.txt` line 68), not project-wide, and a
  hand-rolled writer has no template depth remotely approaching sol2's regardless of which file it
  lives in.
- **sol2 binding site**: `bind_database()` in `lua_runner.cpp`, alongside `read_csv`/
  `read_csv_stream` (lines 452-525) and `open_file` (lines 431-441) — same sandboxing call
  (`resolve_sandboxed_path`) these already use, per the root `CLAUDE.md` design decision that every
  file-touching Lua operation is sandboxed through that one gate.
- **Agent-facing reference**: `bindings/js/src/lua-api.ts` (`LUA_DB_API_REFERENCE`) needs the new
  `db:write_csv`/`w:write_row`/`w:close` entries in the same phase — `lua-api-sync.test.ts` is a
  hard build gate per `PROJECT.md`'s target features list and root `CLAUDE.md`'s "Do Not Fix" /
  LuaRunner notes on that sync test.

## Sources

- `build/_deps/csv_parser-src/include/internal/csv_writer.hpp` (vendored csv-parser 5.3.0, read
  directly; line numbers cited throughout this document are from that file) — HIGH confidence,
  primary source read in full for the relevant sections.
- `cmake/Dependencies.cmake`, `src/CMakeLists.txt`, `src/csv_read.h`, `src/csv_read.cpp`,
  `src/CLAUDE.md`, `src/lua_runner.cpp`, `.planning/PROJECT.md` — all read directly from this
  repository. HIGH confidence, primary sources.
- GitHub releases page, `https://github.com/vincentlaucsb/csv-parser/releases` — confirmed 5.3.0
  is the newest tag, no writer-design changes in any release. MEDIUM-HIGH confidence (web fetch of
  the official releases page; not independently cross-checked against a second source, but the
  claim is a simple absence-of-newer-tag check with low risk of being wrong).

---
*Stack research for: Quiver v1.1 CSV writing for the Lua runner*
*Researched: 2026-09-16*
