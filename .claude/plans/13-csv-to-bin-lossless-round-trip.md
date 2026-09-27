# 13 — csv_to_bin/bin_to_csv: strict whole-cell float parse and shortest round-trip output

**Batch** 2 · **Severity** high · **Breaking** yes — for callers of `bin_to_csv` that compare its CSV text byte for byte (values are now written at full precision), for CSV files fed to `csv_to_bin` that relied on a cell being truncated (`9.99abc`, `1.5 `), and for matchers on the old bare `stod` error text. Reaches C++, the C API, Julia and Lua. · **Size** M · **Layers** C++ core, C API (tests), Julia (tests), Lua (tests), docs/changelog
**Depends on** none · **Overlaps with** 14 (same function `CSVConverter::read_line`; see below), 58 (the `import_csv` call sites of `parse_float`), 56 (the same `src/CLAUDE.md` typing-policy bullet), 61 (the include block of `src/database_csv_import.cpp`), 45 and 48 (both add tests to `tests/test_lua_binary.cpp`), 08 and 10 (other functions in `src/binary/csv_converter.cpp`)

Overlap details:
- **14** (runs after this plan) restructures `read_line` into split-then-convert and adds a row-width check. It has to keep the `utils::parse_float` call and the exact message this plan adds, because four layers of tests pin that message. Once its width check runs before conversion, the `if (row.data.size() < metadata_.labels.size())` guard in this plan's throw can never be false, and 14 may delete it. The C++ test `NonNumericCellPastLastLabel` added here asserts only the `"Cannot csv_to_bin: "` prefix, so 14's width error still satisfies it.
- **58** (later) rewrites `import_csv` around one `convert_cell`. After this plan its REAL branch has to call `utils::parse_float` from `src/utils/number.h`, because the file-local `parse_float` is gone.
- **56** (later) rewrites the "One scalar typing policy" bullet in `src/CLAUDE.md`. This plan changes only the file reference for `parse_float` in that bullet.
- **61** (later) removes `<cstdio>` from `src/database_csv_import.cpp`. This plan removes `<cerrno>`, `<clocale>`, `<cmath>` and `<cstdlib>` from the same include block.
- **08 / 10** change `build_datetime_string_from_time_dimension_values` and the dimension-start logic. Those are different functions in the same file, so no lines are shared.

## Why

`CSVConverter` is the binary subsystem's `.qvr` ⇄ `.csv` converter. It loses data in both directions.

**Writer** — `src/binary/csv_converter.cpp`, `CSVConverter::build_line` (currently ~L164-170):

```cpp
    for (double v : data) {
        if (std::isnan(v)) {
            elements.push_back("null");
        } else {
            elements.push_back(fmt::format("{:.6g}", v));
        }
    }
```

`{:.6g}` keeps 6 significant digits. `1.23456789` is written as `1.23457`, `1234567.89` as `1.23457e+06`, and `0.1 + 0.2` as `0.3` (one ULP away). A bin → csv → bin round trip therefore changes the data without any error. The test `CSVConverterFixture.FloatPrecision` (tests/test_csv_converter.cpp ~L184-201) and the Julia testset "Float precision" (bindings/julia/test/test_csv_converter.jl ~L197-218) currently **pin** this lossy output (`"1,1.23457"`). `export_csv` had the same `%g` bug and it was fixed as a BREAKING change (CHANGELOG `export_csv() writes floats at full precision`). The project already has one number emitter for exactly this: `quiver::utils::append_number` in `src/utils/number.h`, which gives the shortest form that reads back to the same double, independent of locale.

**Reader** — the same file, `CSVConverter::read_line` (currently ~L130-135):

```cpp
            // Convert data value to double, treating "null" as NaN.
            if (field == "null") {
                row.data.push_back(std::numeric_limits<double>::quiet_NaN());
            } else {
                row.data.push_back(std::stod(field));
            }
```

`std::stod` never reports how much of the string it consumed, and it calls `strtod`, which uses the process's `LC_NUMERIC` decimal point. Reproductions:

| Input | Now | Should be |
| --- | --- | --- |
| data cell `9.99abc` (header `row,col,val1,val2`, row `1,1,9.99abc,2.0`, in an otherwise complete file) | stored as `9.99`, no error | `Cannot csv_to_bin: invalid float value '9.99abc' for label 'val1'` |
| data cell `1.5` under a decimal-comma C locale (a Python host that ran `locale.setlocale(locale.LC_ALL, "")` on a pt-BR machine, then `LuaRunner` → `db:csv_to_bin`, bound at src/lua_runner.cpp ~L707) | stored as `1.0`, no error | `1.5` |
| data cell `1e-310` (a subnormal; `bin_to_csv` writes it) on Linux/macOS | `std::out_of_range("stod")` | the subnormal |
| data cell `abc` or empty | bare `invalid stod argument` (MSVC) / `stod` (libstdc++), which is a `std::invalid_argument` | a Pattern 1 message naming the label |

The project fixed exactly this bug for `import_csv` (CHANGELOG 0.11.0 "Fixed: `import_csv()` reads numbers the same way in every host locale and on every platform"). The fix lives one file away as a `static` function, `parse_float`, in `src/database_csv_import.cpp` (currently ~L47-74). It spells the cell in the active locale, requires the whole cell to parse, and accepts subnormals. `std::stod` is the only one left in `src/`.

Principles violated: the root typing-policy decision (numbers are read "in the 'C' locale's number format whatever locale the host process set"); "Error Messages" (every message is a Pattern 1/2/3 message defined in C++, never a bare library string); reuse over re-implementation.

