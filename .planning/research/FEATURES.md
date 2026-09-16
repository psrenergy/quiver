# Feature Research

**Domain:** Streaming CSV writer for an embedded, sandboxed Lua runtime (`db:write_csv`)
**Researched:** 2026-09-16
**Confidence:** MEDIUM (every per-library claim below cross-checked against that library's own
official docs/source — docs.python.org, docs.rs, pkg.go.dev/go.dev source, papaparse.com,
csv.juliadata.org + JuliaData/CSV.jl source, IETF datatracker, cppreference/MSVC docs — not
recalled from training memory alone; MEDIUM rather than HIGH because these are web fetches of
primary sources, not the project's own vendored dependency, and Julia's write-side line-ending
default was not directly confirmed in any source found)

The shape (`db:write_csv(path, opts)` returning a handle with `write_row`/`close`, exactly two
options `separator`/`header`, `nil`→empty/boolean→`1`/`0`/table-function-userdata→error, strict
row width, `std::to_chars`-formatted numbers, auto-flush-and-close-with-warning on script end,
Lua-only) is **decided** (`.planning/PROJECT.md` Key Decisions, all "Pending" outcome but firm
scope). This document researches what that shape implies, not whether it's right.

## Feature Landscape

### Table Stakes (A Writer Must Get These Right Or It Is Broken)

