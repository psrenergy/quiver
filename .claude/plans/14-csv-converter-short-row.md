# 14 — CSVConverter: field-count check on read and one set of split/header/dimension helpers

**Batch** 2 · **Severity** medium · **Breaking** no. Every input that is rejected now was already rejected; only the error type and message change for a short or long data row. The constructor goes private, but an object built with it had no public member to call. · **Size** M · **Layers** C++ core (binary subsystem); tests in C++, C API, Julia and Lua; docs
**Depends on** 13 (ordering only). Plan 13 rewrites the data-cell parse inside `CSVConverter::read_line` and the value formatting inside `CSVConverter::build_line`, and this plan rewrites the code around both. Land 13 first and carry its code over unchanged, as described in Changes steps 5 and 6.
**Overlaps with**
- **13:** the same two functions in `src/binary/csv_converter.cpp`. It may also add a message-asserting helper to `CSVConverterFixture`, and it adds a `### Fixed` CHANGELOG entry next to this one.
- **08:** `add_offset_from_int`, which feeds `build_datetime_string_from_time_dimension_values`. This plan calls that function but does not change it.
- **10:** `first_dimensions`/`next_dimensions`, which drive the `csv_to_bin`/`bin_to_csv` loops. This plan does not change them.
- **08, 11, 45, 48:** they add tests to `tests/test_csv_converter.cpp` or `tests/test_lua_binary.cpp`. All of those changes, and this plan's, only add tests, so they do not conflict.

## Why

**Bug: a data row with too few fields is read past its end.** `CSVConverter::read_line` (`src/binary/csv_converter.cpp`, currently ~L106-146) pushes only as many dimension cells as the line actually has:

```cpp
        if (row.dimension_values.size() < n_dim_fields) {
            row.dimension_values.push_back(std::move(field));
        } else {
```

`CSVConverter::validate_dimensions` (currently ~L301-331) then indexes every expected dimension, and nothing checks the size first:

```cpp
    for (size_t i = 0; i < expected_names.size(); ++i) {
        if (csv_dimension_values[i] != expected_values[i]) {
```

Reproduction:
- Setup: metadata with `dimensions = {row, col}`, `dimension_sizes = {3, 2}` and `labels = {val1, val2}`, and the CSV `row,col,val1,val2\n1\n`.
- What happens: `dimension_values == {"1"}`. At i=0 the cell matches row=1. At i=1 the code reads `csv_dimension_values[1]` from a one-element vector, which is undefined behaviour.
- In this repo's MSVC Debug build (`build/CMakeCache.txt`: `cl.exe`, Debug), the STL bounds check fires ("vector subscript out of range") and the process aborts.
- In Release, the code compares against whatever memory follows the buffer.
- Who can reach it: the C API (`quiver_csv_converter_csv_to_bin`), Julia (`Quiver.Binary.csv_to_bin`) and Lua (`db:csv_to_bin`, `src/lua_runner.cpp` ~L707). In every case the file content is untrusted input.

The other malformed widths fail, but in the wrong place, with the wrong exception type and no location:
- `1,1,1.0` (short) and `1,1,1.0,2.0,3.0` (long) get past the dimension check. They then fail inside `BinaryFile::write` → `validate_data_length` (`src/binary/binary_file.cpp` ~L261) with `std::invalid_argument("Data length 1 does not match expected length 2")`. That is a `logic_error`, not the `runtime_error` every other `csv_to_bin` failure raises, and it has no line number.
- `1,1,1.0,2.0,` (a trailing comma) fails in `std::stod("")`. After plan 13 it fails in plan 13's parse instead. Plan 13's message may name the label by indexing `metadata_.labels[row.data.size()]`, and for this row that index is 2 with only 2 labels: another out-of-bounds read. The width check in this plan closes that too.

**Duplication: each writer/reader pair builds its text twice.** Pairs (2) and (3) below are writer/validator pairs that must produce identical text, so if one copy drifts, `csv_to_bin` rejects a file `bin_to_csv` wrote.
1. The comma-split loop appears twice: in `read_line` (~L114-144) and in `validate_header` (~L263-293). Both use the same `find(',')` / `field_end` / "advance past the comma, or jump to `size()+1`" steps.
2. The header is built twice. `write_header` (~L203-231) hand-builds what `expected_dimension_names()` (~L233-250) returns, then appends the labels. `validate_header` (~L257-260) assembles `expected_dimension_names()` plus the labels a second time. `read_line` and `validate_dimensions` also call `expected_dimension_names()` **once per row**, allocating a vector every time.
3. The dimension cells are built twice. `build_line` (~L152-162) and `validate_dimensions` (~L309-322) both produce [the date/datetime string if aggregating] + `std::to_string` of each non-aggregated dimension.
4. The comma-join loop appears twice: in `write_header` (~L224-230) and in `build_line` (~L172-178).

**The constructor is public without reason.** `include/quiver/binary/csv_converter.h:17` declares `CSVConverter(const BinaryMetadata& metadata, std::unique_ptr<std::iostream> io, bool aggregate_time_dimensions);` under `public:`. Its only callers are `csv_converter.cpp:51` and `:84`, both inside the two static functions (grep `CSVConverter(` over the repo, excluding `build/`). Every other member is private, so an instance built by an outside caller is unusable.

Principles violated: the root CLAUDE.md error rule (a precondition failure is Pattern 1 `Cannot {operation}: {reason}`), and "Simple solutions" (one definition per shape).

## Constraints and decisions