## Constraints and decisions

- **Maintainer decision (binding):** "Include the bin_to_csv {:.6g} -> utils::append_number writer fix in the same change." The reader fix and the writer fix land together.
- **Root CLAUDE.md, "One scalar typing policy lives in C++":** `import_csv` reads CSV text with a whole-cell parse "in the 'C' locale's number format whatever locale the host process set, since export always writes `.`". `csv_to_bin` now follows the same rule through the same function.
- **Root CLAUDE.md, "One CSV parser, one CSV emitter":** `CSVConverter` "is not on this path: it splits and joins on `,` and never quotes". That stays true. This plan does **not** route `CSVConverter` through `csv_read::Reader` / `csv_write::append_record`. It only shares the *number* reader and writer.
- **Root CLAUDE.md, "C++ Error Message Patterns":** new code uses Pattern 1, `"Cannot {operation}: {reason}"`. The operation is `csv_to_bin`, which is the public name in C++, the C API (`quiver_csv_converter_csv_to_bin`), Julia (`csv_to_bin`) and Lua (`db:csv_to_bin`).
- **Root CLAUDE.md, "Binary + expression subsystems are exposed in Julia and Lua only":** there are no Dart, Python or JS tests or changes.
- **Root CLAUDE.md, "Do Not Fix":** the binary hot-path decisions in `src/CLAUDE.md` (the `unordered_map` dims parameter) are untouched.
- **Root CLAUDE.md, Build System:** the macOS 13.3 floor exists because libc++ gates floating-point `std::to_chars`. `csv_converter.cpp` now instantiates it through `append_number`. The floor already applies to the whole core (`cmake/Platform.cmake`), so no build change is needed, only the list of files that CLAUDE.md names.
- **Principles:** "delete unused code" (the `fmt` include in `csv_converter.cpp` and the four C headers in `database_csv_import.cpp` become unused), and "simple over abstract" (no new helper beyond moving `parse_float`).
- **Verifier corrections adopted:** `parse_float` becomes `inline` in the header; no line numbers in the message (`read_line` has no row counter, and plan 14 owns the row-level restructuring); the label index is bounds-guarded because a row wider than the header would otherwise index past `metadata_.labels`; `NonNumericDataValue` and `EmptyDataField` now assert the message. The Lua test that the policy verifier called optional **is** added, because the repo rule is a test in every layer where the behaviour is visible, and the message reaches Lua scripts.

Rejected alternatives:
- *Fix only the reader and keep `{:.6g}`.* Rejected by the maintainer decision; the round trip would still lose data.
- *`fmt::format("{}", v)` for the writer.* It is also shortest-round-trip, but that would be a second number emitter next to `utils::append_number`, and it would keep the `fmt` dependency in this file.
- *`std::from_chars<double>` for the reader.* It is locale-free, but `import_csv` deliberately ships `strtod` plus the locale shim, and libc++ support for floating-point `from_chars` on the macOS 13.3 floor is not established. One reader for all CSV numeric text beats two.
- *Also move `parse_integer` into `number.h`.* YAGNI: only `import_csv` uses it.
- *Row or line numbers in the message.* That needs new state in `read_line`, and plan 14 restructures that function. The label plus the quoted cell is enough to find the cell.
- *Hoist the test-local `RestoreNumericLocale` idiom into `tests/test_utils.h`.* That would touch `tests/test_database_csv_import.cpp`, which other plans edit. A second copy of a 14-line test idiom is cheaper.

## Changes

### 1. `src/utils/number.h` — add `parse_float` next to `append_number`

Current file (whole file, 28 lines): the include block is `<array>`, `<charconv>`, `<cstddef>`, `<string>`, and the namespace holds only `append_number`.

New file (replace it whole; `append_number` is unchanged):

```cpp
#ifndef QUIVER_NUMBER_H
#define QUIVER_NUMBER_H

#include <array>
#include <cerrno>
#include <charconv>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace quiver::utils {

template <typename T>
void append_number(T value, std::string& out) {
    // Shortest round-trippable form, locale-independent: 0.1 stays "0.1" instead of
    // "0.10000000000000001", and 1000000 never becomes "1,000,000".
    std::array<char, 32> buffer{};
    const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    // 32 bytes covers every type this is instantiated with today (int64 needs 20, double 24), but
    // to_chars leaves the buffer contents unspecified when it reports value_too_large -- appending
    // `end - data()` bytes then injects garbage rather than failing. Append nothing instead.
    if (ec != std::errc{}) {
        return;
    }
    out.append(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
}

// The reading half of append_number: a whole-cell float parse, nullopt unless every character is
// consumed, so "9.99abc" or a decimal-comma "1,5" is not a float (strtod alone accepts any valid
// prefix). strtod reads the C locale's decimal point, which a host process can switch to ','
// (Python's locale.setlocale(LC_ALL, "") on a pt-BR machine), while append_number writes '.' in
// every locale. So the cell is spelled in the active locale first, and parses exactly as it would in
// the "C" locale.
inline std::optional<double> parse_float(std::string cell) {
    const std::string_view point = std::localeconv()->decimal_point;
    if (point != ".") {
        if (cell.find(point) != std::string::npos) {
            return std::nullopt;  // "1,5" is not a number in the "C" locale either
        }
        if (const auto dot = cell.find('.'); dot != std::string::npos) {
            cell.replace(dot, 1, point);
        }
    }
    const char* begin = cell.c_str();
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(begin, &end);
    if (end == begin || end != begin + cell.size()) {
        return std::nullopt;
    }
    // ERANGE marks a literal no finite double holds -- overflow to inf, or a nonzero value flushed to
    // 0 -- but glibc and Apple libc also raise it for a representable subnormal, which append_number
    // writes, so only the first two are rejected.
    if (errno == ERANGE && (std::isinf(value) || value == 0.0)) {
        return std::nullopt;
    }
    return value;
}

}  // namespace quiver::utils

#endif  // QUIVER_NUMBER_H
```

