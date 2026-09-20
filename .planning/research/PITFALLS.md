# Pitfalls Research

**Domain:** Brownfield C++ library milestone — a TOML sidecar reader threaded through a C API and five FFI bindings (Quiver, UI Metadata Layer)
**Researched:** 2026-09-17
**Confidence:** HIGH (two agent surveys over the real corpus + two adversarial verifications; the JS/CHANGELOG hazards re-verified directly in `C:/Development/Quiver/quiver3` while writing this)

Phase labels below use the seams proposed by the larger survey's synthesis brief and consistent with
PROJECT.md's Key Decisions: **P1** enum labels in `describe` on convention, no ABI change ·
**P2** `ui_config_path` + `ui_locale` on `DatabaseOptions` (the ABI break) · **P3** structured
`get_ui_metadata` getter · **P4** collection- and group-level metadata · **P5** `validate_ui_config()`.
Rename freely; the *ordering constraints* are what matter.

---

## Critical Pitfalls

### Pitfall 1: The silent memory corruption — JS `makeDefaultOptions` hardcodes an 8-byte options struct

**What goes wrong:**
`bindings/js/src/ffi-helpers.ts` L10-16 allocates `new Uint8Array(8)` for
`quiver_database_options_t` and writes `setInt32(0, readOnly)` / `setInt32(4, consoleLevel)`. Adding
`ui_config_path` + `ui_locale` grows that struct to **24 bytes** (`int`, `int`, `const char*`,
`const char*`, with padding). The native side then reads — and on any out-direction use, writes —
16 bytes of adjacent JS heap beyond a buffer Bun owns. No exception, no compile error; the symptom
is a corrupted unrelated JS value or a crash far from the call, in `database.ts` L21/39/58
(`fromSchema` / `fromMigrations` / `open`).