- **Maintainer decisions (binding):**
  - The field-count check in `read_line` is the root-cause fix.
  - Keep the hand-rolled split. `std::getline(ss, f, ',')` drops a trailing empty field.
  - Make the constructor private.
  - Do **not** convert the class to free functions.

  The two new helpers that need no member state (`split_fields`, `join_fields`) go in an anonymous namespace in the `.cpp`. The class keeps its shape and its two static entry points.
- **Root CLAUDE.md, "One CSV parser, one CSV emitter":** "The binary subsystem's `CSVConverter` (`bin_to_csv`/`csv_to_bin`) is not on this path: it splits and joins on `,` and never quotes, so a label holding a comma does not round-trip." That stays true. The helpers keep exactly that behaviour, with no quoting.
- **Root CLAUDE.md, "C++ Error Message Patterns":** new messages use Pattern 1. The existing binary messages ("Unexpected header in CSV file: ...", "CSV dimension '...' has value ...") predate the patterns, which the root file lists as a known exception. They keep their wording here: this item moves the code that emits them and does not reword them.
- **Root design decision:** the binary subsystem is exposed in Julia and Lua only. Dart, Python and JS have no binary surface, so there is nothing to change or test there.
- **`src/CLAUDE.md`:** "`CSVConverter` is a plain class composing a `BinaryMetadata` and the CSV `iostream` (no Pimpl, no inheritance)". Kept.
- **Root "Self-Updating" and "Changelog":** update `src/CLAUDE.md`, and add a `### Fixed` entry under `## [0.11.0] — unreleased`. No manifest bump.

Alternatives considered and rejected:
- **`std::getline(ss, f, ',')` for the split.** It drops a trailing empty field, so `1,1,1.0,2.0,` would pass the width check as 4 fields and be accepted silently. The maintainer explicitly rejected it.
- **A `header_fields()` member function, as both verifiers proposed.** This plan corrects that proposal. The function would rebuild a vector every time it is called, and `read_line` and `validate_dimensions` need the header on every row. Both verifiers flagged that per-row allocation. Instead, a `header_` member is built once in the constructor. It replaces both `expected_dimension_names()` and the proposed `header_fields()`, and it removes today's two per-row allocations.
- **A bounds check inside `validate_dimensions` only.** That fixes the symptom: short and long data rows would still fall through to `BinaryFile`'s `invalid_argument`, and plan 13's label lookup would still be unguarded. `read_line` is the one place that sees both halves of the row before any cell is parsed.
- **Dropping the line number.** The policy verifier listed this as an option. It is rejected because a width error in a 100k-row file is useless without a line number, and `csv_to_bin`'s loop header is a natural counter (a loop variable, not a new member).
- **Routing CSVConverter through `csv_read::Reader` / `csv_write::append_record`.** That would change its quoting and its accepted input. It is outside this item and contradicts the recorded "not on this path" decision.
- **Deleting `write_header` and inlining it into `bin_to_csv`.** It shrinks to one line, but it keeps writer/validator symmetry with `validate_header`, and `csv_writer.write_header()` reads better than reaching into two private members from the static function.

## Changes

All line numbers are for HEAD `58dfe7a`, before plan 13. Plan 13 shifts `read_line` and `build_line` by a few lines, so anchor every edit on the function name and the quoted text.

### A. `include/quiver/binary/csv_converter.h`

**Step 1: make the constructor private, change the `read_line` signature, add `dimension_cells` and `header_`, and delete `expected_dimension_names`.**

Current (whole class, ~L15-50):

```cpp
class QUIVER_API CSVConverter {
public:
    CSVConverter(const BinaryMetadata& metadata, std::unique_ptr<std::iostream> io, bool aggregate_time_dimensions);

    static void csv_to_bin(const std::string& file_path);
    static void bin_to_csv(const std::string& file_path, bool aggregate_time_dimensions = true);

private:
    struct CSVRow {
        std::vector<std::string> dimension_values;
        std::vector<double> data;
    };

    // CSV Builders
    std::string build_line(const std::vector<double>& data, const std::vector<int64_t>& current_dimensions);
    std::string
    build_datetime_string_from_time_dimension_values(const std::vector<int64_t>& time_dimension_values) const;
    void write_header();

    // CSV Readers
    CSVRow read_line();

    // Validations
    std::vector<std::string> expected_dimension_names() const;
    void validate_dimensions(const std::vector<std::string>& dimension_values,
                             const std::vector<int64_t>& current_dimensions);
    void validate_header();

    // Time-dimension predicates
    bool aggregates_time_dimensions() const;
    bool has_hourly_dimension() const;

    BinaryMetadata metadata_;
    std::unique_ptr<std::iostream> io_;
    bool aggregate_time_dimensions_ = false;
};
```

New:

```cpp
class QUIVER_API CSVConverter {
public:
    static void csv_to_bin(const std::string& file_path);
    static void bin_to_csv(const std::string& file_path, bool aggregate_time_dimensions = true);

private:
    // Only the two static entry points above construct a converter.
    CSVConverter(const BinaryMetadata& metadata, std::unique_ptr<std::iostream> io, bool aggregate_time_dimensions);

    struct CSVRow {
        std::vector<std::string> dimension_values;
        std::vector<double> data;
    };

    // CSV Builders
    std::string build_line(const std::vector<double>& data, const std::vector<int64_t>& current_dimensions);
    std::string
    build_datetime_string_from_time_dimension_values(const std::vector<int64_t>& time_dimension_values) const;
    std::vector<std::string> dimension_cells(const std::vector<int64_t>& current_dimensions) const;
    void write_header();

    // CSV Readers
    CSVRow read_line(size_t line_number);

    // Validations
    void validate_dimensions(const std::vector<std::string>& dimension_values,
                             const std::vector<int64_t>& current_dimensions);
    void validate_header();

    // Time-dimension predicates
    bool aggregates_time_dimensions() const;
    bool has_hourly_dimension() const;

    BinaryMetadata metadata_;
    std::unique_ptr<std::iostream> io_;
    bool aggregate_time_dimensions_ = false;
    // The CSV column names, built once by the constructor: the dimension columns (or, when time
    // dimensions are aggregated, one date/datetime column in their place), then the labels.
    std::vector<std::string> header_;
};
```