Why: this is the body from `database_csv_import.cpp` byte for byte. Only `static` becomes `inline`, because the header is included by several translation units (`database_csv_import.cpp`, `database_csv_export.cpp`, `lua_runner.cpp`, `binary/csv_converter.cpp`). The comment is reworded so it no longer speaks for import alone ("export_csv writes '.'" becomes "append_number writes '.'"; `export_csv`, `db:write_csv` and now `bin_to_csv` all emit through it).

### 2. `src/database_csv_import.cpp` — delete the local copy and call `utils::parse_float`

2a. Include block (currently ~L1-22). Add `#include "utils/number.h"` after `#include "utils/datetime.h"`, and delete the four headers only `parse_float` used:

Current:
```cpp
#include "csv/csv_read.h"
#include "database_impl.h"
#include "quiver/options.h"
#include "quiver/schema.h"
#include "utils/datetime.h"
#include "utils/string.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
```
New:
```cpp
#include "csv/csv_read.h"
#include "database_impl.h"
#include "quiver/options.h"
#include "quiver/schema.h"
#include "utils/datetime.h"
#include "utils/number.h"
#include "utils/string.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
```
(Leave `<cstdio>` alone; plan 61 owns it. The lines after `<ctime>` stay: `<filesystem>`, `<fstream>`, `<iomanip>`, `<optional>`, `<set>`, `<sstream>`, `<string_view>`.) Before deleting, confirm that nothing else in the file uses those four headers: `grep -nE "errno|ERANGE|localeconv|strto|std::isinf|std::isnan|std::(floor|ceil|round|abs|fabs|pow)" src/database_csv_import.cpp` must list only lines inside the `parse_float` you are about to delete. It does at HEAD 58dfe7a.

2b. The comment above `parse_integer` (currently ~L32-35). Current:
```cpp
// Whole-cell number parses: nullopt unless every character is consumed, so "1.5" is not an integer
// and "9.99abc" or a decimal-comma "1,5" is not a float (stoll/strtod alone accept any valid prefix).
// Import writes through a raw INSERT, so this is where the core typing policy -- a float into an
// INTEGER column is rejected -- reaches CSV text.
static std::optional<int64_t> parse_integer(const std::string& cell) {
```
New:
```cpp
// Whole-cell integer parse: nullopt unless every character is consumed, so "1.5" is not an integer
// (stoll alone accepts any valid prefix); REAL cells go through its float counterpart,
// utils::parse_float. Import writes through a raw INSERT, so this is where the core typing policy --
// a float into an INTEGER column is rejected -- reaches CSV text.
static std::optional<int64_t> parse_integer(const std::string& cell) {
```

2c. Delete the whole `parse_float` block (currently ~L47-74), from the comment line `// strtod reads the C locale's decimal point, which a host process can switch to ',' (Python's` down to and including the function's closing `}` just before `// Lowercase a string for case-insensitive comparison.`

2d. Qualify the four call sites. They are inside `namespace quiver`, so `utils::` resolves to `quiver::utils`:
- scalar validation (currently ~L447): `if (type == DataType::Real && !parse_float(cell)) {` → `if (type == DataType::Real && !utils::parse_float(cell)) {`
- scalar insert (currently ~L521): `parameters.emplace_back(*parse_float(cell));  // validated above` → `parameters.emplace_back(*utils::parse_float(cell));  // validated above`
- group validation (currently ~L699): `if (type == DataType::Real && !parse_float(cell)) {` → `if (type == DataType::Real && !utils::parse_float(cell)) {`
- group insert (currently ~L765): `parameters.emplace_back(*parse_float(cell));  // validated above` → `parameters.emplace_back(*utils::parse_float(cell));  // validated above`

Check: `grep -n "parse_float" src/database_csv_import.cpp` must show exactly these four `utils::parse_float(` lines plus the comment from 2b.

### 3. `src/binary/csv_converter.cpp` — read and write through the shared pair

3a. Includes (currently ~L1-15). Add `#include "utils/number.h"` after `#include "utils/datetime.h"` and delete `#include <spdlog/fmt/fmt.h>`. The `fmt::format` in `build_line` is the file's only `fmt` use; confirm with `grep -n "fmt::" src/binary/csv_converter.cpp`, which must be empty after 3c.

New block:
```cpp
#include "quiver/binary/csv_converter.h"

#include "binary_utils.h"
#include "quiver/binary/binary_file.h"
#include "quiver/binary/dimension.h"
#include "quiver/binary/iteration.h"
#include "utils/datetime.h"
#include "utils/number.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
```

