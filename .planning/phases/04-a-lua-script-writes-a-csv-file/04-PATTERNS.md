# Phase 4: A Lua script writes a CSV file - Pattern Map

**Mapped:** 2026-09-16
**Files analyzed:** 9 (3 new, 6 modified)
**Analogs found:** 9 / 9

## Line-number drift check (CONTEXT.md anchors vs. current tree)

| CONTEXT.md citation | Current location | Drift |
|---|---|---|
| `src/lua_runner.cpp:128-135` (`append_number`) | `129-135` | none (off-by-one only, same block) |
| `src/lua_runner.cpp:851-900` (`resolve_sandboxed_path`) | `850-900` | none |
| `src/lua_runner.cpp:926-1000` (`read_csv_options_from_lua`) | `934-993`(body continues past 1000 was wrong upper bound) | function starts at 934, ends ~1000; fine |
| `src/lua_runner.cpp:565-579` (`BinaryFile` usertype registration) | `565-...` (usertype call starts 565, `close`/`is_open`/`get_metadata`/`get_file_path` at 578-585, arithmetic metamethods start 586) | none, confirmed exact |
| `src/lua_runner.cpp:1085-1150` (`lua_table_to_value_map`/`table_to_element`) | `1085-1157` | none, confirmed exact |
| `bindings/js/test/lua-api-sync.test.ts:75` | line 75 (`for (const type of ["BinaryFile", "BinaryMetadata", "Expression"])`) | **exact match** |
| `src/CMakeLists.txt:13` (`csv_read.cpp` in source list) | line 13 | **exact match** |
| `src/CMakeLists.txt:68` (`/bigobj` on `lua_runner.cpp`) | line 68 | **exact match** |
| `db:read_csv` binding site (CONTEXT said `452-480`) | `452-479` | none, confirmed |

All anchors CONTEXT.md cites are current and trustworthy; no re-derivation needed before planning.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `src/csv_write.h` | utility/service (internal, no Pimpl) | file-I/O | `src/csv_read.h` | exact (mirror, minus Pimpl) |
| `src/csv_write.cpp` | utility/service | file-I/O | `src/csv_read.cpp` | exact (mirror, minus Pimpl) |
| `src/utils/number.h` | utility | transform | `src/utils/datetime.h`, `src/utils/string.h` | exact (header-only inline helper) |
| `tests/test_lua_runner_write_csv.cpp` | test | request-response (Lua script -> assertions) | `tests/test_lua_runner_read_csv.cpp` | exact |
| `src/lua_runner.cpp` (binding site + usertype) | controller/binding (sol2) | request-response | itself, `db:read_csv` binding + `BinaryFile` usertype (same file) | exact |
| `src/CMakeLists.txt` | config | batch (build list) | existing `csv_read.cpp` entry | exact |
| `tests/CMakeLists.txt` | config | batch (build list) | existing `test_lua_runner_read_csv.cpp` entry | exact |
| `bindings/js/src/lua-api.ts` | config/docs | transform (template literal) | existing `db:read_csv` section | exact |
| `bindings/js/test/lua-api-sync.test.ts` | test | transform (static parse check) | existing hardcoded array at line 75 | exact |
| Root `CLAUDE.md` / `src/CLAUDE.md` | docs | N/A | existing `db:read_csv` / csv_read entries | exact |

## Pattern Assignments

### `src/csv_write.h` / `src/csv_write.cpp` (utility, file-I/O)

**Analog:** `src/csv_read.h` (68 lines) / `src/csv_read.cpp` (188 lines)

**Header shape to mirror** (`src/csv_read.h:19-66`):
```cpp
namespace quiver::csv_read {

struct Options {
    char separator = ',';
    int64_t header_row = 1;
};

class Reader {
public:
    Reader(std::string resolved_path, std::string original_path, std::string operation, Options options = {});
    ~Reader();
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) noexcept;
    Reader& operator=(Reader&&) noexcept;

    const std::vector<std::string>& header() const;
    int64_t for_each_row(const RowSink& sink);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver::csv_read
```
**Deviation per D-37:** `csv_write::Writer` drops the `struct Impl` / `unique_ptr<Impl>` Pimpl
entirely — hold `std::ofstream` and any state (separator, header flag, closed flag, `operation`,
`original_path`) as plain direct members. No zero-dependency headers to hide, so Pimpl would be
cargo-culted.