No other change to the header, and no new includes: `<string>` and `<vector>` are already there, and unqualified `size_t` is already used in public headers (`row.h`, `result.h`).

### B. `src/binary/csv_converter.cpp`

**Step 2: includes.** Current (~L9-15):

```cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <spdlog/fmt/fmt.h>
#include <sstream>
#include <stdexcept>
```

- Delete `#include <sstream>`. Its only user, the `std::ostringstream` in `build_line`, is removed in step 6.
- Add `#include <limits>` if it is not already present (for `std::numeric_limits`, currently reached only transitively).
- Add `#include <string_view>`.
- Leave `<spdlog/fmt/fmt.h>` exactly as plan 13 left it. If plan 13 removed it along with `fmt::format`, do not re-add it. If it is still present, it is still used by `fmt::format` in `build_line`.

**Step 3: add the two file-local helpers.** Insert right after `namespace quiver {` (~L17), before the constructor:

```cpp
namespace {

// Splits on every ',' and keeps empty fields, so "a,b," is three fields and "" is one. Do not swap
// this for std::getline(stream, field, ','): it drops the trailing empty field, which would let a
// row with a trailing comma pass read_line's width check one field short.
std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> fields;
    size_t field_start = 0;
    for (;;) {
        const size_t comma = line.find(',', field_start);
        if (comma == std::string::npos) {
            fields.push_back(line.substr(field_start));
            return fields;
        }
        fields.push_back(line.substr(field_start, comma - field_start));
        field_start = comma + 1;
    }
}

std::string join_fields(const std::vector<std::string>& fields, std::string_view separator) {
    std::string joined;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0)
            joined += separator;
        joined += fields[i];
    }
    return joined;
}

}  // namespace
```

Check by hand:
- `split_fields("")` → `{""}`, the same as the old loop (1 field).
- `"a,b,"`: after `b`, `field_start == 4 == size()`, `find` returns npos, and `substr(4)` is `""` → `{"a","b",""}`, the same as the old loop.

**Step 4: the constructor builds `header_`.** Current (~L19-22):

```cpp
CSVConverter::CSVConverter(const BinaryMetadata& metadata,
                           std::unique_ptr<std::iostream> io,
                           bool aggregate_time_dimensions)
    : metadata_(metadata), io_(std::move(io)), aggregate_time_dimensions_(aggregate_time_dimensions) {}
```

New:

```cpp
CSVConverter::CSVConverter(const BinaryMetadata& metadata,
                           std::unique_ptr<std::iostream> io,
                           bool aggregate_time_dimensions)
    : metadata_(metadata), io_(std::move(io)), aggregate_time_dimensions_(aggregate_time_dimensions) {
    // The one definition of the CSV columns. write_header emits it, and validate_header, read_line
    // and validate_dimensions check against it, so a file bin_to_csv writes is exactly what
    // csv_to_bin accepts.
    const bool aggregate_time = aggregates_time_dimensions();
    if (aggregate_time) {
        header_.push_back(has_hourly_dimension() ? "datetime" : "date");
    }
    for (const auto& dim : metadata_.dimensions) {
        if (aggregate_time && dim.is_time_dimension())
            continue;
        header_.push_back(dim.name);
    }
    header_.insert(header_.end(), metadata_.labels.begin(), metadata_.labels.end());
}
```

Build it in the constructor **body**, not in a default member initializer. `aggregates_time_dimensions()` reads `metadata_` and `aggregate_time_dimensions_`, which are initialized only by the init list.

This header is identical to the old one:
- When aggregating: `date`/`datetime` first, then the non-time dimensions in order.
- Otherwise: every dimension in order.
- In both cases the labels follow.

That matches both the old `expected_dimension_names()` and the old `write_header`.

**Step 5: `csv_to_bin` passes the line number, and `read_line` checks the width.**

In `CSVConverter::csv_to_bin`, current (~L57-62):

```cpp
    // Iterate CSV lines and write to binary
    const auto& dimensions = metadata.dimensions;
    std::vector<int64_t> current_dimensions = first_dimensions(metadata);
    for (;;) {
        auto row = csv_reader.read_line();
```

New:

```cpp
    // Iterate CSV lines and write to binary. Line 1 is the header, so data starts at line 2.
    const auto& dimensions = metadata.dimensions;
    std::vector<int64_t> current_dimensions = first_dimensions(metadata);
    for (size_t line_number = 2;; ++line_number) {
        auto row = csv_reader.read_line(line_number);
```

The rest of the loop body (`validate_dimensions`, the `dims` map, `bin_writer.write`, `next_dimensions`) is unchanged. Every `getline` reads exactly one physical line: the header is read once for detection, rewound with `seekg(0)`, and read again by `validate_header`. So `line_number` is the physical line number of the file.