3b. `CSVConverter::read_line` — the data branch (currently ~L126-136). Current:
```cpp
        // Route the field: dimension labels come first, data values follow.
        if (row.dimension_values.size() < n_dim_fields) {
            row.dimension_values.push_back(std::move(field));
        } else {
            // Convert data value to double, treating "null" as NaN.
            if (field == "null") {
                row.data.push_back(std::numeric_limits<double>::quiet_NaN());
            } else {
                row.data.push_back(std::stod(field));
            }
        }
```
New:
```cpp
        // Route the field: dimension labels come first, data values follow.
        if (row.dimension_values.size() < n_dim_fields) {
            row.dimension_values.push_back(std::move(field));
        } else {
            // Convert data value to double, treating "null" as NaN. The whole cell must parse, in the
            // "C" locale's number format whatever locale the host set -- bin_to_csv writes '.'.
            if (field == "null") {
                row.data.push_back(std::numeric_limits<double>::quiet_NaN());
            } else if (auto value = utils::parse_float(field)) {
                row.data.push_back(*value);
            } else {
                std::string message = "Cannot csv_to_bin: invalid float value '" + field + "'";
                // A cell past the last label belongs to a row wider than the header: no label to name.
                if (row.data.size() < metadata_.labels.size()) {
                    message += " for label '" + metadata_.labels[row.data.size()] + "'";
                }
                throw std::runtime_error(message);
            }
        }
```
Why: `row.data.size()` at this point is the index of the current data cell, so it names that cell's label. The guard is not defensive noise. `read_line` does not yet check the field count (plan 14 adds that), so a row like `1,1,1.0,2.0,abc` reaches this branch with `row.data.size() == labels.size()`, and an unguarded `labels[...]` would read past the vector. Keep the `"null"` test first: `parse_float("null")` returns nullopt and would throw.

3c. `CSVConverter::build_line` — the data loop (currently ~L164-170). Current:
```cpp
    for (double v : data) {
        if (std::isnan(v)) {
            elements.push_back("null");
        } else {
            elements.push_back(fmt::format("{:.6g}", v));
        }
    }
```
New:
```cpp
    for (double v : data) {
        if (std::isnan(v)) {
            elements.push_back("null");
        } else {
            // Shortest text that reads back to the same double, '.' in every locale.
            std::string cell;
            utils::append_number(v, cell);
            elements.push_back(std::move(cell));
        }
    }
```
Why: `utils::append_number` is the project's one float emitter (`export_csv`, `db:write_csv`, the Lua JSON encoder). Its output (`std::to_chars` shortest form, e.g. `1`, `1.5`, `1.23456789`, `1e-310`, `-2.5e+300`, `inf`) is exactly what `parse_float` reads back, so bin → csv → bin is exact. For every value in the existing tests (`1.5`, `42`, `100`, `1001`, `11.5`, ...) the text is unchanged, because `{:.6g}` and `to_chars` agree whenever 6 digits are enough.

No header change (`include/quiver/binary/csv_converter.h` is untouched). No C API, FFI declaration, Julia wrapper or Lua binding change: `quiver_csv_converter_csv_to_bin` already forwards `e.what()` to `quiver_get_last_error` (`src/c/binary/csv_converter.cpp`), Julia's `check` turns it into `DatabaseException(msg)`, and Lua's `db:csv_to_bin` (src/lua_runner.cpp ~L707) calls the C++ function directly. `bindings/julia/src/c_api.jl` does **not** need regenerating.

## Tests

### C++ — `tests/test_csv_converter.cpp` (fixture `CSVConverterFixture`, no schema)

T1. Add `#include <clocale>` to the include block (alphabetically, after `#include <chrono>`).

T2. Add a fixture helper after `csv_lines()` (inside `class CSVConverterFixture`, `protected:`):
```cpp
    // csv_to_bin(path) must throw std::runtime_error carrying exactly `message`.
    void expect_csv_to_bin_error(const std::string& message) {
        try {
            CSVConverter::csv_to_bin(path);
            FAIL() << "expected csv_to_bin to throw";
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), message.c_str());
        }
    }
```

T3. Change `FloatPrecision` (currently ~L184-201). Old:
```cpp
    // {:.6g} format
    EXPECT_EQ(lines[1], "1,1.23457");
```
New:
```cpp
    // Shortest round-trip form (utils::append_number), not 6 significant digits.
    EXPECT_EQ(lines[1], "1,1.23456789");
```
It fails before the fix, because the writer emits `1.23457`.

T4. Change `NonNumericDataValue` and `EmptyDataField` (currently ~L367-379). Old bodies end with `EXPECT_THROW(CSVConverter::csv_to_bin(path), std::exception);`. New:
```cpp
TEST_F(CSVConverterFixture, NonNumericDataValue) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2\n1,1,abc,2.0\n");
    expect_csv_to_bin_error("Cannot csv_to_bin: invalid float value 'abc' for label 'val1'");
}

TEST_F(CSVConverterFixture, EmptyDataField) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2\n1,1,,2.0\n");
    expect_csv_to_bin_error("Cannot csv_to_bin: invalid float value '' for label 'val1'");
}
```
Both fail before the fix: `std::stod` throws `std::invalid_argument`, which is not a `std::runtime_error`, so it escapes the helper and gtest reports an uncaught exception.

T5. Add right after `EmptyDataField`:
```cpp
// std::stod read the longest valid prefix, so this cell was stored as 9.99 with no error.
TEST_F(CSVConverterFixture, TrailingGarbageDataValue) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2\n1,1,1.0,9.99abc\n");
    expect_csv_to_bin_error("Cannot csv_to_bin: invalid float value '9.99abc' for label 'val2'");
}

// A cell past the last label belongs to a row wider than the header, so there is no label to name
// (and indexing labels there would read past the end). Only the Pattern 1 prefix is pinned: a
// row-width check in front of the conversion would report this row first, just as correctly.
TEST_F(CSVConverterFixture, NonNumericCellPastLastLabel) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2\n1,1,1.0,2.0,abc\n");
    try {
        CSVConverter::csv_to_bin(path);
        FAIL() << "expected csv_to_bin to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_TRUE(std::string(e.what()).starts_with("Cannot csv_to_bin: ")) << e.what();
    }
}
```
`TrailingGarbageDataValue` fails before the fix: `9.99abc` is stored as `9.99`, and the call only throws later, on the missing second row, with the unrelated `CSV dimension 'row' has value '', expected '1'`, so `EXPECT_STREQ` fails. `NonNumericCellPastLastLabel` fails before the fix (`std::invalid_argument` escapes). It would crash or assert with an unguarded label index.