| Feature | Consensus across Python `csv.writer`, Rust `csv::Writer`, Go `encoding/csv`, papaparse `unparse`, Julia `CSV.write` | Complexity | Notes for Quiver |
|---------|---|------------|-------|
| Minimal ("necessary-only") quoting | **Unanimous.** Python: `QUOTE_MINIMAL` is the writer default — quotes only a field containing the delimiter, quote char, or any char in the line terminator (`\r`/`\n`). Rust: `QuoteStyle::Necessary` — quotes only when the field contains a quote, delimiter, or record terminator (also forced for an all-empty single-field record, to disambiguate from a truly empty record). Go: `fieldNeedsQuotes` — same trigger set, minimal by construction, no opt-out to "always quote". papaparse: `quotes: false` by default *and* still auto-detects and quotes any field needing it regardless of that flag. Julia `CSV.write`: quotes only as needed unless `quotestrings=true` is passed explicitly. None of the five defaults to "always quote everything." | LOW | Already implied by "cell containing separator/quote/CR/LF" in the milestone's own quoting language. Quote trigger set: the configured `separator`, the quote character (`"`), CR, and LF — nothing else (not leading/trailing whitespace — see below). |
| Doubled-quote escaping inside a quoted field | Unanimous: `"` inside a quoted field is escaped as `""` (RFC 4180 doubling), not backslash-escaping, in every one of the five. | LOW | The vendored `csv-parser` reader already does this on the read side (`src/CLAUDE.md`); the writer must produce exactly what it consumes. |
| A field containing the delimiter/quote/CR/LF is quoted | Unanimous — this is the entire content of "minimal quoting" above, listed separately because it is the one non-negotiable behavior: any writer that fails this corrupts the file the moment a script writes `"May 1, 2014"`-shaped data (the exact real-world case that motivated this milestone — root PROJECT.md). | LOW | Direct test case already exists in the fixtures used for the reader (`ma_gd_data.csv`, quoted comma field) — the writer's regression test should round-trip that exact file. |
| Header written once, from the caller-declared list, not inferred | Split precedent, but converges on "explicit, not inferred" for a builder-style API: Rust's `WriterBuilder`/`has_headers` requires the caller to hand it an explicit header record; Python's `DictWriter.writeheader()` is an explicit, separate call the caller makes once. The auto-inferring end (Julia `CSV.write` derives column names from the table's own schema; papaparse infers a header from object keys when given an array-of-objects) only applies because those libraries are handed an already-schema'd, in-memory table — not applicable to a row-at-a-time streaming API with no upstream schema. | LOW (already decided) | Quiver's `header` option — declared once at `write_csv(path, opts)`, written immediately as row 1 — matches the Rust precedent closest: explicit list, written exactly once, never re-derived from data. |
| Row terminator written after every record including the last | Not RFC-mandated (RFC 4180: "the last record in the file may or may not have an ending line break" — verified against the RFC text itself), but every one of the five writers surveyed terminates every record it writes, the last one included — none special-cases "omit the terminator on the final row." | LOW | Simplifies the implementation: terminate unconditionally after every `write_row` (and after the header row), never track "is this the last call" — matches every real writer and is friendlier to `tail`/line-based tooling and to a future `append`-adjacent tool reading the file. |
| Line terminator choice does not need to match the reader | RFC 4180 specifies CRLF; **two of five default to LF instead** (Go explicitly: *"Writer uses LF instead of CRLF as newline character by default"* per its own docs; Rust's `csv::Writer` likewise defaults to `\n`, not the RFC's `\r\n`, per its own docs). Python and papaparse default to CRLF. No real consensus on CRLF vs LF for the *default*, only that both are accepted by real-world parsers. | LOW | Quiver's own `csv-parser`-backed reader is documented as handling both CRLF and LF (root PROJECT.md: v1.0 handled "CRLF" as a dirty-input case) — so whichever the writer emits, the reader consumes it. This means the choice is genuinely free; see Anti-Features for why it should still not be a knob. |
| No BOM emitted by default | Consensus across the general-purpose writers surveyed: BOM emission is opt-in or absent entirely (Python, Rust, Go, papaparse's `unparse` do not add one by default). The one prominent counter-example is Microsoft Excel's own "CSV UTF-8 (Comma delimited)" *export* feature, which does add a BOM automatically — a human-facing export target, not a general-purpose library default. | LOW | Quiver's writer output is consumed by another Lua script, another tool, or a human inspecting a case folder — not primarily double-clicked open in Excel. Emit no BOM, unconditionally (see Anti-Features — this should not be a knob either). |
| An empty string and a missing/null value cannot be distinguished on read-back | Structural fact of CSV, not a library choice: with minimal quoting, an empty field and an omitted field render identically (two consecutive delimiters, or a delimiter followed immediately by the terminator), and no reader — including Quiver's own `db:read_csv` — has any signal to tell them apart. Julia's `CSV.write` defaults `missingstring=""` — i.e. it deliberately collapses `missing` into the same empty-string representation the milestone's `nil`→empty rule already chose. | N/A (structural) | This is not something to fix — see Round-Trip Fidelity below for the concrete implication. |

### Differentiators (Real Value, Genuinely Optional)

| Feature | Value Proposition | Complexity | Recommendation |
|---------|--------------------|------------|-----------------|
| Idempotent `close()` (calling it twice is a harmless no-op) | Every general-purpose file-handle API surveyed treats a second `close` as a no-op rather than an error (Python: closing an already-closed file object is a no-op). Cheap insurance against a script that calls `close()` defensively in a `pcall`-guarded cleanup path *and* relies on the "unclosed writer at script end is auto-closed" fallback also firing. | LOW | **Adopt**, in place of a separate `is_open()` predicate (see Anti-Features) — it gets the same safety property (no "double-close" crash) for zero added documented surface, since it changes `close()`'s behavior rather than adding a method. |
| Explicit `flush()` | Real gotcha in the reference libraries: Rust's `csv::Writer` wraps a buffered writer and its `Drop` impl attempts a flush but silently swallows the error — the documented advice is to call `.flush()` explicitly before relying on file contents. | LOW to add, but a new permanent doc line | **Skip for v1.1.** The milestone's own "unclosed writer at script end is flushed and closed, with a warning logged" decision already covers the scenario `flush()` exists for elsewhere (a script that dies partway through still leaves readable partial output) — that decision fires regardless of *how* the script ended (normal return, Lua runtime error, or host-side abort), so there is no remaining gap `flush()` would close for this API's actual usage pattern (one script, one file, no other process reading it mid-write). Revisit only if a concrete case needs another process to tail the file while the writer is still open. |
| Row-count return value from `write_row`/`close` | Superficially convenient bookkeeping. | LOW to add, permanent doc cost | **Skip.** None of the five surveyed writers expose a row-count getter as a feature — it's understood to be the caller's own loop-counter concern, and it already is one here: `for _, id in ipairs(...) do w:write_row(...) end` — the script already knows `#ids`. Not worth a permanent prompt-token line for something one local variable already gives the script. |
| Atomic write-then-rename (write to a temp path, rename into place on `close()`) | Common pattern in data-pipeline / config-writer tooling generally (not baked into any of the five CSV libraries themselves — it is an application-level wrapper in every ecosystem surveyed, never a CSV-writer-library feature). Would guarantee a reader never observes a half-written file. | MEDIUM (temp-path-in-sandbox handling, rename-across-filesystem edge cases on the three target OSes) | **Argue against, not just skip.** This would actively *contradict* the milestone's own decision that "a writer still open when the script ends is flushed and closed, with a warning logged" — that decision makes a half-written file, plus a visible warning, the intended outcome of a script that errored partway through (the human/agent can see and use the partial output, rather than it vanishing or never appearing). Atomic rename would instead either hide the partial file entirely (bad — the warning becomes meaningless, there is nothing to point to) or require inventing a second "partial-write" naming convention nobody asked for. Not a complexity trade-off, a direct conflict with an already-made decision. |

### Anti-Features (Commonly Offered, Argued Against Here)

Every item below costs a permanent line in `LUA_DB_API_REFERENCE`, shipped into every `claw`
session forever (`.planning/PROJECT.md` Constraints — "Prompt weight"). The bar is not "would
this be nice" but "does this pay for its permanent token cost in this specific one-script,
one-case-folder, LLM-authored-Lua context."

| Anti-feature | Why it looks appealing (and is real in the surveyed libraries) | Why it does not earn its permanent cost here | What to do instead |
|---|---|---|---|
| `append` mode | Real file-open-mode concept; every ecosystem supports it (Python: `open(path, 'a')` + `csv.writer`). Already explicitly rejected in PROJECT.md. | None of the five bake append-with-CSV-semantics *into the writer itself* — it's the caller's file-open mode, orthogonal to the writer, in every one of them. Quiver's `write_csv` resolves and creates the file itself (sandboxed), so "append" would require Quiver to invent answers no reference library gives for free: does a second `header` re-write duplicate a header row, or get silently skipped, or error? A `claw` session is one script writing one file once — the recurring-append use case (accumulating a log across many separate script runs) doesn't fit the one-script-one-file mental model this API targets. | If cross-run accumulation is ever needed: the script can `read_csv` the existing file's rows into memory, then `write_csv` the union — expensive on a huge file, but that case hasn't occurred yet (same "no concrete case" reasoning PROJECT.md used to defer writing itself in v1.0). |
| `line_ending` (CRLF vs LF toggle) | Real disagreement among the five surveyed writers (Python/papaparse default CRLF; Go/Rust default LF) — genuinely looks like something that needs to be a choice. Already explicitly rejected in PROJECT.md. | The disagreement is exactly why a *default* can be picked without a knob: Quiver's own `csv-parser`-backed reader already accepts both LF and CRLF (documented CRLF-handling in the v1.0 dirty-input list), so whichever the writer picks, `db:read_csv` never chokes on its own output. The audience is another script or tool consuming the case-folder file, not a human double-clicking it into Excel (which is the only place CRLF specifically matters for compatibility). One fixed, defensible default (LF — smaller, matches the two writers, Go and Rust, that ship in a systems-programming context closest to this one) removes the need for the knob entirely. | Emit LF unconditionally, document it in one sentence, no option. |
| Configurable quote character / escape character | Real, supported knob in Python (`quotechar`, `doublequote`), Rust (`quote`, `escape`), Julia (`quotechar`, `escapechar`) — for sources that aren't `"`-quoted RFC 4180. | `db:read_csv` — the counterpart this writer must stay symmetric with — exposes **no** quote-character option at all (only `separator`/`header_row`). Adding one to the writer that the reader doesn't have would be an asymmetry with no matching read-side use, and no case in this project's history (the dirty real-world fixtures) has ever used a non-`"` quote character. | Hardcode `"` as the quote character and RFC-4180 doubling as the escape, exactly like the reader already does implicitly. |
| Quoting-mode selector (Python's 4-way `quoting=`, Rust's `QuoteStyle::{Always,Never,NonNumeric,Necessary}`) | Real feature in two of five surveyed writers. `QUOTE_NONNUMERIC`/`NonNumeric` exists specifically to let a *reader* re-infer which cells were originally numbers. | `db:read_csv` deliberately returns every cell as a plain string with **no numeric or date inference** (documented in `bindings/js/src/lua-api.ts`) — so a quoting mode that exists purely to help a downstream reader distinguish "was a number" from "was a numeric-looking string" has no consumer on this project's read side. `Always` (quote everything) only matters for human readability or working around one specific downstream parser's quirks — neither applies to a machine-to-machine case-folder file. | Minimal quoting only, no mode. |
| Dialect presets (Python's named `"excel"`/`"excel-tab"`/`unix_dialect`) | Real DX feature for a general-purpose library serving many CSV "flavors." | `separator = "\t"` already **is** the excel-tab dialect; there is exactly one other axis to vary (the separator), already covered by the one option that exists. A preset system is pure indirection over a single character. | Nothing to add — `separator` already covers this. |
| Output encoding option (non-UTF-8 output) | Real option in general-purpose writers whose callers may target legacy systems (Python's `open(..., encoding=...)`). | Every other layer of this project is already UTF-8-only by explicit design — the Lua JSON return-value encoder validates UTF-8 and rejects anything else (`src/CLAUDE.md`, `LuaRunner::run`). Adding an encoding knob to the writer would contradict a stance already taken everywhere else in the stack, for an audience (LLM-authored Lua) with no legacy-encoding need. | UTF-8 output only, unconditionally, matching the rest of the project. |
| BOM-emission toggle | Real, named use case: Excel's own CSV-UTF-8 export path adds one automatically so double-clicking the file in Excel renders accented characters correctly. | The target reader here is `db:read_csv`/another script/a human `cat`-ing the file in a terminal, not Excel-by-double-click; none of the five general-purpose libraries surveyed default to emitting one, and a stray BOM byte silently becomes part of the *first header cell's* text for any reader that doesn't specifically strip it (a real footgun, not just noise). | Never emit a BOM. No option — matches the "no knob needed, defensible fixed default" reasoning used for line endings above. |
| `is_open()` / `closed` predicate | Real property on Python file objects (`.closed`) and useful for scripts that want to guard a `close()` call. | Gets the identical safety property from making `close()` itself idempotent (see Differentiators) at zero added documented surface — a query method plus documentation of what it means before/after close is strictly more prompt weight for the same outcome. | Idempotent `close()`; no query method. |

## Feature Dependencies

```
db:write_csv(path, opts) returns a writer handle
    └──requires──> filesystem sandbox (resolve_sandboxed_path) — already exists, shared with
                    db:read_csv/db:read_csv_stream/db:open_file/db:bin_to_csv/db:csv_to_bin/
                    db:export_csv/db:import_csv/db:validate_migrations/expr:save
    └──requires──> a Lua value→CSV-cell converter (string/number/boolean/nil/error-on-else)
                       └──requires──> numeric formatting via std::to_chars with the int64
                                       overload used directly for a Lua integer (not routed
                                       through the double path first) — see Round-Trip Fidelity
    └──requires──> row-width enforcement
                       └──depends-on──> whether `header` was supplied:
                              header given  → width = #header, fixed at open time
                              header absent → width = first write_row's cell count, fixed then
                                              (open design question — see below)
    └──requires──> the minimal-quoting/escaping encoder (separator/quote-char/CR/LF triggers)

`header` option ──enhances──> row-width enforcement (gives it a definite width before any data
                                row is seen, closing the "row 1 silently sets a wrong width" gap
                                that the no-header path cannot close — see below)

Script-end auto-close-and-warn ──requires──> the writer handle's lifetime being tracked by
                                              LuaRunner across the whole script run (same
                                              lifetime pattern already used for BinaryFile's
                                              write registry in the binary subsystem, src/CLAUDE.md)
```

### Dependency Notes

- **The sandbox gate is a hard prerequisite, already built.** `write_csv` slots into the existing
  `resolve_sandboxed_path` single gate (`src/lua_runner.cpp`) alongside every other file-touching
  Lua operation — no new sandbox logic, just one more caller naming `"write_csv"` as its operation
  string for Pattern 1 messages.
- **Numeric formatting must not go through the double path for integers.** This is not merely a
  style preference — see the sharp round-trip failure mode below (a large int64 losing precision
  if converted to `double` before formatting). The int64 `std::to_chars` overload must be selected
  directly whenever the Lua value is the integer subtype, never `(double)value` first.
- **Row-width enforcement for the no-header case depends on solving the "which row fixes the
  width" question below before implementation** — it is the one piece of the feature set the
  milestone context itself flags as unsettled ("An open edge for planning" in PROJECT.md).

## The No-Header Row-Width Question

**What other writers do when there is no header, surveyed:**

| Writer | Behavior with no declared header/schema |
|---|---|
| Python `csv.writer` | No validation at all, ever — ragged rows fully allowed. Each `writerow(seq)` call writes exactly the given sequence's length; the module has no concept of "the" row width. |
| Go `encoding/csv` `Writer.Write([]string)` | Same — fully permissive, no built-in width tracking or validation. |
| papaparse `unparse` (array-of-arrays input, no `header` option) | No validation; ragged arrays render as ragged rows. |
| **Rust `csv::Writer`** (`flexible` defaults to `false`) | **The first record written — with or without headers — fixes the expected field count for the rest of the file.** Any later record of a different length raises `UnequalLengths` (position of the offending record, expected count from the first record, actual count found) — verified directly against `docs.rs/csv` and the crate's own error-type source. An explicit `.flexible(true)` builder call opts back into the fully-permissive behavior. |
| Julia `CSV.write` | Not directly applicable — it always writes from an already-schema'd `Tables.jl` source, so "no header" never means "no known width" the way it does for a row-at-a-time streaming API. |

Two camps: three writers are fully permissive (schema-free by design, matching those languages'
generally dynamic/loose CSV story), and one — the one architecturally closest to what's being
built here (a builder-configured, streaming, statically-typed writer with an explicit strict/
flexible toggle) — fixes the schema from the first row written and enforces it strictly by
default.

**Recommendation: mirror Rust's default.** When no `header` option is given, the cell count of
the **first `write_row` call** becomes the writer's fixed expected width for the remainder of the
file; every subsequent `write_row` is checked against it with the same Pattern 1 mismatch error
the header path already uses (row ordinal + both counts). This is the option that matches the
milestone's own already-decided philosophy for the header case — "strict row width... catches an
LLM-authored loop that drops a field at the row that dropped it" (PROJECT.md) — rather than the
permissive camp, which is exactly the failure class that decision exists to prevent. It also
requires no new concept: "width" is still just "a fixed integer determined once," only its source
(the `header` option vs. the first row) differs.

**Failure modes this recommendation must be documented against, not silently accepted:**

1. **Zero rows written.** If `close()` is called with no `write_row` calls and no `header`, there
   is no width to have ever fixed — trivially correct (an empty, or header-only, file); no special
   case needed in the encoder, just note it so a test exists.
2. **A single-row file has no independent schema to validate against.** Whatever width row 1
   happens to have silently becomes "the" width with nothing to catch a mistake *in* that row —
   the header path does not share this gap (the header itself is declared before any data row
   exists to be checked against it). Worth one sentence in the reference: callers who want the
   validation benefit, not just the mechanical benefit, should pass `header`.
3. **The sharpest failure mode: `nil`-shaped trailing cells on row 1 specifically.** Because
   `nil`→empty cell means a Lua row table can have holes, and `#t` is undefined on a table with
   trailing holes, a call like `w:write_row({1, "x", nil})` — meant as a 3-column row whose third
   value happens to be nil for this particular id — risks reporting width 2 instead of 3
   (Lua's length operator is free to pick either border on a table with a trailing hole). If this
   happens to be row 1 specifically, the *wrong* width (2, not 3) gets locked in as the file's
   schema, and every later row that legitimately has a non-nil 3rd value then throws a mismatch
   error that blames row 2 (or later) for a mistake actually made on row 1 — a genuinely confusing
   error message pointing at the wrong line. This is strictly worse than the header case, where the
   same nil-hole ambiguity can only ever affect the row it happens on, never silently redefine the
   schema for every row after it.
4. **Consequence of (3): document, in the same reference section, that `write_csv` without
   `header` is a completeness/parity feature (mirroring `read_csv`'s `header_row = 0`), not the
   expected common case** — a `claw` script almost always knows its column names (it wrote the
   loop), so steering toward `header` sidesteps this failure mode entirely rather than needing to
   solve it more cleverly.

**Alternatives considered and rejected:**

- **Fully permissive (Python/Go/papaparse style), no width tracking without a header at all** —
  rejected: reintroduces exactly the "a dropped field is silently written, some downstream tool
  chokes on it later, far from the actual mistake" bug class that the header-case strict-width
  decision exists to prevent (PROJECT.md's own stated rationale). Inconsistent within one API to be
  strict only when `header` happens to be supplied.
- **Require `header` unconditionally (reject `write_csv` calls that omit it)** — rejected: breaks
  symmetry with `read_csv`'s optional `header_row = 0`, and removes a legitimate use case (a
  single unlabeled positional column, or a file whose meaning is obvious from context) for close
  to zero implementation savings over "first row fixes it."

## Round-Trip Fidelity

**What `read_csv(write_csv(x)) == x` requires, and where it structurally cannot hold:**

- **Equality can only be asserted at the string level, never the type level.** `db:read_csv`
  returns every cell as a plain Lua string with no numeric/date inference (already documented,
  unchanged by this milestone). So the correct round-trip property is `read_csv(path).rows[i][j]
  == expected_string_form(x[i][j])`, not `== x[i][j]` — a Lua number `2014` written by `write_csv`
  and read back by `read_csv` is the *string* `"2014"`, not the number `2014`.
- **`nil` and `""` are indistinguishable after the round trip — this is structural, not a bug to
  fix.** `write_row({1, nil, 3})` and `write_row({1, "", 3})` produce byte-identical output (two
  consecutive separators either way, per the minimal-quoting table-stakes rule above), so both
  read back as `{"1", "", "3"}`. Every CSV writer surveyed has this same limitation — CSV has no
  representation for "absent" distinct from "empty" (Julia's own `missingstring=""` default
  collapses the two on purpose, for the same reason). Document this explicitly as the one
  intentionally lossy conversion, not an open bug.
- **Numeric formatting must guarantee `tonumber(read_back_string) == original_number`, not
  textual equality with an assumed format.** `std::to_chars`'s default (no explicit `chars_format`)
  is defined to produce the *shortest* string such that parsing it back recovers the exact
  original value (verified against cppreference/the standard's own wording) — so `0.1` round-trips
  as `"0.1"`, never `"0.1000000000000000055511151231257827021181583404541015625"` (naive
  `%.17g`-style over-precision) nor a lossy truncation. The testable property is
  `tonumber(cell) == original` for floats, not a fixed string comparison.
- **The Lua integer/float distinction matters most at magnitudes a `double` cannot represent
  exactly — this is the sharpest test to write.** `std::to_chars`'s shortest-round-trip default
  already renders a whole-number *double* like `2014.0` as `"2014"` with no trailing `.0` — so for
  small values, using the int64 overload directly vs. converting the Lua integer to `double` first
  produces the *same visible string* most of the time, and the "integer subtype preserved" rule
  can look redundant on small numbers alone. It stops being redundant exactly at IEEE-754's
  53-bit exact-integer boundary: an int64 like `9007199254740993` (2^53 + 1) printed via the int64
  path is exact, but the same value first converted to `double` silently rounds to
  `9007199254740992` before any formatting even happens — a corrupted round-trip a naive
  "always format as a Lua number" implementation could easily introduce by routing every numeric
  cell through the double formatter. **Recommend this exact case —
  `write_row({9007199254740993})` must read back the string `"9007199254740993"`, not
  `"9007199254740992"` — as a named regression test**, not just a general "integers preserved"
  smoke test.
- **A string that merely looks like a number must round-trip byte-for-byte, unreformatted.**
  `"0012"`, `"1e10"`, `"007"` written as Lua *strings* (not numbers) must come back exactly as
  written — the writer never parses or reformats a string cell, only formats actual Lua numbers.
  This is the direct payoff of rejecting the `QUOTE_NONNUMERIC`-style anti-feature above: a writer
  that tried to "help" by distinguishing numeric-looking strings from real numbers would be exactly
  the kind of reformatting that breaks this property. Worth a dedicated test:
  `write_row({"0012"})` → `read_csv(...).rows[1][1] == "0012"`.
- **A Lua boolean round-trips as an ordinary numeric-looking string, indistinguishable from an
  actual integer 1 or 0** — matches the project-wide policy that Lua has no boolean readers
  (root CLAUDE.md). `write_row({true, 1})` must produce two cells that both read back as `"1"`;
  this is expected, not a gap, but worth asserting explicitly so a future change doesn't
  accidentally special-case booleans into `"true"`/`"false"`.
- **Special-character cells are the highest-value round-trip test given this project's own
  history.** A cell equal to the separator, a bare `"`, a CR, or an LF (singly and combined, e.g. a
  comma-and-newline-containing string) must survive `write_csv` then `read_csv` byte-identical —
  this is the exact bug class (`"May 1, 2014",33`) that motivated the reader in v1.0 and is
  precisely what minimal quoting exists to guarantee on the write side.
- **Leading/trailing whitespace in an unquoted-but-round-tripped cell is not guaranteed by RFC
  4180 or by any of the five writers surveyed** — none of them quotes a field solely because it
  has leading/trailing spaces (only delimiter/quote/CR/LF trigger quoting), and RFC 4180 is silent
  on whether a conforming reader should trim unquoted whitespace. Quiver's own `csv-parser`-backed
  reader has not been observed to trim (nothing in `src/CLAUDE.md`'s pinned `CSVFormat` settings
  mentions trimming), so this project's own round trip should hold — but it is worth one explicit
  test (`write_row({" x "})` → reads back `" x "` unchanged) specifically because it is the one
  property that is *not* guaranteed by the format itself, only by this specific reader/writer pair
  agreeing not to trim.

**Recommended round-trip property tests for the plan** (each maps to a bullet above):

1. Header + N mixed-type data rows → read back → header matches, every cell equals the expected
   string form (`""` for nil, `"1"`/`"0"` for boolean, `tostring`-equivalent for numbers,
   verbatim for strings).
2. Separator, quote char, CR, LF — each alone and combined in one cell — byte-identical round trip.
3. `9007199254740993` (2^53 + 1) as an integer cell — exact string round trip, not
   `9007199254740992`.
4. `"0012"` / `"1e10"` / `"007"` as string cells — byte-identical, unreformatted.
5. `true`/`false` cells — read back as `"1"`/`"0"`, indistinguishable from literal integers.
6. `nil` cell vs. explicit `""` cell — assert they are (deliberately) indistinguishable on read
   back, as a documentation-locking test rather than a bug regression test.
7. Leading/trailing-whitespace-only string cell — byte-identical round trip.
8. No-header write, first row's width silently becomes the schema (see previous section) — a
   mismatch on row 2+ throws; a matching row 2+ succeeds.

## Interaction With `db:read_csv`'s Existing Option Surface

`separator` (both sides, same default `,`, same single-character-string type) is fully symmetric
— no issue.

`header_row` (reader: 1-based integer, `0` = "no header," selects *where in an existing file* the
header line sits) vs. `header` (writer: a list of column names, selects *what to write* as the
header) look asymmetric at first glance but are not a naming inconsistency to fix — they answer
genuinely different questions that only make sense on their respective side:

- Reading has to cope with **noise the file might already contain** (a junk title row, a units
  row, duplicate/blank header names — all real cases in the v1.0 fixtures) — hence a *position*
  parameter, because the header the caller wants might not be the file's first line.
- Writing has **no such noise to locate**, because the writer is creating the file from nothing —
  there is nothing above row 1 to skip, so a position parameter would be meaningless; `header` is
  necessarily *content* (the names themselves), always written as literally the first line.

The one real risk is not the names differing, but an LLM author skimming the two option tables
and assuming `header_row`'s semantics (a number, `0` disables it) also apply to the writer's
`header` (a list, its absence disables it). **Recommendation for the doc, not the API:** place the
"CSV file writing" reference section immediately after "CSV file reading" (as `read_csv`/
`read_csv_stream` already are relative to each other) and add one explicit cross-reference
sentence — "unlike `read_csv`'s `header_row` (a position), `write_csv`'s `header` is the column
names themselves, written once as row 1" — so the asymmetry is named rather than left for the
reader to notice. This is a documentation-ordering recommendation, not a design change; the two
options as decided are already correctly shaped for what each side needs.

## MVP Definition

### Launch With (v1.1 — already scoped, listed here for dependency completeness)

- [ ] `db:write_csv(path, opts)` → writer handle, sandboxed — foundation for everything else
- [ ] `w:write_row({...})` — value-to-cell conversion (string/number/boolean/nil/error) +
      minimal-quoting encoder
- [ ] `w:close()` — idempotent (see Differentiators), flush-on-close
- [ ] Row-width enforcement, both the `header`-declared and first-row-implied cases
- [ ] Script-end auto-flush-and-close-with-warning for an unclosed handle
- [ ] `std::to_chars`-based numeric formatting, int64 overload selected directly for Lua's
      integer subtype (not via `double`)
- [ ] `LUA_DB_API_REFERENCE` update in the same phase (hard build gate, `lua-api-sync.test.ts`)

### Explicitly Not In Scope (per PROJECT.md, reinforced by this research)

- [ ] `append` — see Anti-Features
- [ ] `line_ending` — see Anti-Features
- [ ] Any quote-character, escape-character, or quoting-mode option — see Anti-Features
- [ ] BOM emission (in any form, default or optional) — see Anti-Features
- [ ] `is_open()` — replaced by idempotent `close()`
- [ ] Row-count return value — trivial for the caller to track itself
- [ ] Atomic write-then-rename — actively conflicts with the "partial file + warning" decision

### Future Consideration (only if a concrete case emerges, per the project's own precedent for
deferring v1.0's writer in the first place)

- [ ] Explicit `flush()` — only if a case emerges where another process needs to read the file
      while the writer is still open (no such case exists today)
- [ ] Cross-run append semantics — only if a case emerges needing accumulation across separate
      `claw` script runs on the same file

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---|---|---|---|
| Minimal quoting/escaping | HIGH | LOW | P1 |
| Strict row width (header-declared) | HIGH | LOW (decided) | P1 |
| Strict row width (no-header, first-row-implied) | MEDIUM | LOW | P1 |
| `nil`→empty / boolean→1-0 / error-on-table-function-userdata | HIGH | LOW (decided) | P1 |
| `std::to_chars` numeric formatting, int64 direct path | HIGH | LOW | P1 |
| Idempotent `close()` | MEDIUM | LOW | P1 |
| Auto-flush-and-close-with-warning on script end | HIGH | MEDIUM (lifetime tracking) | P1 |
| No-BOM / LF-only fixed defaults (no knobs) | MEDIUM | LOW (nothing to build) | P1 |
| Explicit `flush()` | LOW today | LOW | P3 (deferred) |
| Row-count getter | LOW | LOW | Rejected |
| Atomic write-then-rename | LOW here (conflicts with a decision) | MEDIUM-HIGH | Rejected |
| `append` mode | LOW (no concrete case) | MEDIUM (semantics undefined) | Rejected |
| `line_ending` option | LOW (defensible fixed default exists) | LOW | Rejected |
| Quote-char/quoting-mode options | LOW (no reader-side consumer) | MEDIUM | Rejected |

## Sources

- Python `csv` module docs — dialect defaults (`QUOTE_MINIMAL`, `lineterminator = '\r\n'`,
  `doublequote`): https://docs.python.org/3/library/csv.html
- Rust `csv` crate — `WriterBuilder`/`QuoteStyle::Necessary` default, `Terminator` default `\n`
  (not RFC's `\r\n`), `flexible`/`UnequalLengths` first-record-fixes-width behavior:
  https://docs.rs/csv/latest/csv/struct.WriterBuilder.html ,
  https://docs.rs/csv/latest/csv/enum.QuoteStyle.html ,
  https://github.com/BurntSushi/rust-csv/blob/master/src/error.rs
- Go `encoding/csv` — `Writer` defaults to LF not CRLF, `UseCRLF` field, `fieldNeedsQuotes`:
  https://pkg.go.dev/encoding/csv , https://go.dev/src/encoding/csv/writer.go
- papaparse `unparse` — default `quotes: false` with auto-detection, default `newline: "\r\n"`:
  https://www.papaparse.com/docs
- Julia `CSV.jl` — `CSV.write` defaults (`quotechar`, `missingstring = ""`, `quotestrings`,
  `escapechar`): https://csv.juliadata.org/latest/writing.html ,
  https://github.com/JuliaData/CSV.jl/blob/main/src/write.jl
- RFC 4180 (IETF) — CRLF specified, final record's line break optional:
  https://datatracker.ietf.org/doc/html/rfc4180
- `std::to_chars` default floating-point format — shortest round-trip guarantee, no forced
  trailing zero: https://en.cppreference.com/cpp/utility/to_chars ,
  https://learn.microsoft.com/en-us/cpp/standard-library/charconv-functions
- UTF-8 BOM / Excel CSV interop — BOM opt-in in general-purpose tools, Excel's own "CSV UTF-8"
  export path adds one automatically:
  https://support.microsoft.com/en-us/excel/opening-csv-utf-8-files-correctly-in-excel
- Project sources: `.planning/PROJECT.md` (milestone scope, decisions, constraints),
  `bindings/js/src/lua-api.ts` (existing `db:read_csv`/`db:read_csv_stream` reference language),
  `src/csv_read.h`, `src/CLAUDE.md` (reader internals, sandbox mechanics, `to_chars` precedent in
  `database_csv_export.cpp`/`lua_runner.cpp`), root `CLAUDE.md` (boolean write policy, Lua
  no-boolean-readers policy, error message patterns).

---
*Feature research for: Quiver `db:write_csv` (v1.1 milestone)*
*Researched: 2026-09-16*