**Constructor error pattern to copy verbatim** (`src/csv_read.cpp:67-132`, esp. the `std::error_code`
existence/directory checks at lines 78-105): same shape applies to open-for-write — check parent
directory exists (WRITE-03), wrap `std::ofstream::open`/`fail()` failures as Pattern 1 with
`operation` threaded through, quote `original_path` not the resolved path. Constructor truncates the
target on open (WRITE-08) — `std::ofstream` default mode already truncates; do not add `std::ios::app`.

**Move semantics to copy verbatim** (`src/csv_read.cpp:134-138`):
```cpp
Reader::~Reader() = default;
Reader::Reader(Reader&&) noexcept = default;
Reader& Reader::operator=(Reader&&) noexcept = default;
```
(Since `Writer` is not Pimpl'd, these become ordinary defaulted special members over an
`std::ofstream` member — `std::ofstream` is move-constructible, so `= default` still works.)

**`operation` threading (D-36):** `Reader`'s constructor takes `operation` and stores it
(`Impl::operation`, `src/csv_read.cpp:16`) so every later Pattern-1 throw (in `for_each_row`,
`src/csv_read.cpp:172-174`) reuses the same string set at construction. `csv_write::Writer` must do
the same for its constructor's own throws, but per D-36 each Lua-visible **method** (`write_csv`,
`write_row`, `close`) passes its own operation name at the sol2 binding boundary, not a single
value baked in at construction — thread `operation` per-call (or store separately per call site)
so a `write_row` failure says "Cannot write_row: ..." even though the writer was opened by
`write_csv`.

**No public header, no `QUIVER_API`, no C API** — same posture as `csv_read` (see
`src/csv_read.h:4-11` comment block, which the new header should mirror/adapt).

---

### `src/utils/number.h` (utility, transform)

**Analog:** `src/utils/datetime.h`, `src/utils/string.h` — both header-only inline under `quiver::<name>`

**Namespace/include-guard shape to copy** (`src/utils/string.h:1-25` in full):
```cpp
#ifndef QUIVER_STRING_H
#define QUIVER_STRING_H

#include <algorithm>
#include <string>

namespace quiver::string {

inline std::string trim(const std::string& str) { ... }

}  // namespace quiver::string

#endif  // QUIVER_STRING_H
```
Follow this exactly for `number.h`: guard `QUIVER_NUMBER_H`, namespace `quiver::utils` (D-38 says
`quiver::utils::append_number`, note this is a **new** namespace, not `quiver::number`), `inline`
function(s), no `.cpp` companion.

**Function to move verbatim (D-38), current location** (`src/lua_runner.cpp:128-135`):
```cpp
template <typename T>
void append_number(T value, std::string& out) {
    // Shortest round-trippable form, locale-independent: 0.1 stays "0.1" instead of
    // "0.10000000000000001", and 1000000 never becomes "1,000,000".
    std::array<char, 32> buffer{};
    const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    out.append(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
}
```
Move behaviour-preserving (D-38 is explicit: "do not change the function while moving it"). Needs
`<array>`, `<charconv>`, `<cstddef>`/`<string>` includes in the new header. Two call sites to update
after the move: `src/lua_runner.cpp:143` (`append_json_double`, `append_number(value, out)`) and
`:227` (`append_json`, `append_number(value.as<std::int64_t>(), out)`) — change unqualified
`append_number` to `quiver::utils::append_number` (or bring the namespace in via a `using`/include),
and the new `csv_write.cpp` becomes the third caller for FMT-04 cell formatting.

---

### `tests/test_lua_runner_write_csv.cpp` (test)

**Analog:** `tests/test_lua_runner_read_csv.cpp` (1323 lines) + `tests/test_lua_runner.h` fixture

**Pattern:** every test opens a `LuaRunner` over the fixture db, runs a Lua script string via
`runner.run(...)`, and asserts on the returned JSON / thrown message. For this phase, per the
round-trip rule in `<specifics>`, correctness assertions must call `db:write_csv` + `w:write_row`
+ `w:close()` and then **read the file back through `db:read_csv`** in the same script (or a
follow-up `run()` call) and assert on the returned Lua table — never grep the raw file content for
non-error-message assertions. Error-path tests (TEST-12) assert on the exact Pattern-1 string,
matching how `test_lua_runner_read_csv.cpp` asserts D-22's messages.

Register in `tests/CMakeLists.txt` immediately after `test_lua_runner_read_csv.cpp` (currently line
43); see exact insertion point below.