T6. Add in the `CSVConverterRoundTrip` section, after `RoundTripWithNullValues`:
```cpp
// bin_to_csv writes each value in the shortest form that reads back to the same double, and
// csv_to_bin parses it whole, so the round trip is exact. {:.6g} brought 1.23456789 back as 1.23457,
// and std::stod rejected the subnormal on Linux/macOS (ERANGE -> out_of_range). EXPECT_EQ, not
// EXPECT_DOUBLE_EQ: 0.1 + 0.2 and 0.3 are one ULP apart, inside EXPECT_DOUBLE_EQ's tolerance.
TEST_F(CSVConverterFixture, RoundTripIsLossless) {
    const std::vector<double> values = {1.23456789, 0.1 + 0.2, 1234567.89, -2.5e300, 1e-310};
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"row"})
                                               .set("dimension_sizes", {5})
                                               .set("labels", {"val"}));
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        for (size_t i = 0; i < values.size(); ++i) {
            binary_file.write({values[i]}, {{"row", static_cast<int64_t>(i + 1)}});
        }
    }
    CSVConverter::bin_to_csv(path);
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    for (size_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(reader.read({{"row", static_cast<int64_t>(i + 1)}})[0], values[i]) << "row " << (i + 1);
    }
}
```

T7. Add right after `RoundTripIsLossless`:
```cpp
// A host can switch the process's C locale to a decimal comma -- Python's
// locale.setlocale(locale.LC_ALL, "") does on a pt-BR or de-DE machine -- and strtod follows it,
// while bin_to_csv writes '.' in every locale. std::stod read "1.5" back as 1 there.
TEST_F(CSVConverterFixture, DecimalCommaLocaleReadsWrittenFloats) {
    struct RestoreNumericLocale {
        std::string saved = std::setlocale(LC_NUMERIC, nullptr);
        ~RestoreNumericLocale() { std::setlocale(LC_NUMERIC, saved.c_str()); }
    } restore;
    bool switched = false;
    for (const char* name : {"pt-BR", "pt_BR.UTF-8", "de_DE.UTF-8"}) {
        if (std::setlocale(LC_NUMERIC, name) != nullptr) {
            switched = true;
            break;
        }
    }
    if (!switched) {
        GTEST_SKIP() << "no decimal-comma locale installed";
    }

    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.5, 0.1 + 0.2}, {{"row", 1}, {"col", 1}});
    }
    CSVConverter::bin_to_csv(path);
    auto lines = csv_lines();
    ASSERT_GE(lines.size(), 2u);
    EXPECT_EQ(lines[1], "1,1,1.5,0.30000000000000004");
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    auto v = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_EQ(v[0], 1.5);
    EXPECT_EQ(v[1], 0.1 + 0.2);
}
```
It fails before the fix on any machine with a decimal-comma locale (Windows always has `pt-BR`). The written line says `0.3`, and `v[0]` reads back as `1`. It skips where no such locale is installed, the same as `DatabaseCSV.ImportCSV_DecimalCommaLocale_ReadsExportedFloats`.

### C API — `tests/test_c_api_csv_converter.cpp` (fixture `BinaryCApiCSVFixture`)

C1. Strengthen `RoundTrip` (currently ~L103-144). Old:
```cpp
        double data[] = {42.5, 99.5};
```
and
```cpp
        EXPECT_DOUBLE_EQ(out_data[0], 42.5);
        EXPECT_DOUBLE_EQ(out_data[1], 99.5);
```
New:
```cpp
        // 0.1 + 0.2 needs 17 significant digits; bin_to_csv used to write "0.3", one ULP away.
        double data[] = {0.1 + 0.2, 99.5};
```
and
```cpp
        EXPECT_EQ(out_data[0], 0.1 + 0.2);  // exact: EXPECT_DOUBLE_EQ tolerates the 1-ULP loss
        EXPECT_EQ(out_data[1], 99.5);
```
It fails before the fix (it reads back 0.3).

C2. Add in the "Error cases" section, after `CsvToBinMissingToml`:
```cpp
TEST_F(BinaryCApiCSVFixture, CsvToBinTrailingGarbageReportsMessage) {
    auto* md = make_simple_metadata();
    quiver_binary_file_t* binary_file = nullptr;
    ASSERT_EQ(quiver_binary_file_open_file(path.c_str(), 'w', md, &binary_file), QUIVER_OK);  // writes the .toml
    quiver_binary_file_close(binary_file);
    quiver_binary_metadata_free(md);

    {
        std::ofstream csv(path + ".csv");
        csv << "row,col,val1,val2\n1,1,9.99abc,2.0\n";
    }

    EXPECT_EQ(quiver_csv_converter_csv_to_bin(path.c_str()), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot csv_to_bin: invalid float value '9.99abc' for label 'val1'");
}
```
It fails before the fix: `9.99abc` is accepted, and the call only fails later, on the missing second row, with the unrelated `CSV dimension 'row' has value '', expected '1'`, so `EXPECT_STREQ` fails. `<fstream>` is already included.

### Lua — `tests/test_lua_binary.cpp` (fixture `LuaBinaryTest`, schema `valid/collections.sql`)