Replace the whole `CSVConverter::read_line` (current ~L106-146, starting `CSVConverter::CSVRow CSVConverter::read_line() {`) with:

```cpp
CSVConverter::CSVRow CSVConverter::read_line(size_t line_number) {
    std::string line;
    if (!std::getline(*io_, line)) {
        throw std::runtime_error("Cannot csv_to_bin: file ends before line " + std::to_string(line_number));
    }

    // Check the width before touching any cell. validate_dimensions indexes every dimension cell,
    // and the data branch below may name a cell's label by position, so a short row would read
    // past the end.
    std::vector<std::string> fields = split_fields(line);
    if (fields.size() != header_.size()) {
        throw std::runtime_error("Cannot csv_to_bin: line " + std::to_string(line_number) + " has " +
                                 std::to_string(fields.size()) + " fields, expected " +
                                 std::to_string(header_.size()));
    }

    // The leading fields are dimension cells (strings), the rest are data values (doubles), one per label.
    const size_t n_dim_fields = header_.size() - metadata_.labels.size();
    CSVRow row;
    for (std::string& field : fields) {
        // Route the field: dimension cells come first, data values follow.
        if (row.dimension_values.size() < n_dim_fields) {
            row.dimension_values.push_back(std::move(field));
        } else {
            // KEEP THIS BRANCH EXACTLY AS IT IS IN THE FILE (plan 13 rewrote it). At HEAD 58dfe7a it reads:
            //   if (field == "null") {
            //       row.data.push_back(std::numeric_limits<double>::quiet_NaN());
            //   } else {
            //       row.data.push_back(std::stod(field));
            //   }
            // After plan 13 the stod line is a whole-cell float parse that throws a
            // "Cannot csv_to_bin: ..." Pattern 1 message.
        }
    }
    return row;
}
```

The `// KEEP ...` comment is an instruction to you: paste the current data branch in its place and do not commit the comment. The loop variable is still named `field`, and `row.data` still grows in label order, so plan 13's branch transplants verbatim.

Two follow-ups inside that transplanted branch:
- If plan 13's message indexes the label with its own range guard (for example `row.data.size() < metadata_.labels.size() ? metadata_.labels[row.data.size()] : ...`, or a positional fallback), simplify it to `metadata_.labels[row.data.size()]`. The width check above guarantees the data branch runs at most `labels.size()` times, so the index is always in range.
- If plan 13 threaded its own line number into `read_line` (the policy verifier advised against it, but check), keep one counter only: the `line_number` parameter above. Point plan 13's message at it and delete the other counter.

`getline` returning false happens only when there was nothing left to extract. A last line without a trailing newline sets `eofbit` but not `failbit`, so it is still read normally.

**Step 6: `build_line` uses `dimension_cells` and `join_fields`.** Replace the whole `CSVConverter::build_line` (current ~L148-180) with:

```cpp
std::string CSVConverter::build_line(const std::vector<double>& data, const std::vector<int64_t>& current_dimensions) {
    std::vector<std::string> cells = dimension_cells(current_dimensions);
    for (double v : data) {
        // KEEP THIS BODY EXACTLY AS IT IS IN THE FILE (plan 13 rewrote it), renaming `elements` to `cells`.
        // At HEAD 58dfe7a it reads:
        //   if (std::isnan(v)) {
        //       elements.push_back("null");
        //   } else {
        //       elements.push_back(fmt::format("{:.6g}", v));
        //   }
        // After plan 13 the else branch formats with quiver::utils::append_number.
    }
    return join_fields(cells, ",") + '\n';
}
```

As in step 5, the `// KEEP ...` comment is an instruction to you: paste the current per-value body and do not commit the comment. The old `aggregate_time` local, the dimension loop and the `std::ostringstream` join are all gone. `dimension_cells` covers the first two and `join_fields` covers the third.

**Step 7: add `dimension_cells`.** Insert it directly after `build_datetime_string_from_time_dimension_values` (which ends ~L201 with `return quiver::datetime::format_utc(datetime).substr(0, 10);\n}`):

```cpp
// The dimension cells of one coordinate, in header_ order. build_line writes them and
// validate_dimensions checks against them.
std::vector<std::string> CSVConverter::dimension_cells(const std::vector<int64_t>& current_dimensions) const {
    const auto& dimensions = metadata_.dimensions;
    const bool aggregate_time = aggregates_time_dimensions();

    std::vector<std::string> cells;
    if (aggregate_time) {
        cells.push_back(build_datetime_string_from_time_dimension_values(current_dimensions));
    }
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (aggregate_time && dimensions[i].is_time_dimension())
            continue;
        cells.push_back(std::to_string(current_dimensions[i]));
    }
    return cells;
}
```

Do not modify `build_datetime_string_from_time_dimension_values`. Plan 08 owns its date arithmetic.

**Step 8: shrink `write_header` to one line.** Replace the whole `CSVConverter::write_header` (current ~L203-231) with:

```cpp
void CSVConverter::write_header() {
    *io_ << join_fields(header_, ",") << '\n';
}
```

**Step 9: delete `CSVConverter::expected_dimension_names`** (current ~L233-250, `std::vector<std::string> CSVConverter::expected_dimension_names() const { ... }`) entirely. After step 10 it has no callers: grep `expected_dimension_names` must return nothing in `src/` and `include/`.

**Step 10: `validate_header` is one comparison.** Replace the whole `CSVConverter::validate_header` (current ~L252-299) with:

```cpp
void CSVConverter::validate_header() {
    std::string header_line;
    std::getline(*io_, header_line);

    if (split_fields(header_line) != header_) {
        throw std::runtime_error("Unexpected header in CSV file: '" + header_line +
                                 "'. Expected columns are: " + join_fields(header_, ", "));
    }
}
```

This deliberately changes one message. Before, a header that was a correct *prefix* (too few columns, e.g. `row,col`) threw `CSV header has 2 columns, expected 4`, and every other mismatch threw the `Unexpected header ...` message. Now every mismatch throws the `Unexpected header ...` message, which also lists the expected columns. No test pinned the old text (grep `CSV header has` → only `csv_converter.cpp:296`). The CHANGELOG entry records the change.

**Step 11: `validate_dimensions` compares against `dimension_cells`.** Replace the whole `CSVConverter::validate_dimensions` (current ~L301-331) with:

```cpp
void CSVConverter::validate_dimensions(const std::vector<std::string>& csv_dimension_values,
                                       const std::vector<int64_t>& current_bin_dimension_values) {
    // read_line's width check guarantees csv_dimension_values has one cell per dimension column.
    const std::vector<std::string> expected_values = dimension_cells(current_bin_dimension_values);
    for (size_t i = 0; i < expected_values.size(); ++i) {
        if (csv_dimension_values[i] != expected_values[i]) {
            throw std::runtime_error("CSV dimension '" + header_[i] + "' has value '" + csv_dimension_values[i] +
                                     "', expected '" + expected_values[i] + "'");
        }
    }
}
```

The message is byte-identical to today's, because `header_[i]` for `i < expected_values.size()` is exactly the old `expected_names[i]`.

`dimension_cells(...).size() == header_.size() - metadata_.labels.size()` holds by construction. Both derive from the same `aggregates_time_dimensions()` rule: aggregating gives 1 plus the number of non-time dimensions, otherwise it is all dimensions.

**Unchanged:**
- `bin_to_csv`: it still calls `csv_writer.write_header()` and `*csv_writer.io_ << csv_writer.build_line(...)`.
- `aggregates_time_dimensions`, `has_hourly_dimension` and `build_datetime_string_from_time_dimension_values`.
- The aggregate detection at the top of `csv_to_bin` (`header_line.substr(0, header_line.find(','))`): it is a first-field lookup, not a split loop.

### C. C API, FFI declarations and bindings

- **C API:** nothing changes. `src/c/binary/csv_converter.cpp` wraps only the two static functions and already forwards any `std::exception`'s `what()` to `quiver_set_last_error`, so the new messages reach every binding unchanged.
- **FFI declarations:** none change. There is no C signature change, so there is no Julia regeneration, and Dart, Python and JS do not bind the binary subsystem.
- **Bindings:** `bindings/julia/src/binary/csv_converter.jl` is a pass-through (`check(C.quiver_csv_converter_csv_to_bin(path))`), and the Lua binding `db:csv_to_bin` (`src/lua_runner.cpp` ~L707-709) calls `CSVConverter::csv_to_bin` directly. Both stay as they are.

## Tests

### C++: `tests/test_csv_converter.cpp`

**T1. Fixture helper.** Add this to `CSVConverterFixture`, after `csv_lines()` (~L84-94). If plan 13 already added an equivalent "run csv_to_bin and return the message" helper, reuse that one and adapt the calls below.

```cpp
    // csv_to_bin's std::runtime_error message, or "" when it succeeds. Any other exception type
    // escapes and fails the test, which also pins that a malformed row raises a runtime_error.
    std::string csv_to_bin_error() {
        try {
            CSVConverter::csv_to_bin(path);
        } catch (const std::runtime_error& e) {
            return e.what();
        }
        return "";
    }
```

**T2. Tighten `HeaderTooFewColumns`** (~L345-350). Old assertion:

```cpp
    write_csv("row,col\n1,1\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
```

New assertion:

```cpp
    write_csv("row,col\n1,1\n");
    EXPECT_EQ(csv_to_bin_error(), "Unexpected header in CSV file: 'row,col'. Expected columns are: row, col, val1, val2");
```

This fails before the fix, because the old message was `CSV header has 2 columns, expected 4`.

**T3. A new section of tests.** Insert it after `EmptyCSVFile` (~L381-386) and before the `CSVConverterRoundTrip` banner:

```cpp
// ============================================================================
// CSVConverterCsvToBin -- Row width
// ============================================================================

// Every data row must have exactly as many fields as the header. A row missing a dimension cell
// used to reach validate_dimensions, which indexes every dimension cell: under row,col a lone "2"
// matched row=2 and then read past the end of the one-element row.
TEST_F(CSVConverterFixture, RowMissingDimensionCellReportsLine) {
    write_toml(make_simple_metadata());
    write_csv("row,col,val1,val2\n1,1,1.0,2.0\n1,2,3.0,4.0\n2\n");
    EXPECT_EQ(csv_to_bin_error(), "Cannot csv_to_bin: line 4 has 1 fields, expected 4");
}

// Used to surface from BinaryFile::write as std::invalid_argument("Data length 1 does not match ...").
TEST_F(CSVConverterFixture, RowMissingDataCellReportsLine) {
    write_toml(make_simple_metadata());
    write_csv("row,col,val1,val2\n1,1,1.0\n");
    EXPECT_EQ(csv_to_bin_error(), "Cannot csv_to_bin: line 2 has 3 fields, expected 4");
}

TEST_F(CSVConverterFixture, RowWithExtraFieldReportsLine) {
    write_toml(make_simple_metadata());
    write_csv("row,col,val1,val2\n1,1,1.0,2.0,3.0\n");
    EXPECT_EQ(csv_to_bin_error(), "Cannot csv_to_bin: line 2 has 5 fields, expected 4");
}

// A trailing comma is an empty fifth field. Pins split_fields: std::getline(stream, field, ',')
// would drop it and let this row through as four fields.
TEST_F(CSVConverterFixture, RowWithTrailingCommaReportsLine) {
    write_toml(make_simple_metadata());
    write_csv("row,col,val1,val2\n1,1,1.0,2.0,\n");
    EXPECT_EQ(csv_to_bin_error(), "Cannot csv_to_bin: line 2 has 5 fields, expected 4");
}

// Six data rows are expected (3 x 2); the file stops after the first.
TEST_F(CSVConverterFixture, FileEndingEarlyReportsLine) {
    write_toml(make_simple_metadata());
    write_csv("row,col,val1,val2\n1,1,1.0,2.0\n");
    EXPECT_EQ(csv_to_bin_error(), "Cannot csv_to_bin: file ends before line 3");
}
```