---

### `src/lua_runner.cpp` (multiple edit sites)

**Analog for the `db:write_csv` binding site:** the existing `db:read_csv` binding
(`src/lua_runner.cpp:452-479`, inside `bind_database()`), which itself sits right after
`bin_to_csv`/`csv_to_bin` (`:442-447`) and right before `read_csv_stream` (`:480-525`). Insert
`write_csv` in the same `bind.set_function(...)` chain, in the same comment block
("CSV file reading/writing -- db-scoped and sandboxed like the file I/O above").

**Evaluation order to copy (D-22 lesson, reused for D-36):**
```cpp
bind.set_function(
    "read_csv",
    [](Database& self, const std::string& path, sol::object options, sol::this_state s) -> sol::table {
        sol::state_view lua(s);
        const auto resolved = resolve_sandboxed_path(self, "read_csv", path);
        auto csv_options = read_csv_options_from_lua(options, "read_csv");
        csv_read::Reader reader(resolved, path, "read_csv", csv_options);
        ...
    });
```
`write_csv` must resolve the sandboxed path and validate options **before** constructing the
writer, exactly this order, and return a `std::unique_ptr<CsvWriter>` (see LUA-11 pattern below)
rather than a `sol::table`.

**Options decoding — reuse `read_csv_options_from_lua`'s `separator` branch almost verbatim**
(`src/lua_runner.cpp:934-993`, esp. `962-973` for `separator`). The writer's options table has only
`separator` and `header` (an array of strings, not `header_row`) — the collect-then-validate shape
(`:944-960`) and the "unknown option" throw (`:958`) transfer directly; the `header_row` numeric
branch (`:976-993`) does **not** apply (header is a table of column names for write, not a row
index) — model a new branch after the `separator` one's explicit-`get_type()` discipline, walking
the header table with the same integer-key `pairs` walk as `append_json_table` (FMT-08, see below),
not `#t`.

**LUA-11 ownership pattern — copy `BinaryFile`'s usertype registration verbatim**
(`src/lua_runner.cpp:565-585`):
```cpp
lua.new_usertype<BinaryFile>(
    "BinaryFile",
    sol::no_constructor,
    "read", [...] { ... },
    "write", [...] { ... },
    "close", [](BinaryFile& self) { self.close(); },
    "is_open", [](BinaryFile& self) { return self.is_open(); },
    "get_metadata", [...] { ... },
    "get_file_path", [...] { ... },
    ...);
```
`db:open_file` constructs it via `std::make_unique<BinaryFile>(...)` (line 440:
`return std::make_unique<BinaryFile>(BinaryFile::open_file(resolved, mode[0], md));`). `CsvWriter`
(D-35's name) copies this shape exactly: `sol::no_constructor`, no explicit `__gc`,
`db:write_csv(...)` returns `std::make_unique<CsvWriter>(...)`, and the usertype is registered next
to `BinaryFile`'s block (same `bind_binary()`-style area, or directly after the `Database` usertype
in `bind_database()` since `CsvWriter` is CSV-scoped like `csv_read::Reader` — planner's call, but
keep it near the `write_csv` binding site for locality, not inside `bind_binary()` which is a
different subsystem).

**`resolve_sandboxed_path`** (`src/lua_runner.cpp:850-900`) — call unchanged, no modification
needed (confirmed by CONTEXT.md; `weakly_canonical` is existence-agnostic so a not-yet-created
target resolves fine).

**`append_number`** (`src/lua_runner.cpp:129-135`) — moves out per D-38 (see `utils/number.h`
above); both `append_json_double` (`:137-144`) and `append_json` (`:212-239`, the `int64_t`/`double`
branches at `:226-229`) keep calling it, now qualified.

**`append_json_table`'s integer-key walk — copy this exact technique for FMT-08**
(`src/lua_runner.cpp:146-176`):
```cpp
std::int64_t count = 0;
std::int64_t max_index = 0;
bool is_array = true;
for (auto& pair : table) {
    if (!pair.first.is<std::int64_t>()) { is_array = false; break; }
    const auto index = pair.first.as<std::int64_t>();
    if (index < 1) { is_array = false; break; }
    ++count;
    max_index = std::max(max_index, index);
}
if (is_array && count == max_index) {
    for (std::int64_t i = 1; i <= count; ++i) { ... table[i] ... }
}
```
`write_row`'s row-cell walk must copy this pattern (a `pairs`-based integer-key scan, never
`lua_rawlen`/`t.size()`), because a table with a trailing `nil` hole — exactly what a nullable
`db:read_csv` result produces — truncates under `size()`. The comment at `:147-148` states the
rationale to preserve when copying: `"Array iff keys are exactly 1..n. Walked with pairs, never
sol::table::size() -- lua_rawlen returns an arbitrary border on a table with holes."`