L1. Add `#include <fstream>` to the include block (after `#include <filesystem>`).

L2. Strengthen `CsvRoundTrip` (currently ~L162-181). Old lines:
```lua
        for row=1,3 do for col=1,2 do f:write({row*10+col, row+col}, {row=row, col=col}) end end
```
```lua
          assert(cell[1] == row*10+col and cell[2] == row+col, 'csv roundtrip at '..row..','..col)
```
New:
```lua
        for row=1,3 do for col=1,2 do f:write({row*10+col + 0.123456789, row+col}, {row=row, col=col}) end end
```
```lua
          assert(cell[1] == row*10+col + 0.123456789 and cell[2] == row+col, 'csv roundtrip at '..row..','..col)
```
It fails before the fix: `11.123456789` came back as `11.1235`.

L3. Add after `CsvRoundTrip`:
```cpp
TEST_F(LuaBinaryTest, CsvToBinRejectsTrailingGarbage) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(md1() + "local f = db:open_file('bin_a', 'w', md)\nf:close()\n");  // writes bin_a.toml
    {
        std::ofstream csv(sandbox / "bin_a.csv");
        csv << "row,v\n1,9.99abc\n2,1\n3,1\n";
    }
    expect_lua_error(lua, "db:csv_to_bin('bin_a')\n", "Cannot csv_to_bin: invalid float value '9.99abc' for label 'v'");
}
```
It fails before the fix, because the script succeeds (`FAIL() << "expected script to throw"`). `md1()` is the fixture's 1×3 metadata with label `v` and dimension `row`, so the expected header is `row,v`.

### Julia — `bindings/julia/test/test_csv_converter.jl`

J1. "Float precision" testset (currently ~L197-218). Old: `@test lines[2] == "1,1.23457"` → new: `@test lines[2] == "1,1.23456789"`.

J2. "Non-numeric data value" testset (currently ~L439-449). Old:
```julia
            @test_throws Quiver.DatabaseException Quiver.Binary.csv_to_bin(path)
```
New:
```julia
            exc = @test_throws Quiver.DatabaseException Quiver.Binary.csv_to_bin(path)
            @test exc.value.msg == "Cannot csv_to_bin: invalid float value 'abc' for label 'val1'"
```

J3. Add a testset right after "Non-numeric data value":
```julia
    @testset "Trailing garbage data value" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col,val1,val2\n1,1,1.0,9.99abc\n")
            exc = @test_throws Quiver.DatabaseException Quiver.Binary.csv_to_bin(path)
            @test exc.value.msg == "Cannot csv_to_bin: invalid float value '9.99abc' for label 'val2'"
        finally
            cleanup_binary_file(path)
        end
    end
```
All three fail before the fix. J1 fails because of the `1.23457` text. J2 and J3 fail because the message differs: J2 gets the bare `stod` text, and J3 gets the later dimension error on the missing second row.

### Dart / Python / JS

None. The binary subsystem is not exposed there (root design decision).

### Existing tests that must still pass unchanged

- Every other `CSVConverterFixture.*` test. The values they use print the same under `to_chars` as under `{:.6g}`.
- `DatabaseCSV.*` (import/export), including `ImportCSV_DecimalCommaLocale_ReadsExportedFloats`, which now exercises `utils::parse_float`.
- `LuaBinaryTest.*` and the other `BinaryCApiCSVFixture.*` tests.

No new schema files.

## Docs and changelog

D1. **Root `CLAUDE.md`, "One scalar typing policy lives in C++"** (currently ~L94-97). Old:
> `import_csv` writes through a raw `INSERT`, so it applies the rule to CSV text itself: `parse_integer` / `parse_float` (`src/database_csv_import.cpp`) accept a cell only if it parses whole, so `1.5` is not an INTEGER and `9.99abc` / `1,5` are not REALs — in the "C" locale's number format whatever locale the host process set, since export always writes `.`.

New:
> `import_csv` writes through a raw `INSERT`, so it applies the rule to CSV text itself: `parse_integer` (`src/database_csv_import.cpp`) and `utils::parse_float` (`src/utils/number.h`) accept a cell only if it parses whole, so `1.5` is not an INTEGER and `9.99abc` / `1,5` are not REALs — in the "C" locale's number format whatever locale the host process set, since export always writes `.`. `csv_to_bin` reads its data cells through the same `utils::parse_float`.

(Keep the existing line wrapping style, about 100 columns.)

D2. **Root `CLAUDE.md`, "One CSV parser, one CSV emitter"** (currently ~L280-282). Old sentence:
> The binary subsystem's `CSVConverter` (`bin_to_csv`/`csv_to_bin`) is not on this path: it splits and joins on `,` and never quotes, so a label holding a comma does not round-trip.

New:
> The binary subsystem's `CSVConverter` (`bin_to_csv`/`csv_to_bin`) is not on this path: it splits and joins on `,` and never quotes, so a label holding a comma does not round-trip. Its numbers do share the stack: data cells are written by `utils::append_number` and read by `utils::parse_float`, the same pair `export_csv`/`import_csv` use, so a value round-trips exactly in every host locale.

D3. **Root `CLAUDE.md`, Build System, macOS floor** (currently ~L385-386). Old: "libc++ marks the floating-point `std::to_chars` used by `database_csv_export.cpp` and `lua_runner.cpp` unavailable below it". New: "libc++ marks the floating-point `std::to_chars` used by `database_csv_export.cpp`, `lua_runner.cpp` and `binary/csv_converter.cpp` (all through `utils::append_number`) unavailable below it". Make the same list edit in the comment at the top of `cmake/Platform.cmake` (currently L3-4: `libc++ marks the floating-point std::to_chars used by` / `database_csv_export.cpp and lua_runner.cpp unavailable before macOS 13.3`) → `... used by database_csv_export.cpp, lua_runner.cpp and binary/csv_converter.cpp unavailable before macOS 13.3`, re-wrapping the comment lines as needed.