Why each one fails before the fix:
- `RowMissingDimensionCellReportsLine`: the old code reads out of bounds in `validate_dimensions`. **In the MSVC Debug build this aborts the whole `quiver_tests.exe`** with the STL assertion "vector subscript out of range". It is the root-cause regression test, and it also pins that the counter advances (line 4, not 2).
- `RowMissingDataCellReportsLine` and `RowWithExtraFieldReportsLine`: `std::invalid_argument("Data length ...")` escapes the helper, and gtest reports an unexpected exception.
- `RowWithTrailingCommaReportsLine`:
  - Before plan 13: `std::stod("")` raises `std::invalid_argument`, which escapes the helper.
  - After plan 13: plan 13's parse error instead of the width message, or an out-of-bounds label lookup if plan 13 names the label by index.
- `FileEndingEarlyReportsLine`: the old message was `CSV dimension 'row' has value '', expected '1'`.

These existing tests keep passing unchanged, and I checked each one against the new code:
- `HeaderMismatchWrongColumnName`, `HeaderTooManyColumns`, `ValidateHeaderWrongColumn`, `ValidateHeaderExtraColumn`, `ValidateHeaderMissingColumn`: the header still fails first, now always with the `Unexpected header ...` runtime_error.
- `DimensionValueMismatch` and `ValidateDimensionsWrongValue`: the width is 4, so the unchanged dimension message fires.
- `ValidateDimensionsWrongAggregatedDatetime`: the width is 3 = `date,plant_1,plant_2`, then the dimension message fires.
- `NonNumericDataValue` and `EmptyDataField`: the width is 4, then the data parse throws. That parse is plan 13's business.
- `EmptyCSVFile`: `split_fields("")` is `{""}` ≠ `header_`, so it throws `Unexpected header`.
- Every header test (`NonTimeMetadataHeader`, `AggregatedNoHourlyHeader`, `AggregatedWithHourlyHeader`, `NonAggregatedHeader`), data test (`DataValuesMatch`, `NaNValuesAppearAsNull`, `FloatPrecision` as plan 13 left it) and round-trip test (`CsvToBinToCsvWithoutHourly`, `CsvToBinToCsvWithHourly`, `RoundTripMixedTimeAndNonTime`, ...): these pin that `header_`, `dimension_cells` and `join_fields` reproduce the old writer output byte for byte.

### C API: `tests/test_c_api_csv_converter.cpp`

Add this in the "Error cases" section, after `CsvToBinMissingToml` (~L269-280). It pins that the message reaches the C error channel verbatim.

```cpp
TEST_F(BinaryCApiCSVFixture, CsvToBinShortRowReportsLine) {
    // Opening a writer writes the .toml sidecar csv_to_bin reads; the metadata is copied, so free it now.
    auto* md = make_simple_metadata();
    quiver_binary_file_t* binary_file = nullptr;
    const auto opened = quiver_binary_file_open_file(path.c_str(), 'w', md, &binary_file);
    quiver_binary_metadata_free(md);
    ASSERT_EQ(opened, QUIVER_OK);
    quiver_binary_file_close(binary_file);
    {
        std::ofstream csv(path + ".csv");
        csv << "row,col,val1,val2\n1\n";
    }

    EXPECT_EQ(quiver_csv_converter_csv_to_bin(path.c_str()), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot csv_to_bin: line 2 has 1 fields, expected 4");
}
```

`<fstream>` is already included. Before the fix, this aborts `quiver_c_tests.exe` in Debug for the same out-of-bounds reason. The fixture's `TearDown` removes `.qvr`, `.toml` and `.csv`.

### Julia: `bindings/julia/test/test_csv_converter.jl`

Add this inside `@testset "Binary CSV"`, in the "csv_to_bin -- Error cases" block, after `@testset "Empty CSV file"` (~L463-473). It uses the file's existing helpers `make_binary_file_path`, `make_simple_metadata`, `write_toml`, `write_csv` and `cleanup_binary_file`, and follows the `exc = @test_throws ...; exc.value.msg` idiom from `test_database_lifecycle.jl:148-149`.

```julia
    @testset "Short data row reports its line" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col,val1,val2\n1\n")
            exc = @test_throws Quiver.DatabaseException Quiver.Binary.csv_to_bin(path)
            @test exc.value.msg == "Cannot csv_to_bin: line 2 has 1 fields, expected 4"
        finally
            cleanup_binary_file(path)
        end
    end
```