**Why it happens:**
Every other binding has a generated or declared struct to fall back on. **JS does not**: Bun cannot
call `quiver_database_options_default` because it returns a struct by value (bun#6139), so the
layout exists *only* as this hand-written literal, and nothing in the build cross-checks it against
`include/quiver/c/options.h`. There is no JS generator at all (`bindings/js/CLAUDE.md`: symbols are
hand-added to `src/loader.ts`).

**How to avoid:**
- Do not touch `DatabaseOptions` in P1 at all. P1 finds the UI dir by convention (`<db_dir>/ui/`),
  which is correct for **100% of the corpus** — every one of the 13 models puts it exactly there.
  That removes this hazard from the phase that delivers the whole triggering value.
- P2 gets its **own plan** for this one file plus a runtime size assert: add a C API accessor that
  returns `sizeof(quiver_database_options_t)` (a plain `size_t`, which Bun *can* call) and assert it
  against the JS constant at load, so the next growth fails loudly in every binding that can call it.
- Name the offsets as constants next to the buffer and comment the field order, as
  `bindings/js/src/metadata.ts` does.

**Warning signs:**
- A diff that adds a field to `quiver_database_options_t` without a matching hunk in
  `bindings/js/src/ffi-helpers.ts`.
- JS tests that pass but leave stray failures in *other* JS suites, or intermittent Bun crashes with
  no stack in Quiver code.
- **Careless-grep trap, verified in-repo:** `ffi-helpers.ts` contains **three** `new Uint8Array(8)`
  — L11 (the options struct), L20 (`allocPtrOut`), L33 (`allocUint64Out`). Only the first changes.
  A search-and-replace over "8" in this file is itself the bug.

**Phase to address:** P2, with a dedicated plan. P1 must be designed so it never reaches this file.

---

### Pitfall 2: Same class — `SCALAR_METADATA_SIZE` / `GROUP_METADATA_SIZE` are out-buffer allocations, not read strides

**What goes wrong:**
`bindings/js/src/metadata.ts` L30-31 declares `SCALAR_METADATA_SIZE = 56` and
`GROUP_METADATA_SIZE = 32`. These are used two ways: as the stride when walking a C array (L62,
L168, L183, L195, L210) **and as the buffer handed to C as an out-parameter** (L81, L96, L111,
L126 — `const outBuf = new Uint8Array(SCALAR_METADATA_SIZE)` then
`lib.quiver_database_get_scalar_metadata(..., outBuf)`). Growing `quiver_scalar_metadata_t` to
carry UI fields turns a stale constant into a **native write past a JS-owned `Uint8Array`**, not
merely a misread. `bindings/js/src/csv.ts` L24 has the same shape (`new Uint8Array(56)` for
`quiver_csv_options_t` with hand-written offsets 0/8/16/24/32/40/48).

**Why it happens:**
It is invisible in review: the constant looks like a decoding detail, and the C signature takes an
opaque buffer, so neither side type-checks the other.

**How to avoid:**
PROJECT.md already settles this — **do not add fields to `ScalarMetadata` / `GroupMetadata`.** Ship
a new `quiver_ui_metadata_t` with its own size constant and its own `quiver_database_free_ui_metadata`.
Nothing existing changes size, JS adds one new constant, and the enrichment stays off
`list_scalar_attributes`, which would otherwise carry an enum map per column on every whole-collection
read.

**Warning signs:**
Any PR that edits `include/quiver/attribute_metadata.h` or the `quiver_scalar_metadata_t` /
`quiver_group_metadata_t` typedefs in `include/quiver/c/database.h`. Treat that edit as a stop sign
for this milestone.

**Phase to address:** P3 (the getter's C API shape). The decision is load-bearing before a line of
C API code is written.

---

### Pitfall 3: Stale Python CFFI cdef — ABI mode corrupts silently, with no compile error

**What goes wrong:**
`bindings/python/src/quiverdb/_c_api.py` declares the structs (options at L27-30, plus L192-204 and
L378-384) in **CFFI ABI mode**: the cdef *is* the layout, and CFFI never sees the real header. If
`quiver_database_options_t` grows and the cdef does not, Python allocates the old size and hands it
to C — same corruption as Pitfall 1, with a passing test suite. `bindings/python/CLAUDE.md` states
the failure mode outright: struct layout mismatches corrupt silently.

**Why it happens:**
`bindings/python/generator/generator.bat` only *prints* cdecls as a diff aid; it does not write
`_c_api.py`. The human step is easy to skip when the change looks like "one field".

**How to avoid:**
Make running the generator and diffing its output a checklist item in the P2 plan, alongside
`bindings/python/src/quiverdb/database.py` `_make_options` (L30-37) and its three call sites
(L40/65/96). Add one Python test that opens a database with the new option set and reads a value
back through it — a wrong layout fails that, where a smoke-test `open()` would not.

**Warning signs:**
`_c_api.py` untouched in a diff that changed `include/quiver/c/options.h`. Python tests green while
C++/C API tests exercise a new field.

**Phase to address:** P2.

---

### Pitfall 4: Dart — regenerating `bindings.dart` breaks Hub, and stale `.dart_tool/` caches hide the change

**What goes wrong:**
Two distinct failures. (a) Running `bindings/dart/generator/generator.bat` (ffigen) to pick up the
new struct field **regenerates enums and breaks Hub**, which pins `quiverdb` from this repo
(`Hub/hub1/pubspec.yaml` → `psrenergy/quiver`, `bindings/dart`, ref `e598fb46…` = v0.10.6). (b)
Even with the source correct, `.dart_tool/hooks_runner/` and `.dart_tool/lib/` hold the previously
built native, so `dart test` links the **old struct layout** and passes or corrupts silently —
nothing reports that the native is stale.

**Why it happens:**
The hook builds natives out of band, and the generator is a whole-file rewrite where only a few
lines are wanted.

**How to avoid:**
**Hand-edit** `bindings/dart/lib/src/ffi/bindings.dart` (the options struct is around L3555), plus
`lib/src/database.dart` `_makeOptions` (L50-61) and its three factories (L68/97/140). Put
"delete `.dart_tool/hooks_runner/` and `.dart_tool/lib/` before running the Dart suite" in the plan
text, not in someone's memory. Dart is also the binding with **no CI publish job** — it is published
by hand — so a local stale cache is the only gate it has.

**Warning signs:**
A `bindings.dart` diff touching more than the intended struct (enum reordering is the tell). Dart
tests passing on a machine that never rebuilt the native.

**Phase to address:** P2 (options struct), again in P3 (new symbol).

---

### Pitfall 5: The authoritative-wrong-label risk — Quiver repeats `HasCommitment`'s inversion to an agent

**What goes wrong:**
`HydroThermalDispatch.jl/src/collections/hydro_plant.jl` declares
`@enumx HydroPlant_HasCommitment { YES = 0; NO = 1 }`. Its
`HydroThermalDispatch.jl/database/ui/enum.toml` declares `[[bool]] id = 0 → "Disable"`,
`id = 1 → "Enable"`, and both `hydro_plant.toml` and `thermal_plant.toml` bind
`has_commitment` with `type = "enum"` / `enum = "bool"`. **The label is the inverse of the code's
meaning and nothing in the ecosystem catches it.** Today one Flutter app renders a wrong string.
After P1, Quiver hands that inversion to an LLM agent that will *reason* on it — "commitment is
disabled" — and act. A reader alone makes this class of defect worse, not better.

**Why it happens:**
There are four unsynced sources of enum truth (Julia `@enumx`, DDL comments, `enum.toml`, an
agent-authored skill) and **nothing tests that they agree**. The TOML is hand-maintained;
Foresight's own `enum.toml` header says its values *"mirror the @enumx definitions"* — mirror, not
generate.

**How to avoid:**
PROJECT.md records `validate_ui_config()` shipping **last** as an accepted decision, made with this
case explicitly on the table. So: **do not propose resequencing.** Make the accepted window
visible and bounded instead —

- Keep the accepted-risk note in the P1/P3 plan text so the window is a recorded state, not a
  surprise at audit.
- P1's rendering should keep the raw code visible next to the label
  (`values {0: 8 (Disabled), 1: 4 (Enabled)}` — the code is still there), so a consumer that
  distrusts a label can still reason on the integer. Never replace a code with only its label.
- P5 exists to close it and is therefore **not** a stretch goal. Run it against HTD as the
  acceptance case — on real repos it should also flag `Configuration.inflow_type` (4 labelled
  entries, no binding), the 9 untyped 0/1 `Configuration` flags, `Interconnection` (a table absent
  from `main.collections` entirely), SCE's orphan `agent.toml`, SCE's `tab = "original_green"` with
  no `[[scalar_tab]]`, and the 4 dead vocabularies.
- Codegen from Julia is **not available** as an alternative: HTD has 19 `@enumx` vs 11 TOML
  vocabularies; GNoMo has **3 `@enumx` vs 11** TOML vocabularies, with `optimization_solver`,
  `solution_method`, `scenario_tree`, `weekday`, `plant_initial_state` having no Julia declaration
  at all. Naming is unmappable across three styles. P5 reports disagreement; it cannot resolve it.

**Warning signs:**
Any P1–P4 plan text that describes labels as "correct" or "authoritative". Any binding surface that
exposes only a label with no code. Absence of the accepted-window note in the phase artifacts.

**Phase to address:** Stated as accepted risk in P1; mitigated only in P5.

---

### Pitfall 6: The format-as-table trap — `format` accepts a 4-key table that zero files use

**What goes wrong:**
Hub's `AttributeConfiguration.fromTOML` accepts `format` as a **string or a 4-key table**
(`element_view` / `collection_view` / `edit` / `data`). All 25 observed `format` values across the
117-file corpus are strings, so a string-only C++ parser passes every fixture today — and throws (or
silently drops the format) the first time any model author uses the table form. `format_configuration.dart`
is also **the one parser file that differs between hub1 and hub3**, i.e. the shape is actively moving.

**Why it happens:**
Fixtures are built from the corpus, and the corpus does not exercise the branch. The adversarial
verification calls this the one latent risk its own survey understated.

**How to avoid:**
Accept both forms from day one — the verification's estimate is **four lines**: if the node is a
string, use it; if it is a table, resolve one of its keys (pick one and document the choice, since
Quiver has no views) and pass the resolved string through verbatim. Also note `format` carries two
grammars (`{:.2f}` on values, `yyyy-MM-dd` on dates); **Quiver must not classify them** — pass the
string through. Add a fixture with the table form even though no real file has one.

**Warning signs:**
A `format` handler written as `.value<std::string>().value()` (which throws on a table) or as a
silent `if (auto s = ...as_string())` with no else branch. A fixture set that is 100% real-corpus
files.

**Phase to address:** P3 (the getter that exposes `format`); the parser branch lands with the
parser, so write it in P1 even if nothing reads it yet.

---

### Pitfall 7: Writing the parser against `toml-schema.md` instead of the Dart source

**What goes wrong:**
`Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md` calls itself "authoritative reference"
and is **already wrong**: it claims "attributes declared after a group belong to that group". The
parser does not implement that. `Hub/hub1/lib/models/configuration/*.dart` keys **one flat
per-collection map** (a `UniqueLinkedHashMap`) and recovers group membership by joining
`[[attribute_group]].id` against the SQL table names `{Collection}_vector_{id}` /
`{Collection}_time_series_{id}` (`Hub/hub1/lib/models/database.dart`). A C++ parser built on the doc
would produce wrong group membership for the **15 files** that interleave `[[attribute]]` and
`[[attribute_group]]` blocks — legal TOML that flattens, so the visual grouping carries no meaning.
The doc also carries a graveyard (`element_format`, `collection_format`, `singleton` — "silently
ignored"), so it documents keys that do not exist.

**Why it happens:**
It is the only written spec, it is 238 lines, it is titled "authoritative", and reading 719 lines of
Dart is more work.

**How to avoid:**
PROJECT.md already fixes this ("parser written against Hub's Dart source, not `toml-schema.md`").
Make it operational: cite the Dart file and class for each key in the parser plan, and put the
schema join (group id ↔ table-name suffix) in the C++ side that already has the `Schema` —
`Impl::load_schema_metadata` runs after `Schema::from_database`, so the join is free there. Related
consequence: **attribute ids and group ids are separate namespaces** — `BESSOperation/storage.toml`
uses `degradation` as both an `[[attribute_group]].id` and (via `Storage_time_series_degradation`)
a table suffix. One map keyed by name collides.

**Warning signs:**
A plan or PR description citing `toml-schema.md` line numbers as the contract. A parser that tracks
"current group" while iterating. A single `std::map<std::string, ...>` holding both attributes and
groups.

**Phase to address:** P1 (parser foundation); P4 (group metadata, where the join is the feature).

---

### Pitfall 8: Breaking existing `describe` output — and mistaking the binding suites for coverage

**What goes wrong:**
Two failures pointing opposite ways.
(a) **Over-eager enrichment breaks real assertions.** `tests/test_database_describe.cpp` asserts by
substring, so an *appended* enrichment survives but an *inserted* one breaks: `- priority (INTEGER)`
(L33), `- label (TEXT)` (L48), `[date_time]` (L50), the full
`some_integer: 3 non-null, 0 null; values {1: 2, 5: 1}` line (L80). The **brittle** ones are
negative: L84 `EXPECT_FALSE(contains(report, "some_float: 2 non-null, 1 null; values"))` and L104
`EXPECT_FALSE(contains(db.summarize_collection("AllTypes"), "values {"))` — any enrichment that
emits the literal `values {`, or a brace-list after a float scalar, fails them *wherever* it lands.
L52 `EXPECT_FALSE(contains(report, "Collection: Configuration"))` fails if `describe_collection`'s
enrichment names a referenced collection with that phrasing. `tests/test_lua_runner_describe.cpp`
pins the same strings as Lua patterns.
(b) **Pretending the binding suites verify the format.** All four
(`bindings/julia/test/test_database_describe.jl`, `bindings/dart/test/describe_test.dart`,
`bindings/js/test/database-describe.test.ts`, `bindings/python/tests/test_database_metadata.py`
L206-234) assert only "returns a String" / `typeof === "string"`, each with a comment deferring
content to the C++ core tests. They would not catch a regression. Counting them as five-layer
coverage is a false green.

**Why it happens:**
The enrichment is a GUI concern landing in three core format strings that two suites pin and four
suites only appear to.

**How to avoid:**
PROJECT.md's rule does the work: **with no UI config present, output is byte-identical to today.**
None of the existing fixtures has a `ui/` dir, so every assertion above passes untouched. But that
rule protects only the *old* output — the new output needs **new fixtures with a `ui/` dir and
exact-string assertions in the C++ and Lua suites**. Then make a deliberate call on the binding
suites: either strengthen them (at least one exact-string assertion against a UI fixture, so the FFI
path is proven end-to-end) or leave them honest and say so in the plan. Do not describe them as
coverage either way.

**Warning signs:**
A first implementation that emits enrichment unconditionally "because the map is empty anyway". A
phase claiming five-layer test coverage whose binding diffs contain no new assertions. Any green
run where the only new tests are in `tests/`.

**Phase to address:** P1 (the byte-identical rule + new C++/Lua fixtures); revisit in P4 when
collection labels change the header lines.

---

### Pitfall 9: Directory-scanning `ui/` instead of reading `main.collections`

**What goes wrong:**
`C:/Development/SCE/SCE.jl/database/ui/agent.toml` is a **fully-formed collection file** (id
`Agent`, one card) that SCE's `main.toml` `collections` array does not list. Hub never loads it —
67 collection files exist on disk, **66 are ever loaded**. A C++ parser that globs `*.toml` loads a
collection Hub does not, and Quiver's `describe` then reports UI metadata for something the model's
own UI considers nonexistent. Worse, a glob also picks up `enum.toml` and anything under `themes/`.

**Why it happens:**
Globbing is one line; reading `main.collections`, mapping snake_case filenames to files, and
honouring order is five.

**How to avoid:**
Load collections **only** from `main.collections`, in array order (that order *is* the display
order, which P4 exposes). Mind the casing seam: `main.collections` lists **snake_case filenames**
while each file's own `id` is **PascalCase — the SQL table name** (67/67). Join to SQL by `id`,
never by filename. Add a fixture that ships an unlisted orphan file and asserts it is not loaded.

**Warning signs:**
`std::filesystem::directory_iterator` anywhere in the UI reader. A test corpus with no orphan file.
`has_ui_config()` true for a directory with no `main.toml` (Hub throws `"main.toml is missing"`;
Quiver degrades — but it must still not load anything).

**Phase to address:** P1.

---

### Pitfall 10: Conflating attribute ids with group ids — and enum codes with positions

**What goes wrong:**
Two namespace/indexing confusions that produce plausible-looking wrong output.
(a) **Ids collide across namespaces.** `BESSOperation/storage.toml` uses `degradation` as an
`[[attribute_group]].id`, while `Storage_time_series_degradation` is the table it names; attribute
ids are *column* names in a different namespace. One flat map keyed by bare name mixes an attribute's
label into a group's, or vice versa.
(b) **Enum ids are arbitrary integers, never positions.** GNoMo's `weekday` is **1–7**; HTD's
`initial_volume_type` and SCE's `granularity_type` are **gapped `[0, 2]`**. Indexing a
`std::vector<UIEnumEntry>` by code, or assuming `entries[i].code == i`, silently mislabels — the
worst kind of defect here, because the output still looks like a valid vocabulary.

**Why it happens:**
The corpus is mostly 0-based and contiguous, so a positional implementation passes most fixtures.

**How to avoid:**
Separate maps per namespace (collection → attributes, collection → groups, plus a global vocabulary
map). Look up a code with an explicit search or a `map<int64_t, string>`, never by index. Put
GNoMo's 1-based `weekday` and one gapped `[0, 2]` vocabulary in the fixture corpus specifically to
kill positional implementations.

**Warning signs:**
`entries[code]`, `entries.at(code)`, or `code < entries.size()` in the lookup path. A fixture corpus
whose every vocabulary is 0-based and dense. Group metadata that shows an attribute's label.

**Phase to address:** P1 (enum lookup); P4 (group namespace).

---

### Pitfall 11: The release ritual — `CHANGELOG.md` is already drifted before the milestone starts

**What goes wrong:**
`CHANGELOG.md` currently heads `## [0.10.4] — unreleased` while `CMakeLists.txt` declares
**0.10.6** (both verified in-repo today). Two releases shipped without the changelog heading moving.
The **Bump Version** workflow runs `scripts/assert_version.py bump`, which rewrites the five
manifests but **not** `CHANGELOG.md` (that file is edited by hand — the ritual for it is explicitly
unsettled). Bumping from this state lands a version whose changelog section names the wrong version,
and the milestone's own entries get filed under a heading two patches stale.

**Why it happens:**
`assert_version.py` asserts the five manifests agree with each other; nothing asserts the changelog
agrees with them.

**How to avoid:**
**Reconcile `CHANGELOG.md` to 0.10.6 before any bump dispatch** — a small, standalone first task in
whichever phase ships first. Then remember the rest of the chain, because a native ABI change means
nothing reaches a consumer without a full release: a **minor** bump (a 0.x minor signals breaking;
patch does not) → `publish-s3` → tag → `publish-julia` / `publish-python` / `publish-js` in parallel
→ **Dart published by hand** (no CI job runs `hook/build.dart` on any OS). P1 is designed to be a
**patch** precisely because it changes no ABI.

**Warning signs:**
A phase plan that ends at "tests green". A bump PR whose changelog diff adds entries under
`[0.10.4]`. Hub or claw still on the old pin after the milestone "shipped" — Dart is the leg with no
automation.

**Phase to address:** A reconcile task before the first shipping phase; the full ritual at P2 (first
ABI change) and again at the milestone close.

---

### Pitfall 12: The governance gap — Quiver silently becomes a second, competing spec

**What goes wrong:**
There are **311** `database/ui/*.toml` files across ~30 repos and **zero** version keys (no
`version`, `schema_version`, or `format_version`; neither Hub nor claw reads one). No CI job
anywhere references `database/ui`. No model repo tests its own TOMLs. Hub's `test/schema/invalid/`
has 7 negative fixtures that never run against a model repo. Meanwhile both sides move: Hub's
parsers have 59 commits with `view = "chart"` added 2026-06-24, Foresight's TOMLs were edited
**2026-09-17 (today)**, CarbSteeler 2026-09-03, GNoMo 2026-09-02. And the Hub clones already
disagree — `format_configuration.dart` differs hub1 ↔ hub3, and hub2/hub3 still pin quiverdb at
`dde801b6` while hub1 pins `e598fb46` (v0.10.6). The failure mode: Hub adds or changes a key, ships
it, and Quiver keeps rendering the old semantics **with more authority than the TOML ever had**,
because Quiver is the one with a test corpus and a changelog.

**Why it happens:**
No owner exists to arbitrate. The only written spec lives in an agent-skill folder inside Hub and
names the Dart parsers as the real source of truth.

**How to avoid:**
- **Unknown keys are ignored, never rejected** — a non-negotiable parser tolerance. Exactly one
  unknown key exists in the corpus today (`conditions`, in `GNoMo/floating_storage_unit.toml`), but
  Hub adds keys on its own schedule. A parser that throws would make Quiver stop opening databases
  the day Hub ships a key.
- Log the unknown key through the existing `impl_->logger` rather than swallowing it — that log line
  is the drift detector this ecosystem does not otherwise have.
- Build `tests/schemas/ui/` from the **real** corpus: a distilled BESSOperation (the all-bare-string
  case, 78 bare / 0 dotted), a Foresight slice (en/es/pt, plus its one bare `[[model]] id = 3`
  sibling), an HTD slice (the plain `[[attribute]] id = "date_time"` spelling). That corpus is worth
  more than the parser.
- Record the scope Quiver promises to read (the Active list in PROJECT.md is that scope) so a future
  disagreement has a written boundary, the way claw's `study-config.ts` scoped itself to four keys.

**Warning signs:**
A parser branch that `throw`s on anything but a structurally broken file. A fixture corpus written
by hand rather than distilled from real files. A milestone artifact that treats the surveyed key
inventory as closed.

**Phase to address:** P1 (tolerances + fixture corpus); P5 (the validator is the only enforcement
that will ever exist here).

---

### Pitfall 13: A half-loaded UI config surviving a failed lazy load

**What goes wrong:**
`Impl::load_schema_metadata` (`src/database_impl.h` L342-349) publishes **neither** `schema` nor
`type_validator` until validation passes, because a half-loaded state would survive a failed lazy
load and crash the next call — a documented invariant. A UI member added naively (assign as you
parse, three collection files in, throw) leaves a Database whose `has_ui_config()` is true and whose
metadata is partial, for the rest of the process.

**Why it happens:**
The UI load is a multi-file walk, so the temptation is to populate incrementally into the member.

**How to avoid:**
Parse into a local, publish by move only on success — the exact shape `load_schema_metadata` already
uses. Store the **path** on `Impl` at construction (the ctor runs long before
`load_schema_metadata`; `Impl::path` is the db file path, so `<db_dir>/ui/` is derivable there).
And honour PROJECT.md's decision: a missing or malformed `ui/` degrades **silently** — log a warning,
`has_ui_config()` returns false, `open()` still succeeds. Failing the schema load would break every
existing caller whose UI dir moved.

**Warning signs:**
Member assignment inside the parse loop. A test that only covers "valid UI dir" and "no UI dir" but
not "UI dir with one broken file". `open()` throwing on a database that opens fine on master.

**Phase to address:** P1.

---

### Pitfall 14: Aggregate-init and linkage traps in the C layer

**What goes wrong:**
Three small ones that each cost a debugging session.
(a) `src/c/options.cpp` is `return {0, QUIVER_LOG_INFO};` — **positional aggregate init**. A new
field silently value-initializes (a `const char*` becomes `nullptr`, which happens to be right
here — so it will *look* fine and teach the wrong lesson) and nothing warns.
(b) `src/c/database_options.h` `convert_database_options` needs an explicit **NULL guard** on the new
`const char*` fields: NULL means "no UI dir", not an empty-string path that then gets resolved.
(c) **`tomlplusplus` is linked `PRIVATE` to the `quiver` target only** (`src/CMakeLists.txt`). It
does not propagate to `quiver_c`, `quiver_cli`, or `quiver_tests`. Parsing placed in `src/c/` — or a
C++ test that tries to `#include <toml++/toml.hpp>` to build an expectation — fails to compile, and
the tempting fix (adding a link line) leaks a private dependency into the C API's build.
`src/database.cpp` and friends are inside `quiver`, so a parser there needs **zero CMake change**.

**How to avoid:**
Use designated initializers in `quiver_database_options_default`. Guard the pointers. Keep every
byte of TOML parsing inside the `quiver` core target and assert expectations on Quiver's own API in
tests, never by re-parsing TOML in the test. Copy the `from_toml_content` / `from_toml_file` split
from `src/binary/binary_metadata.cpp` (L220) — content-first, file wrapper on top — which also makes
the parser unit-testable without fixtures on disk.

**Warning signs:**
A compile error mentioning `toml++` outside `src/`. `quiver_c` gaining a `target_link_libraries`
entry. A default-options function still using positional braces after the struct grew.

**Phase to address:** P1 (linkage), P2 (options init + NULL guard).

---

### Pitfall 15: Labelling only the codes that happen to be present

**What goes wrong:**
`summarize_collection`'s histogram is built from `SELECT <col>, COUNT(*) ... GROUP BY <col>` over
**observed** values only (and only when distinct count ≤ `kMaxDistributionCardinality = 64`). An
enum code that no element uses is invisible today and stays invisible after labelling. An agent
reading `values {0: 8 (Disabled), 1: 4 (Enabled)}` on a four-valued vocabulary reasonably concludes
the domain is two values — and then writes a `2` it believes is out of range, or never considers a
valid option.

**Why it happens:**
The histogram is a data summary; the vocabulary is schema-ish metadata. Reusing the same line
conflates them.

**How to avoid:**
Render the **full vocabulary** where the vocabulary belongs — `describe_collection`'s per-attribute
line (`- has_commitment (INTEGER) enum bool {0: Disable, 1: Enable}`) — and keep
`summarize_collection`'s line as counts-with-labels. That way the complete domain is always
reachable in one call, and the histogram stays a histogram. It also keeps the enrichment out of the
one line pinned by two brittle negative assertions (Pitfall 8).

**Warning signs:**
The vocabulary appearing only under `summary=true`. A fixture where every declared code happens to
be present in the data.

**Phase to address:** P1.

---

### Pitfall 16: Reusing or quietly redefining `CSVOptions::enum_labels`

**What goes wrong:**
Quiver already has an enum concept: `CSVOptions::enum_labels`
(`attribute → locale → label → value`, `include/quiver/options.h`), caller-supplied, CSV-only, never
stored, and **keyed by bare attribute name** — so an enum on `Items.status` also applies to
`Plants.status`. The new UI metadata is per-`(collection, attribute)` and file-sourced. Treating the
two as the same thing (or "unifying" them mid-milestone) is a breaking change to CSV round-tripping
that is out of this milestone's scope, and its FFI plumbing is hand-flattened into grouped parallel
arrays in **four** bindings.

**Why it happens:**
The names match and the shapes look similar, so "we already have enum_labels" is an easy
mis-conclusion in planning.

**How to avoid:**
Leave `CSVOptions::enum_labels` exactly as it is. It is listed in PROJECT.md as existing-and-unused
by any shipped caller; auto-populating it from the UI config is a separate, breaking decision. Do
borrow its **C API shape** as a precedent for a code→label map (grouped parallel arrays, as
`quiver_csv_options_t` does for input, with a dedicated `free_*` mirroring
`quiver_database_free_time_series_data` for output).

**Warning signs:**
A plan that says "populate `enum_labels` from `enum.toml`". A diff touching
`src/database_csv_export.cpp` / `_import.cpp` in a metadata phase.

**Phase to address:** Guard in P1 scoping; the shape precedent applies in P3.

---

### Pitfall 17: Handling only one spelling of the time-series dimension column, or one form of a localizable value

**What goes wrong:**
(a) The dimension column is spelled **two ways**: `[[attribute_group]]` with a nested
`date_time.*` table (SCE, BESSOperation, GNoMo — 39 of 75 groups) **versus** a plain
`[[attribute]] id = "date_time"` (all **6** HTD group files, plus `GNoMo/historical_conditions`).
Implementing only the nested form loses every HTD group's dimension metadata; only the flat form
loses the label/format of the majority. Hub injects `id = "date_time"` and parses the nested table
as a **full attribute**, so `date_time.format` / `.label` / `.tooltip` / `.unit` / `.hide` /
`.type` / `.enum` are all legal.
(b) Localizable values (`label`, `tooltip`, `unit`, `help`, `description`) are **string or
`{locale: string}`**, mixed **within the same file** — Foresight 12 bare / 711 dotted, GNoMo 1/735,
BESSOperation 78/0, and Foresight's `[[model]] id = 3` is a bare `"ARIMA"` among nine dotted
siblings. A parser that decides the form per model, or per file, or on first sight, breaks on real
files.

**How to avoid:**
One resolution function, applied to every localizable node: if it is a string, that is the value;
if it is a table, apply Hub's fallback chain (exact locale → `en` → first key). PROJECT.md settles
the rest: locale resolves **once at parse time**, so the union never reaches the C API or any
binding. For the dimension, absorb both spellings behind one `GroupMetadata`-shaped answer (an
explicit P4 requirement). Fixture both spellings and both localizable forms.

**Warning signs:**
`as_table()` without a string branch (or the reverse). A locale parameter anywhere past the parser.
A group fixture set that contains no HTD-style file.

**Phase to address:** P1 (localizable resolution), P4 (dimension spellings).

---

### Pitfall 18: Adding a `db:` Lua name without updating the agent-facing reference

**What goes wrong:**
`bindings/js/test/lua-api-sync.test.ts` fails whenever a `db:` binding exists in
`src/lua_runner.cpp` that `bindings/js/src/lua-api.ts` does not document. It reads as an unrelated
JS failure in a C++ phase, and the fix (editing the reference constant) is easy to mistake for
"just update the test".

**How to avoid:**
Treat `src/lua_runner.cpp` + `bindings/js/src/lua-api.ts` as one edit. The Lua leg is otherwise
cheap: one new converter next to `scalar_metadata_lua` (`src/lua_runner.cpp` L1166) covers all seven
metadata entry points, and Lua needs **no** options work at all (a `LuaRunner` borrows an
already-open `Database`; Lua has no lifecycle methods). Do not relocate the reference — PROJECT.md's
root `CLAUDE.md` lists that under "Do Not Fix".

**Warning signs:**
A JS suite failing in a phase with no JS changes.

**Phase to address:** Whichever phase first adds a `db:` name (P3 as scoped).

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Convention-only UI dir (`<db_dir>/ui/`), no `DatabaseOptions` field | Skips the entire ABI column: no 8→24 byte JS buffer, no CFFI cdef, no Dart hand-edit, no release ritual. Ships as a patch | A consumer whose UI dir is elsewhere cannot point at it; locale is fixed at `en` | **Yes, for P1** — correct for 100% of the surveyed corpus, and PROJECT.md records the explicit-path override as a later requirement |
| Fields added to `ScalarMetadata` / `GroupMetadata` instead of a new struct | One getter instead of two; no new C symbol | Native OOB write via stale JS constants (56/32), plus an enum map on every `list_scalar_attributes` call | **Never** — PROJECT.md forbids it |
| String-only `format` parser | Four lines saved | Breaks on the first table-form `format`; the corpus can never warn you | **Never** — the four lines are the fix |
| Rejecting unknown TOML keys | Catches typos | Quiver stops opening databases the day Hub ships a new key | **Never** — log a warning instead |
| Fixture corpus written by hand | Fast, readable | Misses the variance that actually exists (bare-vs-dotted mixing, 1-based enums, gapped ids, orphan files, HTD dimension spelling) | Only alongside distilled real files, never instead of them |
| Leaving the four binding describe suites at "returns a String" | No work | A format regression is invisible in four of six suites; and counting them as coverage is a false green in the phase audit | Acceptable **only if stated plainly** in the phase artifact |
| Deferring the `CHANGELOG.md` 0.10.4 → 0.10.6 reconcile | Nothing blocks today | The bump workflow files the milestone's entries under a stale heading and the release ritual starts wrong | Never — it is a one-file edit |
| Auto-populating `CSVOptions::enum_labels` from the UI config | Free CSV enum round-tripping | Silent behaviour change to CSV export/import, four hand-written FFI flattenings, out of milestone scope | Separate milestone, explicitly decided |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| PSR Hub (Flutter, the format owner) | Implementing `toml-schema.md`, which is wrong about group membership and documents dead keys (`element_format`, `collection_format`, `singleton`) | Implement `Hub/hub1/lib/models/configuration/*.dart` + `lib/models/database.dart`; cite the Dart class per key |
| PSR Hub | Assuming one clone is *the* parser | `format_configuration.dart` already differs hub1 ↔ hub3; hub2/hub3 pin an older quiverdb. hub1 is what this milestone was surveyed against — say so in the plan |
| Model repos' `main.toml` | Globbing `ui/*.toml` | Load only `main.collections`, in order; SCE's `agent.toml` is a fully-formed orphan that must stay unloaded |
| Model repos' `enum.toml` | Assuming it exists and is non-empty | Boost has **none**; CHain, SORA and `Templates/PSRExample.jl` ship **0-byte** files. `existsSync`-style guards, not branches on model identity |
| Julia `@enumx` declarations | Treating them as the truth to generate from | Sets diverge both ways (HTD 19/11, GNoMo 3/11) and naming is unmappable across three styles; the TOML is the input and P5 only *reports* disagreement |
| claw (the committed consumer) | Assuming it can read `database/ui` itself | Its read sandbox (`Claw/claw/src/tools/native.ts` `readRoots`) cannot reach the model installation — which is exactly why Quiver parses, not the host. Its `describe_data` is a pure passthrough to `describe` / `describeCollection` / `summarizeCollection`, so P1 reaches it with zero claw changes |
| claw's `study-config.ts` | Dropping its per-attribute half before Quiver ships | Coordinate: claw drops it **after** the structured getters are published and pinned; it currently reads only `main.model` / `processes` / `collections` + each file's `id`, which keeps working regardless |
| Hub as a Quiver consumer | Forgetting it pins `bindings/dart` from this repo | An ffigen regen that flips enums breaks Hub's build; hand-edit `bindings.dart`. Hub consuming the new metadata is explicitly **not** a milestone goal |
| Bun FFI | Expecting a struct-by-value default-options call | bun#6139 — Bun cannot call `quiver_database_options_default`. Expose a scalar-returning size accessor if a runtime check is wanted |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Parsing the UI config eagerly in the `Database` constructor | `open()` slows and starts touching the filesystem for every caller, including read-only and in-memory ones | Store the path in the ctor; parse inside `load_schema_metadata`, which is already lazy behind `require_schema` | Every `open()`, immediately — including `:memory:` databases that have no directory at all |
| Re-reading or re-parsing TOML per `describe` / per attribute lookup | `describe()` on a 25-collection model does N file reads; `summarize_collection` visibly stalls | Parse once into an `Impl` member (published by move, Pitfall 13); all readers consult the in-memory map | GNoMo (13 collections) and Foresight (8 files, 711 dotted localizable nodes) — i.e. today |
| Attaching the enum map to `ScalarMetadata` | `list_scalar_attributes` allocates and copies a vocabulary per column, through the C API, in every binding | Separate `get_ui_metadata` getter (also Pitfall 2's fix) | Any whole-collection metadata read on a model with many enum columns (GNoMo: 49 bindings, 37 to `bool`) |
| `summarize_collection`'s per-column `SELECT DISTINCT` + `GROUP BY` | Already existing cost; labelling adds nothing, but a "let's also count declared codes" query would double it | Take the vocabulary from the parsed config, never from SQL | Large collections; the existing `LIMIT 65` guard is why this is currently cheap — do not remove it |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Resolving the UI dir from an untrusted/config-supplied path with no containment | Quiver reads arbitrary files as "UI config" — the same class the LuaRunner sandbox exists to prevent (`resolve_sandboxed_path`, `src/lua_runner.cpp`) | P1 derives the path from the database file's own directory — no input at all. P2's explicit `ui_config_path` is caller-supplied API surface (like `from_migrations`'s path), so document it as trusted input, and keep any *Lua-reachable* UI path under the existing sandbox rule |
| Trusting label text as safe for downstream rendering | Labels are multi-KB Markdown in `help`, contain newlines (`label.en = "CO₂e Target\nType"`) and non-ASCII; embedding them raw in a fixed-width text report can corrupt the report's structure | Treat labels as opaque text: keep them on one line in `describe` output, or escape newlines. Fixture the CarbSteeler newline case |
| An LLM agent acting on an unverified label (Pitfall 5) | This is the milestone's real security-shaped risk: a wrong label (`HasCommitment` 0 = "Disable" vs Julia `YES = 0`) drives a wrong *write* | Always render the code beside the label; P5 as the mitigation, on the accepted schedule |
| Parsing a 0-byte or truncated TOML as authoritative | An empty `enum.toml` (CHain, SORA, PSRExample) silently means "no vocabularies", which is correct — but an empty `main.toml` silently meaning "no collections" hides a broken install | Distinguish absent (fine, `has_ui_config()` false) from present-but-unusable (log a warning naming the file) |

## UX Pitfalls

The users here are LLM agents reading `describe` output, and binding consumers.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Replacing the code with the label | An agent can no longer write the value it just read, and loses the ability to distrust a bad label | Always `0: 8 (Disabled)` / `{0: Disable, 1: Enable}` — code and label together |
| Showing only observed codes as "the vocabulary" | Agent concludes a 4-value enum has 2 values and never writes the others (Pitfall 15) | Full vocabulary on the structural line; counts stay on the summary line |
| Hiding `hide = true` attributes from `describe` | `hide` is a GUI affordance (360 occurrences, **always `true`**); an agent reading the data model wants those columns — they are real columns with real data | Keep them in the report, tag them `[hidden]` |
| Silently falling back to `en` with no signal | A `pt` consumer cannot tell a translated label from an untranslated one | Resolve per Hub's chain (exact → `en` → first key) and say which locale was requested in `describe`'s header line |
| Reporting UI labels with no indication a config was loaded | A consumer cannot tell "no label configured" from "no config at all" | `has_ui_config()` plus a `describe` header naming the path and locale when loaded; empty label means not configured, caller falls back to the column name |
| `get_ui_metadata` throwing on an unconfigured attribute | Every caller wraps every call in a try/catch; that is not how Hub behaves (`?? AttributeConfiguration(id: …)`) | Return a default-constructed value for a missing attribute/collection; **do** throw Pattern 2 on `get_ui_enum` of an unknown vocabulary name — that is an explicit lookup, not decoration |

## "Looks Done But Isn't" Checklist

- [ ] **Enum labels in `describe`:** often missing the *unobserved* codes — verify a fixture whose data uses 2 of 4 declared codes and the report still names all 4 somewhere.
- [ ] **Byte-identical no-config rule:** often verified only by "tests pass" — verify by diffing actual `describe` / `describe_collection` / `summarize_collection` output on a no-UI fixture against master, byte for byte.
- [ ] **Parser tolerances:** often only the happy path — verify each of: bare-and-dotted localizable values **in the same file**; absent `enum.toml`; 0-byte `enum.toml`; an unknown key (logged, not thrown); a 1-based vocabulary; a gapped `[0, 2]` vocabulary; an orphan collection file not in `main.collections`; `format` as a table; both `date_time` spellings.
- [ ] **JS options struct (P2):** often "updated the size" — verify the offsets too, verify the other two `new Uint8Array(8)` in `ffi-helpers.ts` were **not** touched, and verify a JS test actually round-trips a value through the new option.
- [ ] **Python cdef (P2):** often untouched — verify `_c_api.py` changed and that a Python test reads back a value that only a correct layout can produce.
- [ ] **Dart (P2):** often built against a stale native — verify `.dart_tool/hooks_runner/` and `.dart_tool/lib/` were cleared, and that `bindings.dart`'s diff contains *only* the intended lines (no enum reordering).
- [ ] **Five-binding coverage:** often counted from the existing describe suites, which assert only "returns a String" — verify each binding's new tests make an assertion that a wrong FFI decode would fail.
- [ ] **Lua leg:** often forgotten — verify `bindings/js/src/lua-api.ts` was updated if any `db:` name was added (`lua-api-sync.test.ts` is the gate).
- [ ] **Degrade-silently behaviour:** often only "no dir" is tested — verify a *malformed* `main.toml` leaves `open()` succeeding, `has_ui_config()` false, and a warning logged.
- [ ] **Release:** often stops at merge — verify `CHANGELOG.md` was reconciled to 0.10.6 first, all five manifests bumped together, and that **Dart was published by hand** (no CI job does it).
- [ ] **Accepted-risk record:** verify the `HasCommitment` window is written into the phase artifacts, not just known.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| JS options buffer too small (shipped) | **HIGH** | Patch `ffi-helpers.ts`, republish npm — but the corrupting version is already on npm and in any bundled `libs/`; yank or supersede fast, and add the size accessor so it cannot recur |
| Stale Python cdef (shipped) | **HIGH** | Same shape: fix, bump, republish to PyPI via cibuildwheel; assume silent data corruption in any user who upgraded the native without the wheel |
| Dart `bindings.dart` regenerated, Hub broken | MEDIUM | Revert the generated file, re-apply by hand, and rebuild after clearing `.dart_tool/hooks_runner/` + `.dart_tool/lib/`. Hub pins by commit, so nothing breaks until the pin moves |
| Existing `describe` assertions broken | LOW | The failures are loud and local to `tests/test_database_describe.cpp` / `tests/test_lua_runner_describe.cpp`; restore the byte-identical rule rather than editing the assertions |
| Wrong label shipped to agents (Pitfall 5) | MEDIUM — and *invisible* | No technical recovery inside Quiver: the data is right, the label is wrong, and only P5 or a model-repo fix closes it. Mitigate by always emitting the code, so a consumer can be corrected without a Quiver release |
| Parser rejects a key Hub added | MEDIUM | Databases stop opening in the field. Hot-fix to warn-instead-of-throw, then full release ritual. Prevented for free by the ignore-unknown-keys rule |
| Bump landed on a stale changelog heading | LOW | Edit `CHANGELOG.md` by hand and re-run the bump; `scripts/assert_version.py` refuses a state where the five manifests disagree, so catch it before, not after |
| Orphan collection loaded (`agent.toml`) | LOW | Fix the loader to read `main.collections`; output is wrong but nothing is corrupted |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| 1. JS `makeDefaultOptions` 8-byte struct | P2 (own plan); P1 avoids the file entirely | P1 diff touches no `bindings/` file; P2 has a runtime size assert + a JS round-trip test through the new option |
| 2. `SCALAR_METADATA_SIZE` / `GROUP_METADATA_SIZE` | P3 (new struct, not new fields) | `git diff` shows `attribute_metadata.h` and the two C typedefs unchanged; JS adds a new constant only |
| 3. Stale Python CFFI cdef | P2 | `_c_api.py` in the diff; a Python test that fails on a wrong layout |
| 4. Dart regen / stale `.dart_tool` caches | P2, P3 | `bindings.dart` diff is hand-scoped; plan text includes the cache-clear step; Dart suite run after a clean build |
| 5. Authoritative wrong label (`HasCommitment`) | Recorded in P1; mitigated in P5 | Accepted-window note present in P1/P3 artifacts; P5 run against HTD lists `inflow_type`, the 9 untyped flags, `Interconnection`, the orphan `agent.toml`, the 4 dead vocabularies |
| 6. `format` as a table | P1 (parser) / P3 (exposure) | A fixture using the 4-key table form parses and round-trips |
| 7. Doc-driven parser / group membership | P1, P4 | Group membership fixture with interleaved blocks; join done against SQL table names |
| 8. Breaking `describe`; false binding coverage | P1 | Byte-for-byte diff on a no-UI fixture; new exact-string assertions in C++ and Lua suites; an explicit written call on the four binding suites |
| 9. Directory scan vs `main.collections` | P1 | Fixture with an unlisted orphan file that must stay unloaded |
| 10. Attribute/group namespaces; positional enum ids | P1, P4 | Fixtures: `degradation` as both ids; a 1-based and a gapped vocabulary |
| 11. Release ritual / changelog drift | Reconcile task before the first shipping phase | `CHANGELOG.md` heads 0.10.6 before any bump dispatch; five manifests agree via `scripts/assert_version.py` |
| 12. No version field / no CI / second spec | P1 (tolerances + real-file fixture corpus); P5 | Unknown-key fixture logs a warning and does not throw; `tests/schemas/ui/` distilled from BESSOperation, Foresight, HTD |
| 13. Half-loaded UI config | P1 | Fixture with one broken collection file: `open()` succeeds, nothing partial is published, next call does not crash |
| 14. Aggregate init / NULL guard / toml++ linkage | P1 (linkage), P2 (init) | Designated initializers in `quiver_database_options_default`; no `toml++` include outside `src/`; `quiver_c` link line unchanged |
| 15. Only observed codes labelled | P1 | Fixture where declared codes outnumber observed ones |
| 16. `CSVOptions::enum_labels` conflation | P1 scoping guard | CSV sources and tests untouched across the milestone |
| 17. Dimension spellings / localizable union | P1, P4 | Both spellings and both localizable forms in the fixture corpus; no locale parameter past the parser |
| 18. Lua `db:` name vs `lua-api.ts` | Whichever phase adds a `db:` name (P3) | `lua-api-sync.test.ts` green |

## Sources

- `C:/Users/rsampaio/.claude/projects/C--Development-Quiver-quiver3/09393d1a-ae67-415d-86f8-a8b91ba2b210/subagents/workflows/wf_92804be2-64e/journal.jsonl` — the larger survey: key inventory across 117 files / 13 models, enum shape, format owner, Quiver build cost (layer-by-layer file list), template provenance, plus the two adversarial verifications (`verify:one-parser-possible` → **CONFIRMED**; `verify:no-owner` → **REFUTED**, two live consumers exist) and the synthesis brief.
- `C:/Users/rsampaio/.claude/projects/C--Development-Quiver-quiver3/09393d1a-ae67-415d-86f8-a8b91ba2b210/subagents/workflows/wf_e5d70f00-efb/journal.jsonl` — the earlier survey: the triggering agent defect, Foresight's DDL-comment vocabularies (46 comment lines, **zero** CHECK constraints, block comment lost from `sqlite_master`), Quiver's current describe/metadata surface with verbatim CLI output, and claw's tool surface and sandbox roots.
- `C:/Development/Quiver/quiver3/.planning/PROJECT.md` — decided scope, constraints, and Key Decisions (authoritative; nothing above re-opens it).
- Re-verified directly in-repo while writing: `bindings/js/src/ffi-helpers.ts` (three `new Uint8Array(8)`, only L11 is the options struct), `bindings/js/src/metadata.ts` L30-31 and its allocation sites L81/96/111/126, `CHANGELOG.md` head `## [0.10.4] — unreleased` against `CMakeLists.txt` `VERSION 0.10.6`.
- Reference implementation cited throughout: `C:/Development/Hub/hub1/lib/models/configuration/*.dart` + `lib/models/database.dart` + `lib/models/utils/toml_utils.dart`; the non-normative doc it supersedes is `C:/Development/Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md`.

---
*Pitfalls research for: Quiver UI Metadata Layer (brownfield C++ library + C API + five bindings)*
*Researched: 2026-09-17*