D4. **`src/CLAUDE.md`, File Map** (currently ~L53). Old:
```
  utils/number.h          # quiver::utils::append_number -- std::to_chars shortest round-trip
```
New:
```
  utils/number.h          # quiver::utils::append_number (std::to_chars shortest round-trip) and
                          # parse_float (its whole-cell, host-locale-proof reader)
```

D5. **`src/CLAUDE.md`, "One scalar typing policy" bullet** (currently ~L428-430). Old:
> `import_csv` is the third enforcer, on CSV text: its `parse_integer` / `parse_float` (`database_csv_import.cpp`) take a cell only if it parses whole, so a policy change must reach them too.

New:
> `import_csv` is the third enforcer, on CSV text: its `parse_integer` (`database_csv_import.cpp`) and `utils::parse_float` (`utils/number.h`, shared with `csv_to_bin`) take a cell only if it parses whole, so a policy change must reach them too.

D6. **`src/CLAUDE.md`, Binary Subsystem** (currently ~L693). Old:
> - `CSVConverter` class (composition, no Pimpl): `bin_to_csv(path, aggregate)`, `csv_to_bin(path)`

New:
> - `CSVConverter` class (composition, no Pimpl): `bin_to_csv(path, aggregate)`, `csv_to_bin(path)`. Data cells are written by `utils::append_number` and read by `utils::parse_float` (the whole cell must parse, in the "C" locale's format; `null` is NaN), so bin → csv → bin is exact in every host locale. A bad cell throws `Cannot csv_to_bin: invalid float value '<v>' for label '<label>'`.

D7. `bindings/js/src/lua-api.ts` documents `db:bin_to_csv` / `db:csv_to_bin` by name only and makes no claim about number formatting. **No change.** `docs/*.md`, the READMEs, `tests/CLAUDE.md` and `bindings/julia/CLAUDE.md` do not mention CSVConverter number handling. **No change.**

D8. **`CHANGELOG.md`, under `## [0.11.0] — unreleased`.**

Under `### Changed`, append after the `export_csv()` quoting entry (the one ending `*Adapt:* regenerate golden files and any byte-for-byte comparisons over exported CSVs.`), with one blank line before it:

```markdown
- **BREAKING — `bin_to_csv` writes values at full precision, and `csv_to_bin` rejects a data cell
  that is not a whole number.** `bin_to_csv` wrote each value with 6 significant digits, so
  `1.23456789` became `1.23457` and a bin → csv → bin round trip silently changed the data. It now
  writes the shortest text that reads back to the same double, as `export_csv()` does. `csv_to_bin`
  used `std::stod`, which reads the longest valid prefix, so a cell `9.99abc` (or `1.5 ` with a
  trailing space) was stored as `9.99`. The whole cell must now parse, and a bad cell reports
  `Cannot csv_to_bin: invalid float value '<v>' for label '<label>'` instead of the bare `stod` /
  `invalid stod argument` text. `null` still reads as a missing value.

  *Adapt:* regenerate golden files and byte-for-byte comparisons over `bin_to_csv` output; fix CSV
  files that relied on a truncated cell; update any matcher on the old `stod` messages.
```

Under `### Fixed`, insert directly after the `import_csv()` locale entry (the one ending `` `export_csv()` writes, was rejected. ``):

```markdown
- **`csv_to_bin` reads numbers the same way in every host locale and on every platform.** Under a
  decimal-comma C locale (e.g. Python's `locale.setlocale(locale.LC_ALL, "")` on a pt-BR machine,
  then `db:csv_to_bin` through a `LuaRunner`) a data cell `1.5` was read as `1`, and on Linux and
  macOS a subnormal value such as `1e-310`, which `bin_to_csv` writes, was rejected. It now uses
  the same parser as `import_csv()`.
```

No manifest version bump (0.11.0 is already the unreleased minor).

## Verification

From the repo root (`C:\Development\Quiver\quiver1`), in order:

1. `cmake --build build --config Debug`. It must compile with no new warnings in `src/utils/number.h`, `src/database_csv_import.cpp` or `src/binary/csv_converter.cpp`.
2. `./build/bin/quiver_tests.exe --gtest_filter='CSVConverterFixture.*'`. All pass, including the new `TrailingGarbageDataValue`, `NonNumericCellPastLastLabel`, `RoundTripIsLossless` and `DecimalCommaLocaleReadsWrittenFloats` (the last may print SKIPPED only on a machine without a decimal-comma locale; on Windows it must run), and the changed `FloatPrecision`, `NonNumericDataValue` and `EmptyDataField`.
3. `./build/bin/quiver_tests.exe --gtest_filter='DatabaseCSV.*:LuaBinaryTest.*:LuaRunner*Csv*'`. All pass, including `DatabaseCSV.ImportCSV_DecimalCommaLocale_ReadsExportedFloats` (now through `utils::parse_float`) and the new `LuaBinaryTest.CsvToBinRejectsTrailingGarbage`.
4. `./build/bin/quiver_c_tests.exe --gtest_filter='BinaryCApiCSVFixture.*'`. All pass, including the new `CsvToBinTrailingGarbageReportsMessage` and the strengthened `RoundTrip`.
5. `bindings/julia/test/test.bat test_csv_converter.jl` (the argument makes `runtests.jl` include only that file), then `bindings/julia/test/test.bat` for the whole suite. Both pass. No generator run: the C API did not change.
6. Optional regression proof: `git stash push -- src`, run `cmake --build build --config Debug`, and rerun steps 2-4. The new and changed tests listed above must fail. Then `git stash pop` and rebuild.
7. `scripts/format.bat`. Then re-run `git diff --stat` and make sure only the intended files changed and no `.bat` file shows up.
8. `scripts/test-all.bat`. Steps 1-6 (C++, C API, Julia, Dart, JS, Python) must pass. Step 7, the CLI smoke test, currently fails at HEAD because `example/example1.lua` was deleted. That is plan 65's fix, not this plan's, so a failure there is expected until 65 lands.

## Acceptance criteria

- [ ] `src/utils/number.h` holds `inline std::optional<double> parse_float(std::string)`, with the exact body of the former import copy and the includes it needs.
- [ ] `src/database_csv_import.cpp` has no `parse_float` definition, calls `utils::parse_float` at its four REAL sites, includes `utils/number.h`, and no longer includes `<cerrno>`, `<clocale>`, `<cmath>` or `<cstdlib>`.
- [ ] `src/binary/csv_converter.cpp` has no `std::stod`, no `fmt::` and no `<spdlog/fmt/fmt.h>`. `read_line` calls `utils::parse_float` and throws `Cannot csv_to_bin: invalid float value '<v>' for label '<label>'` (the label clause is dropped only past the last label). `build_line` writes through `utils::append_number`.
- [ ] `grep -rn "std::stod" src` is empty.
- [ ] C++ tests T3-T7, C API tests C1-C2, Lua tests L2-L3 and Julia tests J1-J3 exist and pass, and each fails when the `src/` change is reverted (a decimal-comma locale is needed for T7).
- [ ] Root `CLAUDE.md` (D1-D3), `cmake/Platform.cmake` comment (D3), and `src/CLAUDE.md` (D4-D6) are updated.
- [ ] CHANGELOG 0.11.0 has the BREAKING `### Changed` entry and the `### Fixed` entry from D8.
- [ ] No C API signature, FFI declaration, binding wrapper or `.bat` file changed.

## Pitfalls

- **`EXPECT_DOUBLE_EQ` hides the bug.** It allows 4 ULPs, and `0.3` vs `0.1 + 0.2` is 1 ULP. Every round-trip assertion added or changed here uses `EXPECT_EQ` (Lua `==`, and Julia's `==` in the string checks). Do not "tidy" them to `EXPECT_DOUBLE_EQ` or `≈`.
- **`inline` is required** on `parse_float` in the header. Four translation units include `number.h`, so a non-inline definition is an ODR/link error, and `static` would silently duplicate it per TU.
- **Keep the `"null"` test before `parse_float`.** `parse_float("null")` returns nullopt, so reordering turns every NaN cell into an error.
- **Do not drop the label bounds guard** in `read_line`. Until plan 14's row-width check runs before conversion, a too-wide row reaches the throw with `row.data.size() == labels.size()`. `NonNumericCellPastLastLabel` catches the regression (an MSVC Debug assert, or garbage in Release).
- **The message text is pinned in four layers** (C++ `EXPECT_STREQ`, C API `EXPECT_STREQ`, Lua substring, Julia `==`). If you change the wording, change all of them. Later plans (14) must keep it.
- **Locale test portability.** On Windows `setlocale(LC_NUMERIC, "pt-BR")` succeeds, so the test runs. On Linux/macOS CI it may `GTEST_SKIP`. The `RestoreNumericLocale` guard must be declared before the switch so the locale is restored even when an assertion fails. It changes only the C locale; the C++ global locale (iostreams, toml++) is untouched, and the metadata TOML holds no floats.
- **Behaviour intentionally unchanged:** `strtod` still skips *leading* whitespace and accepts `inf`, `nan` and hex floats, exactly as `std::stod` did. Only trailing characters and the locale handling change. A CRLF file still works on Windows, because the CSV is opened in text mode, which strips CR. On POSIX it already failed at `validate_header` (`val2\r`), so that is not a new rejection.
- **`to_chars` output differs from `{:.6g}` only when 6 digits were not enough.** No existing test other than `FloatPrecision` (C++ and Julia) depends on the old text. Grep `1.23457` after the change: the only hit left must be the historical `CHANGELOG.md` `export_csv` entry.
- **Line endings.** No `.bat` file is touched. If `scripts/format.bat` or an editor rewrites one, restore CRLF. `.cpp`/`.h`/`.jl` stay LF (`.gitattributes`).
- `bindings/julia/src/c_api.jl` is generated. Do not touch it; nothing in the C API changed.

## Out of scope

- The row-width (field-count) check in `read_line`, the shared split/header/dimension helpers, and making the `CSVConverter` constructor private: plan **14**.
- `import_csv`'s `convert_cell` restructuring: plan **58**. The `<cstdio>` and other unused CSV includes: plan **61**.
- Time-of-day and parent-aware datetime handling in `build_datetime_string_from_time_dimension_values`: plans **08** / **10**.
- Quoting in `CSVConverter` (a label holding a comma still does not round-trip), a documented limit in the root "One CSV parser, one CSV emitter" decision, which is left as is.
- Line or row numbers in the csv_to_bin error message: not needed; plan 14 may add a row counter together with its width check.
- `lua-api.ts` wording about CSV number formats: nothing there is wrong; Lua reference accuracy is owned by plans **43** / **44**.