The existing Julia error tests only assert `Quiver.DatabaseException`, so they need no change.

### Lua: `tests/test_lua_binary.cpp`

Add `#include <fstream>` to the include block (~L3-8). The file does not include it today, and `test_lua_runner.h` does not provide it. Then add this test after `CsvRoundTrip` (~L162-181):

```cpp
TEST_F(LuaBinaryTest, CsvToBinShortRowReportsLine) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    // Opening a writer leaves the .toml sidecar csv_to_bin reads. Lua has no io, so the CSV is written here.
    lua.run(md1() + "db:open_file('bin_a', 'w', md):close()\n");
    {
        std::ofstream csv(sandbox / "bin_a.csv");
        csv << "row,v\n1\n";
    }
    expect_lua_error(lua, "db:csv_to_bin('bin_a')\n", "Cannot csv_to_bin: line 2 has 1 fields, expected 2");
}
```

`md1()` is one dimension `row` of size 3 with one label `v`, so the header is `row,v` (width 2). Before the fix this row failed later with `Data length 0 does not match expected length 1`: with a single dimension there is no out-of-bounds read, so this test does not crash the old code. It pins that the Lua surface reports the new message.

No schema files are added.

## Docs and changelog

**`src/CLAUDE.md`, "Binary Subsystem" bullet list.**

Old line:

```
- `CSVConverter` class (composition, no Pimpl): `bin_to_csv(path, aggregate)`, `csv_to_bin(path)`
```

New text (one bullet):

```
- `CSVConverter` class (composition, no Pimpl): `bin_to_csv(path, aggregate)`, `csv_to_bin(path)` — the only
  entry points; the constructor is private. Writer and reader share one definition of each text shape, so a file
  `bin_to_csv` writes is exactly what `csv_to_bin` checks: the column names (`header_`, built once by the
  constructor — the dimension columns, or one `date`/`datetime` column in place of the time dimensions when
  aggregating, then the labels), a coordinate's dimension cells (`dimension_cells`), and the comma split/join
  (`split_fields`/`join_fields`, file-local). `csv_to_bin` checks each data row's field count against `header_`
  before parsing a cell (Pattern 1 `Cannot csv_to_bin: line N has X fields, expected Y`, or `file ends before
  line N`), which is what lets `validate_dimensions` index every dimension cell. `split_fields` keeps a trailing
  empty field (`a,b,` is three): do not swap it for `std::getline(stream, field, ',')`, which drops it and would let
  a trailing-comma row pass the width check.
```

No other CLAUDE.md edit is needed:
- The root CLAUDE.md "One CSV parser" passage (`splits and joins on , and never quotes`) stays accurate.
- `tests/CLAUDE.md` already lists `test_csv_converter.cpp` and describes no per-test content.
- `bindings/julia/CLAUDE.md` has no CSVConverter rule.
- `bindings/js/src/lua-api.ts` documents `db:csv_to_bin(path)` without error semantics, so it needs no change, and `lua-api-sync.test.ts` is unaffected because no binding name changes.

**`CHANGELOG.md`.** Under `## [0.11.0] — unreleased` → `### Fixed`, add this after plan 13's `csv_to_bin` entry if present, otherwise as the last bullet of `### Fixed`:

```
- **`csv_to_bin()` checks every data row's width against the header.** A row missing a dimension cell
  (`1` under the header `row,col,val1,val2`) was read past its end — an assertion abort in a debug build,
  a comparison against arbitrary memory in a release one. A data row must now have exactly as many fields
  as the header or `csv_to_bin` throws `Cannot csv_to_bin: line N has X fields, expected Y`, and a trailing
  comma counts as an extra field. That message also replaces the `Data length X does not match expected
  length Y` a short or long row used to raise from the binary writer. A file that ends before its last row
  throws `Cannot csv_to_bin: file ends before line N`, and a header with too few columns now reports the same
  `Unexpected header in CSV file: ...` as any other header mismatch instead of `CSV header has N columns,
  expected M`.
```

The entry is not marked **BREAKING**: every input it affects was already rejected, and only the message (and, for the short/long data row, `std::invalid_argument` → `std::runtime_error`) changes. The private constructor gets no changelog line because no caller could observe it: every other member was already private.

## Verification

Run from `C:\Development\Quiver\quiver3`, in Git Bash:

1. `cmake --build build --config Debug`. Expect a clean build with no new warnings from `csv_converter.cpp`.
2. `grep -rn "expected_dimension_names\|ostringstream" src/binary/csv_converter.cpp include/quiver/binary/csv_converter.h`. Expect no output.
3. `./build/bin/quiver_tests.exe --gtest_filter='CSVConverterFixture.*'`. Expect all tests to pass, including `HeaderTooFewColumns`, `RowMissingDimensionCellReportsLine`, `RowMissingDataCellReportsLine`, `RowWithExtraFieldReportsLine`, `RowWithTrailingCommaReportsLine` and `FileEndingEarlyReportsLine`. The count is 42 at HEAD 58dfe7a plus whatever plans 08, 11 and 13 added.
4. `./build/bin/quiver_tests.exe --gtest_filter='LuaBinaryTest.*'`. Expect all tests to pass, including `CsvToBinShortRowReportsLine` and the existing `CsvRoundTrip`.
5. `./build/bin/quiver_c_tests.exe --gtest_filter='BinaryCApiCSVFixture.*'`. Expect all tests to pass, including `CsvToBinShortRowReportsLine`.
6. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe` (full C++ and C suites). Expect all tests to pass.
7. `bindings/julia/test/test.bat`. Expect all tests to pass, including the new `Short data row reports its line` testset under `Binary CSV`.
8. `scripts/format.bat`, then `git diff --stat`. The diff should contain only these files:
   - `include/quiver/binary/csv_converter.h`
   - `src/binary/csv_converter.cpp`
   - `tests/test_csv_converter.cpp`
   - `tests/test_c_api_csv_converter.cpp`
   - `tests/test_lua_binary.cpp`
   - `bindings/julia/test/test_csv_converter.jl`
   - `src/CLAUDE.md`
   - `CHANGELOG.md`

   If `format.bat` touched anything else, it is pre-existing drift: revert it.
9. `scripts/test-all.bat`. Expect all seven steps to pass. The CLI smoke test is plan 65's; if it is still broken at this point, that is unrelated to this change.

No generator run is needed, because there is no C API signature change.

## Acceptance criteria

- [ ] `CSVConverter`'s constructor is under `private:`, and the public section holds only `csv_to_bin` and `bin_to_csv`.
- [ ] `expected_dimension_names()` is deleted. The CSV column names exist once, as `header_`, built in the constructor body.
- [ ] `write_header`, `validate_header`, `build_line` and `validate_dimensions` use `header_`, `dimension_cells`, `split_fields` and `join_fields`. No `find(',')` loop and no hand-rolled join loop remains in the file.
- [ ] `split_fields` keeps a trailing empty field (pinned by `RowWithTrailingCommaReportsLine`).
- [ ] `read_line(line_number)` throws `Cannot csv_to_bin: line N has X fields, expected Y` before parsing any cell, and `Cannot csv_to_bin: file ends before line N` at EOF.
- [ ] Plan 13's data-cell parse and value formatting are carried over unchanged, and any label-index guard plan 13 added is reduced to a plain `metadata_.labels[row.data.size()]`.
- [ ] New tests pass in C++ (5, plus the tightened `HeaderTooFewColumns`), C API (1), Lua (1) and Julia (1). The full `scripts/test-all.bat` is green.
- [ ] `src/CLAUDE.md` Binary Subsystem bullet and the `CHANGELOG.md` `### Fixed` entry are updated as written above.

## Pitfalls

- **Plan 13 lands first and edits the same two functions.** Read the current `read_line` data branch and the current `build_line` per-value body *before* replacing either function, and paste them into the marked spots. Replacing the functions with the HEAD-58dfe7a bodies shown in the comments would silently revert plan 13 (`std::stod` / `{:.6g}` would come back, and plan 13's tests would fail). Delete the `// KEEP ...` instruction comments.
- **Do not run the new row-missing-dimension tests against unfixed code in the full suite.** In the MSVC Debug build the out-of-bounds read triggers an STL debug assertion ("vector subscript out of range"). That can pop a dialog or abort the whole executable, taking every later test with it. If you want to see a test fail first, run it alone with `--gtest_filter`.
- **Initialization order.** Fill `header_` in the constructor body. A default member initializer or an init-list call to a helper that reads `aggregate_time_dimensions_` works only if `header_` is declared after it, which is fragile. The body is always correct.
- **The line numbers are physical lines.** The header is line 1 because `csv_to_bin` rewinds with `seekg(0)` before `validate_header` re-reads it. Do not start the counter at 1, and do not count the header twice.
- **`validate_header`'s old "too few columns" message is gone on purpose** (step 10). Do not re-add a second branch to preserve it. `HeaderTooFewColumns` now pins the single message.
- **The C API test must free `md` right after `quiver_binary_file_open_file`.** The metadata is copied, which is safe, so a failing `ASSERT` does not leak it. Plan 69 is chasing exactly such leaks in this file.
- **`test_lua_binary.cpp` has no `<fstream>` today.** Add the include, or the `std::ofstream` does not compile.
- **Line endings:** every touched file is `.cpp`/`.h`/`.jl`/`.md`, and `.gitattributes` keeps the code files LF. No `.bat` file is edited, so the CRLF trap does not apply. Do not run sed over the scripts.
- **clang-format may rewrap** the long `std::runtime_error(...)` concatenations and the one-line `write_header`. Run `scripts/format.bat` and accept its layout.
- **A throw mid-file leaves a partially written `.qvr`**, because `BinaryFile::open_file(..., 'w')` pre-fills with NaN before the first row is read. That is existing behaviour, and the fixtures' `TearDown` removes it. Do not "fix" it here (see Out of scope).

## Out of scope

- The whole-cell / locale-independent data-cell parse and the shortest round-trip output in `bin_to_csv`: plan 13.
- Time-of-day-preserving offsets behind the `date`/`datetime` column (`add_offset_from_int`): plan 08. Walking the whole parent chain in `first_dimensions`/`next_dimensions`: plan 10.
- Rewording the pre-existing ad-hoc messages (`Unexpected header in CSV file: ...`, `CSV dimension '...' has value ...`) into Pattern 1. They predate the patterns and this item only moves them.
- Quoting, or labels containing commas. By the root "One CSV parser" decision, CSVConverter never quotes.
- Rejecting extra lines after the last expected row. They are still ignored, as today.
- Cleaning up the partial `.qvr` a failed `csv_to_bin` leaves behind.
- Caching `aggregates_time_dimensions()`, which is still recomputed per row by `dimension_cells`.
- Any Dart, Python or JS change: the binary subsystem is Julia + Lua only by design decision.