**Type dispatch for a cell — do NOT reuse `lua_cell_as<T>`** (`src/lua_runner.cpp:1046-1057`); that
helper is for homogeneous-typed arrays (used by `lua_table_to_vector`). `write_row`'s cell-to-string
formatting is heterogeneous per row (string/number/boolean/nil), so mirror the **type-dispatch
pattern** in `table_to_element` (`src/lua_runner.cpp:1114-1157`) instead:
```cpp
if (val.is<sol::table>()) { ... }
else if (is_lua_boolean(val)) { ... }
else if (val.is<int64_t>()) { ... }
else if (val.is<double>()) { ... }
else if (val.is<std::string>()) { ... }
else { throw std::runtime_error("Cannot " + caller + ": ..."); }
```
Per D-40, `nil` cells (and D-34, boolean cells) get their own branch: `nil` → empty cell string
(same as `""`, no sentinel), boolean → per FMT (spelled `true`/`false`? or `1`/`0`? — not pinned by
CONTEXT.md; check REQUIREMENTS.md FMT-01..06 during planning, likely `true`/`false` literal text
since there is no boolean-to-INTEGER coercion rule for CSV *text* cells — this is a planning
decision, not a pattern-mapping one).

---

## Shared Patterns

### Pattern 1 error messages, `{operation}` = the public method name (D-36)
**Source:** root `CLAUDE.md` Pattern 1 spec + `src/csv_read.cpp` (every throw site, e.g.
`:82-104`, `:113-116`, `:126-128`) + `resolve_sandboxed_path` (`:856-857`, `:886-887`, `:895-896`).
**Apply to:** every throw in `csv_write::Writer` and the `write_csv`/`write_row`/`close` sol2
bindings — `write_csv` names itself for constructor/sandbox/option errors, `write_row` names itself
for cell/finite/after-close errors, `close` names itself for its own errors.
```cpp
throw std::runtime_error("Cannot " + operation + ": file not found: " + original_path);
```

### Sandbox choke point
**Source:** `resolve_sandboxed_path` (`src/lua_runner.cpp:850-900`).
**Apply to:** `db:write_csv`'s path argument — call it unmodified, same as `db:read_csv`/
`db:bin_to_csv` do.

### Collect-then-validate options decoding
**Source:** `read_csv_options_from_lua` (`src/lua_runner.cpp:934-993`).
**Apply to:** `write_csv`'s options table (`separator`, `header`) — collect all entries via
`for_each` into a vector first, then validate, so a mid-traversal throw cannot abandon sol2's
iteration state (D-17 rationale, `:944-945` comment).

### Non-finite number guard (FMT-05)
**Source:** `append_json_double`'s `std::isfinite` check (`src/lua_runner.cpp:137-144`).
**Apply to:** `append_number`'s two callers today, and `csv_write`'s cell formatter as the third —
verify (per `<specifics>`) that `std::to_chars` on `0.0/0.0` and `1.0/0.0` behaves as expected
before relying on it; the `isfinite` guard throws regardless, so behavior is guarded either way.

## No Analog Found

None — every file in scope has a direct, current, verified analog in the existing tree.

## Metadata

**Analog search scope:** `src/`, `src/utils/`, `tests/`, `bindings/js/src/`, `bindings/js/test/`
**Files scanned:** `src/csv_read.h`, `src/csv_read.cpp`, `src/lua_runner.cpp` (full, 1984 lines, via
targeted grep + 5 non-overlapping `Read` ranges: 120-360, 440-580, 579-599, 845-945, 944-994,
1085-1165), `src/utils/string.h`, `src/utils/datetime.h`, `src/CMakeLists.txt` (1-25, 65-68),
`tests/CMakeLists.txt` (3-50), `bindings/js/test/lua-api-sync.test.ts` (28-80), `bindings/js/src/lua-api.ts`
(grep only, read_csv/write_csv occurrences)
**Pattern extraction date:** 2026-09-16
