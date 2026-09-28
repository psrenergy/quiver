# 08 — Binary: parent-aware time-coordinate validation and time-of-day-preserving offsets

**Batch** 2 · **Severity** high · **Breaking** yes: C++ callers of `TimeProperties::datetime_to_int` (removed) and `TimeProperties::add_offset_from_int` (new contract); `bin_to_csv`/`csv_to_bin` users whose finest time dimension is monthly/yearly with a mid-period `initial_datetime`, or hourly with a sub-hour start (row labels change); weekly layouts (the initial value of their daily/hourly child changes) · **Size** M · **Layers** C++ core (binary subsystem), C API tests, Lua tests, Julia tests, docs

**Depends on** none · **Overlaps with** 09 (changes the contract 09's rebase relies on; read the 09 note under Overlaps below), 10 (same subsystem; its restore-rule bug limits which walks this plan can test), 11 (rewrites the tail of `from_toml_content` this plan reorders), 12 (deletes builders in the same files), 13/14 (add tests to `tests/test_csv_converter.cpp`), 45 (adds tests to `tests/test_lua_binary.cpp`)

### Overlaps in detail

- **09. It must adapt when it runs.** Plan 09 was written against the old contract, where `add_offset_from_int` shifts by `value - initial_value`. Its rebase in the `ExpressionAggregate` constructor loops over the *remaining* output time dimensions: `output_meta_.initial_datetime = dim.time->add_offset_from_int(output_meta_.initial_datetime, 1);`. Under this plan, `add_offset_from_int(base, 1)` is the start of *base's own* period of that frequency. So that loop floors to the promoted child's period, and 09's `year × month` from 2025-03-01 example stays at 2025-03-01 instead of moving to 2025-01-01. Under the new contract the rebase is one call on the **reduced** dimension: `output_meta_.initial_datetime = operand_meta.dimensions[<reduced index>].time->add_offset_from_int(operand_meta.initial_datetime, 1);`. That floors to the start of the reduced period (Jan 1 for yearly, the 1st for monthly, the day for weekly/daily), which is the policy verifier's rule that 09's maintainer notes adopt. 09's "rebase before derive" pitfall stops mattering, because the new contract reads no `initial_value`. The new contract is written into `include/quiver/binary/time_properties.h` and `src/AGENTS.md` by this plan. **Tell whoever runs 09.**
- **09 also moves this plan's initial-value function.** After this plan, the initial values are set by `set_time_dimension_initial_values(quiver::BinaryMetadata&)`, in the anonymous namespace of `src/binary/binary_metadata.cpp`. It replaces `compute_time_dimension_initial_values` and the assignment loop. 09 moves its body into a `BinaryMetadata` member.
- **10.** The restore rule in `next_dimensions` (and `ExpressionAggregate::compute_row`) looks only at the immediate parent. Any walk with **three or more** time levels and a non-1 innermost initial value therefore skips or invents cells. This plan's full-cell walks are two-level only, and its three-level tests write named cells. 10's "keep the start at midnight" caveat stops applying once this plan lands. 10 also flags a yearly × monthly × daily file from 2024-02-29 that cannot be written past year 1, and says it "belongs with plan 08's add_offset_from_int work". This plan fixes that case and tests it (`LeapDayStartUnderYearlyMonthlyDaily`).
- **11.** It moves the tail of `from_toml_content` into a shared `build_metadata`. It must keep this plan's order: `metadata.validate();`, then `set_time_dimension_initial_values(metadata);`.
- **12.** It deletes `add_dimension`/`add_time_dimension` from `binary_metadata.h/.cpp`. Those are different functions from the ones this plan edits.
- **13/14.** They edit `src/binary/csv_converter.cpp`. This plan does not touch that file; it only changes what `add_offset_from_int` returns to it. Both plans add tests to `tests/test_csv_converter.cpp`, and so does this one; the tests are different.
- **45.** It adds tests to `tests/test_lua_binary.cpp`. This plan adds a different test there.

---

## Why

A `.qvr` file's time dimensions form a chain: each inner time dimension's `parent_dimension_index` points at the previous one. An inner coordinate is the value's **position inside its parent's period**: day of month, day of year, hour of the week, and so on. `BinaryMetadata::validate_time_dimension_sizes` admits eight parent/child pairs (Weekly may only be the outermost dimension, and frequencies must ascend):

- Monthly under Yearly
- Daily under Yearly, Monthly or Weekly
- Hourly under Yearly, Monthly, Weekly or Daily

`first_dimensions` and `next_dimensions` walk those positions. `BinaryFile::read`/`write` then rebuild a datetime from the coordinates and require each inner coordinate to match what that datetime implies. Three defects make read/write reject cells that the metadata validator and the iterator both say exist. Every reader and writer runs this check: `CSVConverter::bin_to_csv`/`csv_to_bin`, `Expression::save` and `ExpressionFile::compute_row` all abort at the first rejected cell.

**1. The consistency check ignores the parent.** `src/binary/time_properties.cpp` (currently ~L55-59):

```cpp
    case TimeFrequency::Daily:
        return static_cast<unsigned>(ymd.day());  // 1-31
    case TimeFrequency::Hourly:
        int64_t hour = std::chrono::floor<std::chrono::hours>(datetime - date).count() + 1;  // 0-23 -> 1-24
        return hour;
```

`BinaryFile::validate_dimension_values` (`src/binary/binary_file.cpp`, currently ~L250-256) compares every inner coordinate against it:

```cpp
            int64_t expected_value = dims.at(dim.name);
            int64_t resulting_value = dim.time->datetime_to_int(datetime);
            if (expected_value != resulting_value) {
                throw std::invalid_argument("Invalid values for time dimensions: dimension '" + dim.name +
```

"Day" therefore always means day-of-month and "hour" always means hour-of-day, whatever the parent is. The parent-aware inverse already exists, but only in `compute_time_dimension_initial_values` (`src/binary/binary_metadata.cpp`, currently ~L20-99), which sets the initial values. So one rule has two copies, and only one of them is right.

**2. Weeks are counted from January 1.** `day_of_week` (`src/binary/binary_utils.h`, currently ~L25-28) is `(day_of_year - 1) % 7 + 1`. `add_offset_from_int` walks whole 7-day blocks from `initial_datetime`, and 365 = 52·7 + 1, so the two grids disagree as soon as a start is not January 1 or a file crosses Dec 31.

**3. Calendar steps drop the time of day and overflow at month ends.** `src/binary/time_properties.cpp` (currently ~L69-72):

```cpp
    case TimeFrequency::Yearly:
        return std::chrono::sys_days{ymd + std::chrono::years{relative_value}};
    case TimeFrequency::Monthly:
        return std::chrono::sys_days{ymd + std::chrono::months{relative_value}};
```

These steps move from `initial_datetime`'s own day, by `value - initial_value`. From January 31, one month is `2025-02-31`, which normalizes to March 3. From 2024-02-29, one year is `2025-02-29`, which becomes March 1 before the monthly step runs.

**Reproduction.** Each case was run against the current build: a Lua script through `build/bin/quiver_cli.exe`, on a scratch database outside the repo.

| Layout (sizes), `initial_datetime` | Coordinate | Result today |
| --- | --- | --- |
| monthly(12) × hourly(744), 2025-01-01T00:00 | month=1, hour=25 (Jan 2, 00:00) | `Invalid values for time dimensions: dimension 'hour' has value 25 but the resulting datetime implies 1` |
| yearly(1) × daily(365), 2025-01-01 | year=1, day=32 (Feb 1) | `... 'day' has value 32 ... implies 1` |
| weekly(2) × daily(7), 2025-01-01 | week=2, day=1 (Jan 8) | `... 'day' has value 1 ... implies 8` |
| weekly(2) × hourly(168), 2025-01-01 | week=1, hour=25 | `... 'hour' has value 25 ... implies 1` |
| yearly(1) × hourly(8760), 2025-01-01 | year=1, hour=25 | `... 'hour' has value 25 ... implies 1` |
| monthly × daily × hourly, 2025-01-01T06:00 | (1,1,7), **the file's own first cell** | `... 'hour' has value 7 ... implies 1` |
| yearly(2) × monthly(12), 2025-01-31 | year=1, month=2 | `... 'month' has value 2 ... implies 3` |
| yearly × monthly × daily, 2024-02-29 | (2,3,1) = 2025-03-01 | `... 'day' has value 1 ... implies 4` |
| monthly(4) only, 2025-01-31, `bin_to_csv` | row labels | `2025-01-31 2025-03-03 2025-03-31 2025-05-01` |
| `quiver.metadata{... frequencies={'monthly','yearly'}}` | — | `YEARLY frequency not implemented. This function should only be used for inner time dimensions.` |
| same with `{'daily','daily'}` | — | `Invalid parent frequency daily for DAILY dimension.` |

The last two rows come from `from_toml_content`, which computes initial values **before** it validates (currently ~L334-344):

```cpp
    // Compute and set initial values for time dimensions
    std::vector<int64_t> initial_values =
        compute_time_dimension_initial_values(metadata.dimensions, metadata.initial_datetime);
    ...
    metadata.validate();
```

As a result, the validator's messages ("must be ordered", "must be unique") are never reached for those layouts.

Principles violated: a file the metadata accepts must be writable and readable in full, and one rule should live in one place (root AGENTS.md, "Simple solutions over complex abstractions"; src/AGENTS.md calls `next_dimensions` the "single source of truth for `.qvr` traversal", and validation disagrees with it).

## Constraints and decisions

**Maintainer decisions for this item (binding):**
1. "Anchor Weekly parents on initial_datetime (not Jan 1)." This plan reads that as: *a week is seven calendar days, starting at midnight of `initial_datetime`'s day*. So the day under a weekly parent starts at 1, and the hour under a weekly parent starts at hour-of-day + 1. It anchors on the **day**, not the exact instant, because hours under a Daily parent are clock hours. A weekly × daily × hourly file's weeks must be whole calendar days, so weekly × hourly has to use the same grid.
2. "Validate time-dimension metadata before computing initial values in from_toml_content." The existing `metadata.validate()` call moves above the initial-value computation. That makes the calculation's `std::logic_error` branches unreachable, so they are deleted.
3. "Tests must walk every cell of all 8 reachable parent/child pairs." Section Tests has one full walk (write every cell, then read every cell) per pair.

**Correction to the proposal (and both verifiers).** The finding proposes one parent-aware inverse, plus making `add_offset_from_int` keep the time of day for Yearly/Monthly while it still steps from `initial_datetime` by `value - initial_value`. That fixes the rows above that the finding lists, but it leaves the calendar-overflow bugs. I modelled the proposed mechanism in a Python script, with the same parent-aware inverse and weekly anchor. The script was a scratch file used while planning and is not in the repo; the tests below pin every case it found. The model still rejects:
- yearly × monthly from 2025-01-31, at (1,2), because Jan 31 + 1 month is March 3;
- yearly × monthly from 2024-03-31, at (1,4);
- yearly × monthly × daily from 2024-02-29, at (2,3,1);
- **yearly × daily and yearly × hourly from any date after February in a leap year, at the first cell of year 2** (from 2024-03-15, (2,1) lands on 2024-12-31, because the offset is taken from a leap-year day number).

Three of the eight pairs (yearly × monthly, yearly × daily, yearly × hourly) would therefore still fail for ordinary starts. Relative offsets cannot be patched: correcting an overflowing month needs a later daily/hourly step, and when none follows the only option is clamping, which breaks the daily step when one does follow.

This plan instead gives `add_offset_from_int` a **period-start contract**: floor the base to the start of its own period, then add `value - 1` periods. Folded outermost-first from `initial_datetime`, every inner step begins at its parent's period start (Jan 1, the 1st, midnight), so no calendar step can overflow and `initial_value` plays no part. A coordinate then names a **cell**, and its datetime is the cell's start. A second scratch model, of this contract, checked 12 layouts (all 8 pairs plus four 3- and 4-level chains) against 20 starts: leap days, month ends, Dec 31 23:00, 1960, and 15 random instants. For every in-bounds coordinate, validation accepts exactly the coordinates whose inner values fit the real length of their parent period, with no mismatches. Every cell count in Tests comes from that model.

The finding's time-of-day symptom is fixed by this contract: the non-midnight first cell (1,1,7) maps to 06:00. Two visible consequences are recorded as BREAKING in the changelog:
- A row label is the cell start. A monthly-only file starting 2025-01-15 is labelled `2025-01-01, 2025-02-01, …`, not `2025-01-15, …`.
- An hourly cell starts on the hour, so a start with minutes is floored.

**Other constraints:**
- Binary and expression are exposed in the C API, Julia and Lua only (root Design Decisions). There is no Dart/Python/JS code or test to change, and no FFI declaration changes because no C signature changes. `c_api.jl` is **not** regenerated.
- src/AGENTS.md "Performance Bottlenecks": `validate_dimension_values` is ~19% of the hot path. The new code keeps one datetime fold per call and adds no allocation.
- src/AGENTS.md: internal helpers in `src/binary/binary_utils.h` are header-only `inline`, and the new position function follows that convention. Root principle "Delete unused code, do not deprecate": `TimeProperties::datetime_to_int` and `day_of_week` are deleted, not kept.
- Root "C++ Error Message Patterns": the binary subsystem's messages are a documented pre-pattern exception. The `Invalid values for time dimensions: ...` text is kept **verbatim**, and nothing in the repo pins it.
- Root "Changelog": BREAKING entries go under `## [0.12.0] — unreleased` and say what the caller must do. No manifest bump is needed (0.12.0 is already the unreleased minor).
- Root "Self-Updating": update `src/AGENTS.md`, plus one root Design Decision bullet so nobody reintroduces a Jan-1 week or relative calendar offsets.

**Alternatives rejected:**
- *Finding as written (relative offsets + time of day):* leaves the leap-year and month-end failures above, in three of the eight pairs.
- *Facts verifier's week grid from `floor(initial) - (initial_value - 1)` days with the Jan-1 initial value kept:* overridden by maintainer decision 1.
- *Policy verifier's `value <= period length` check at the parent-period start:* correct under the period-start contract too, but it costs one datetime build per inner dimension on the hot path and a second copy of `dimension_sizes_at_values`' length switch. The inverse also yields the initial values, so it is the single shared function 09 depends on.
- *Anchoring weeks on the exact instant (06:00):* makes weekly × hourly disagree with weekly × daily × hourly (see decision 1).
- *Keeping `datetime_to_int` as a public `TimeProperties` method with extra parameters, or adding a public `BinaryMetadata` member:* a position depends on the parent's frequency and, for weeks, on `initial_datetime`, which is metadata, not one `TimeProperties`. A public C++ method with no C binding is also what plan 12 deletes. It stays internal instead.
- *Keeping the outermost dimension relative to `initial_datetime` so single-dimension labels keep the 15th:* it needs a month-end clamp plus a second rule for inner steps.

## Changes

Anchor every edit on the quoted code. Line numbers are "currently ~" at HEAD 58dfe7a.

### 1. `include/quiver/binary/time_properties.h`: delete `datetime_to_int`, document the new contract

Current (currently ~L28-30):

```cpp
    int64_t datetime_to_int(std::chrono::system_clock::time_point datetime) const;
    std::chrono::system_clock::time_point add_offset_from_int(std::chrono::system_clock::time_point base_datetime,
                                                              int64_t value) const;
```

New:

```cpp
    // Start of period number `value`, counting the period of this frequency that holds base_datetime as 1:
    // base_datetime floored to January 1, the 1st of its month, its day (Weekly and Daily) or its hour, plus
    // value - 1 periods. Folded over a file's time dimensions outermost first, starting at initial_datetime,
    // it yields the start of the cell a coordinate names. initial_value plays no part.
    std::chrono::system_clock::time_point add_offset_from_int(std::chrono::system_clock::time_point base_datetime,
                                                              int64_t value) const;
```

Why: `datetime_to_int` is parent-unaware and has exactly one caller (`validate_dimension_values`), which switches to `position_in_parent`. Grep confirmed it is not in the C API, Lua or any binding.

### 2. `src/binary/time_properties.cpp`: delete `datetime_to_int`, rewrite `add_offset_from_int`

a) Delete the whole `int64_t TimeProperties::datetime_to_int(...) const { ... }` function (currently ~L43-61, starting `int64_t TimeProperties::datetime_to_int(std::chrono::system_clock::time_point datetime) const {`).

b) Replace `TimeProperties::add_offset_from_int` (currently ~L63-80, starting `TimeProperties::add_offset_from_int(std::chrono::system_clock::time_point base_datetime, int64_t value) const {` and containing `int64_t relative_value = value - this->initial_value;`) with:

```cpp
std::chrono::system_clock::time_point
TimeProperties::add_offset_from_int(std::chrono::system_clock::time_point base_datetime, int64_t value) const {
    // Flooring first is what keeps calendar steps exact: an inner dimension's base is already the start of
    // its parent's period (a 1st at midnight), so January 31 + one month can never become March 3.
    auto date = std::chrono::floor<std::chrono::days>(base_datetime);
    auto ymd = std::chrono::year_month_day{date};
    int64_t steps = value - 1;
    switch (frequency) {
    case TimeFrequency::Yearly:
        return std::chrono::sys_days{(ymd.year() + std::chrono::years{steps}) / std::chrono::January / 1};
    case TimeFrequency::Monthly:
        return std::chrono::sys_days{(ymd.year() / ymd.month() + std::chrono::months{steps}) / 1};
    case TimeFrequency::Weekly:
        return date + std::chrono::weeks{steps};
    case TimeFrequency::Daily:
        return date + std::chrono::days{steps};
    case TimeFrequency::Hourly:
        return std::chrono::floor<std::chrono::hours>(base_datetime) + std::chrono::hours{steps};
    }
}
```

c) Remove the two includes this file no longer uses: `#include "binary_utils.h"` and `#include "quiver/binary/time_constants.h"` (currently ~L3-4). Keep `#include "quiver/binary/time_properties.h"` and `#include <stdexcept>` (`frequency_from_string` throws).

Why: this is the period-start contract (Constraints). For the outermost dimension the floor is its anchor: the period holding `initial_datetime`, where a week starts on `initial_datetime`'s day. That is maintainer decision 1 for the outermost Weekly. For every inner dimension the floor is a no-op, because its base is a coarser period start and therefore aligned. Inner dimensions still produce the correct time of day: an hourly coordinate `h` adds `h - 1` hours to midnight.

### 3. `src/binary/binary_utils.h`: replace `day_of_week` with the parent-aware `position_in_parent`

Current (currently ~L1-8 and ~L25-28):

```cpp
#include "quiver/binary/time_constants.h"

#include <chrono>
#include <cstdint>
#include <string_view>
```

```cpp
inline int64_t day_of_week(chrono::system_clock::time_point datetime) {
    int64_t day_of_year = quiver::day_of_year(datetime);
    return (day_of_year - 1) % quiver::time::MAX_DAYS_IN_WEEK + 1;  // 1-7 instead of 0-6
}
```

New: the includes become

```cpp
#include "quiver/binary/binary_metadata.h"
#include "quiver/binary/time_constants.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>
```

and `day_of_week` is replaced (keep `day_of_year` as is) by:

```cpp
// 1-based position of `datetime` inside the parent period of the inner time dimension at `index`: the inverse of
// TimeProperties::add_offset_from_int. validate() admits Monthly only under Yearly, Daily under Yearly, Monthly or
// Weekly, and Hourly under any of the four. A week is seven days counted from the day of
// metadata.initial_datetime, not from January 1: that is the grid add_offset_from_int walks, so it never restarts
// at a year boundary, and every cell starts on or after that day.
inline int64_t position_in_parent(const BinaryMetadata& metadata,
                                  size_t index,
                                  chrono::system_clock::time_point datetime) {
    const auto& time_properties = *metadata.dimensions[index].time;
    const auto parent_frequency = metadata.dimensions[time_properties.parent_dimension_index].time->frequency;
    const auto date = chrono::floor<chrono::days>(datetime);
    const auto ymd = chrono::year_month_day{date};

    if (time_properties.frequency == TimeFrequency::Monthly) {
        return static_cast<unsigned>(ymd.month());
    }

    int64_t days_into_parent = 0;  // under a Daily parent the hour is the whole position
    switch (parent_frequency) {
    case TimeFrequency::Yearly:
        days_into_parent = day_of_year(datetime) - 1;
        break;
    case TimeFrequency::Monthly:
        days_into_parent = static_cast<unsigned>(ymd.day()) - 1;
        break;
    case TimeFrequency::Weekly:
        days_into_parent = (date - chrono::floor<chrono::days>(metadata.initial_datetime)).count() %
                           quiver::time::MAX_DAYS_IN_WEEK;
        break;
    default:
        break;
    }

    if (time_properties.frequency == TimeFrequency::Daily) {
        return days_into_parent + 1;
    }
    const int64_t hour_of_day = chrono::floor<chrono::hours>(datetime - date).count();
    return days_into_parent * quiver::time::MAX_HOURS_IN_DAY + hour_of_day + 1;
}
```

Why: this is the one parent-aware position function 09 depends on. It has two callers (steps 4 and 5), and nothing else computes a position. `day_of_week`'s only callers were the Weekly branches of `compute_time_dimension_initial_values` (grep: `src/binary/binary_metadata.cpp` ~L52 and ~L77), which step 4 deletes. `binary_utils.h` stays internal: only `.cpp` files in `src/binary/` include it (`binary_file.cpp`, `binary_metadata.cpp`, `csv_converter.cpp`; `time_properties.cpp` stops in step 2c). Including `binary_metadata.h` creates no cycle.

### 4. `src/binary/binary_metadata.cpp`: one initial-value loop, run after `validate()`

a) Replace the whole anonymous-namespace function `compute_time_dimension_initial_values` (currently ~L20-99; it starts `std::vector<int64_t>\ncompute_time_dimension_initial_values(const std::vector<quiver::Dimension>& dimensions,` and ends with `return initial_values;\n}`) with:

```cpp
// Every time dimension starts at the cell holding initial_datetime: the outermost at 1, each inner one at the
// position of initial_datetime inside its parent's period.
void set_time_dimension_initial_values(quiver::BinaryMetadata& metadata) {
    for (size_t i = 0; i < metadata.dimensions.size(); ++i) {
        auto& time_properties = metadata.dimensions[i].time;
        if (!time_properties) {
            continue;
        }
        time_properties->set_initial_value(time_properties->parent_dimension_index == -1
                                               ? 1
                                               : quiver::position_in_parent(metadata, i, metadata.initial_datetime));
    }
}
```

Keep `constexpr std::string_view QUIVER_FILE_VERSION = "1";` above it, in the same anonymous namespace.

b) In `BinaryMetadata::from_toml_content`, delete the counter reset right after the dimension loop (currently ~L332):

```cpp
    time_dim_index = 0;
```

and replace the tail (currently ~L334-345):

```cpp
    // Compute and set initial values for time dimensions
    std::vector<int64_t> initial_values =
        compute_time_dimension_initial_values(metadata.dimensions, metadata.initial_datetime);
    for (auto& dim : metadata.dimensions) {
        if (dim.is_time_dimension()) {
            dim.time->set_initial_value(initial_values[time_dim_index]);
            time_dim_index++;
        }
    }

    metadata.validate();
    return metadata;
```

with:

```cpp
    // Validate first: an initial value is a position inside the parent's period, which exists only for the
    // parent/child layouts validate() accepts
    metadata.validate();
    set_time_dimension_initial_values(metadata);
    return metadata;
```

Leave everything above the dimension loop, and `metadata.dimensions.clear();`, untouched (plan 11 owns them). `time_dim_index` is still declared and used inside the loop (`frequencies[time_dim_index]`); only the reset and the second loop go.

Why: maintainer decision 2. `validate()` reads no initial value, and it needs only the `parent_dimension_index` values set in the loop. The four removed `std::logic_error` branches (`"YEARLY frequency not implemented..."`, `"WEEKLY frequency not implemented..."`, `"Invalid parent frequency ..."`, `"Unhandled frequency ..."`) become unreachable, so they go.

### 5. `src/binary/binary_file.cpp`: validate with `position_in_parent`

In `BinaryFile::validate_dimension_values`, replace the time block (currently ~L231-258, starting `if (metadata.number_of_time_dimensions() > 1) {` and containing `bool first = true;` and `dim.time->datetime_to_int(datetime)`) with:

```cpp
    if (metadata.number_of_time_dimensions() > 1) {
        // The start of the cell the coordinates name
        auto datetime = metadata.initial_datetime;
        for (const auto& dim : dimensions) {
            if (dim.is_time_dimension()) {
                datetime = dim.time->add_offset_from_int(datetime, dims.at(dim.name));
            }
        }

        // An inner value past the real length of its parent's period (day 30 of February, hour 700 of February)
        // spills into the next period, where its position is no longer the value given
        for (size_t i = 0; i < dimensions.size(); ++i) {
            const auto& dim = dimensions[i];
            if (!dim.is_time_dimension() || dim.time->parent_dimension_index == -1)
                continue;

            int64_t expected_value = dims.at(dim.name);
            int64_t resulting_value = position_in_parent(metadata, i, datetime);
            if (expected_value != resulting_value) {
                throw std::invalid_argument("Invalid values for time dimensions: dimension '" + dim.name +
                                            "' has value " + std::to_string(expected_value) +
                                            " but the resulting datetime implies " + std::to_string(resulting_value));
            }
        }
    }
```

Why: the `first` flag skipped the outermost time dimension. `parent_dimension_index == -1` says the same thing directly. The message is unchanged. `binary_file.cpp` already includes `binary_utils.h`.

### 6. Unchanged on purpose, but behaviour changes

- `dimension_sizes_at_values` (`src/binary/iteration.cpp`) and `CSVConverter::build_datetime_string_from_time_dimension_values` (`src/binary/csv_converter.cpp`) keep their `add_offset_from_int` loops. They now get the cell start: sizes are read from the right month/year in every case, and CSV labels are cell starts (see the changelog).
- The C API (`src/c/binary/*`), Lua (`src/lua_runner.cpp`) and Julia (`bindings/julia/src/binary/*`) copy `initial_value` out and call read/write. None of them calls `datetime_to_int` (grep confirmed), so there is no FFI or wrapper change. `bindings/julia/src/c_api.jl`: no regeneration.

## Tests

The shared start for the walks is `2025-03-15T06:00:00`: a Saturday, mid-March, mid-year, not at midnight. The count comments come from the model named under Constraints.

### C++: `tests/test_binary_time_properties.cpp`

a) Delete the whole `TimePropertiesDatetimeToInt` section (currently ~L84-153): its banner comment and the 11 tests `MonthlyJanuary`, `MonthlyMarch`, `MonthlyDecember`, `DailyFirst`, `DailyFifteenth`, `DailyThirtyFirst`, `HourlyMidnight`, `HourlyNoon`, `HourlyLastHour`, `YearlyThrows`, `WeeklyThrows`. They call the deleted method, for example `EXPECT_EQ(props.datetime_to_int(dt), 1);`. Their coverage moves to the cell walks in `test_binary_file.cpp`.

b) In `TimePropertiesAddOffset`, rename `NoOpWhenValueEqualsInitial` to `ValueOneIsThePeriodHoldingTheBase` and keep its body. Replace `NonOneInitialValue`, whose old assertion `EXPECT_EQ(ymd.day(), 10d);` pinned the relative contract, with:

```cpp
TEST(TimePropertiesAddOffset, IgnoresInitialValue) {
    // Periods count from the one holding the base, not from initial_value
    TimeProperties props{TimeFrequency::Daily, 5, 0};
    auto base = sys_days{2025y / January / 5d};
    EXPECT_EQ(props.add_offset_from_int(base, 10), sys_days{2025y / January / 14d});
}
```

c) Append at the end of the file:

```cpp
TEST(TimePropertiesAddOffset, MonthlyStartsFromTheFirstOfTheBaseMonth) {
    TimeProperties props{TimeFrequency::Monthly, 1, -1};
    auto base = sys_days{2025y / January / 31d} + hours{6};
    EXPECT_EQ(props.add_offset_from_int(base, 2), sys_days{2025y / February / 1d});  // not March 3
}

TEST(TimePropertiesAddOffset, YearlyStartsFromJanuaryFirstOfTheBaseYear) {
    TimeProperties props{TimeFrequency::Yearly, 1, -1};
    auto base = sys_days{2024y / February / 29d};
    EXPECT_EQ(props.add_offset_from_int(base, 2), sys_days{2025y / January / 1d});
}

TEST(TimePropertiesAddOffset, WeeklyStartsOnTheBaseDay) {
    TimeProperties props{TimeFrequency::Weekly, 1, -1};
    auto base = sys_days{2025y / March / 15d} + hours{6};
    EXPECT_EQ(props.add_offset_from_int(base, 2), sys_days{2025y / March / 22d});
}

TEST(TimePropertiesAddOffset, DailyStartsAtMidnight) {
    TimeProperties props{TimeFrequency::Daily, 1, 0};
    auto base = sys_days{2025y / March / 15d} + hours{6};
    EXPECT_EQ(props.add_offset_from_int(base, 1), sys_days{2025y / March / 15d});
}

TEST(TimePropertiesAddOffset, HourlyStartsOnTheHour) {
    TimeProperties props{TimeFrequency::Hourly, 1, 0};
    auto base = sys_days{2025y / March / 15d} + hours{6} + minutes{30};
    EXPECT_EQ(props.add_offset_from_int(base, 2), sys_days{2025y / March / 15d} + hours{7});
}
```

The file already has `using namespace std::chrono;`. The existing `YearlyAddsYears`, `MonthlyAddsMonths`, `WeeklyAddsWeeks`, `DailyAddsDays` and `HourlyAddsHours` start on Jan 1 at midnight and keep passing unchanged.

Fail before the fix:
- `IgnoresInitialValue` gives Jan 10.
- `MonthlyStartsFromTheFirstOfTheBaseMonth` gives March 3.
- `YearlyStartsFromJanuaryFirstOfTheBaseYear` gives 2025-03-01.
- `WeeklyStartsOnTheBaseDay`, `DailyStartsAtMidnight` and `HourlyStartsOnTheHour` keep 06:00 or 06:30.

### C++: `tests/test_binary_metadata.cpp`

After `TEST(BinaryMetadataFromTomlContent, InitialValuesHourlyOffset)` (currently ~L101-116) add:

```cpp
TEST(BinaryMetadataFromTomlContent, InitialValuesUnderWeeklyCountFromInitialDatetime) {
    // Saturday 2025-03-15 was day 4 of a week counted from January 1. A week now starts on the day of
    // initial_datetime, so the day starts at 1 and the hour at hour-of-day + 1.
    auto daily = BinaryMetadata::from_toml_content(R"(
version = "1"
dimensions = ["week", "day"]
dimension_sizes = [60, 7]
time_dimensions = ["week", "day"]
frequencies = ["weekly", "daily"]
initial_datetime = "2025-03-15T06:00:00"
unit = "MW"
labels = ["val"]
)");
    EXPECT_EQ(daily.dimensions[1].time->initial_value, 1);

    auto hourly = BinaryMetadata::from_toml_content(R"(
version = "1"
dimensions = ["week", "hour"]
dimension_sizes = [60, 168]
time_dimensions = ["week", "hour"]
frequencies = ["weekly", "hourly"]
initial_datetime = "2025-03-15T06:00:00"
unit = "MW"
labels = ["val"]
)");
    EXPECT_EQ(hourly.dimensions[1].time->initial_value, 7);
}
```

After `TEST(BinaryMetadataFromTomlContent, ErrorTimeDimensionsOutOfOrder)` (currently ~L151-163) add:

```cpp
TEST(BinaryMetadataFromTomlContent, InvalidFrequencyLayoutReportsTheValidatorMessage) {
    // validate() runs before the initial values are computed, so an inner yearly dimension or a repeated frequency
    // gets the validator's message instead of an std::logic_error from the initial-value calculation
    auto toml_with = [](const std::string& frequencies) {
        return "version = \"1\"\ndimensions = [\"a\", \"b\"]\ndimension_sizes = [12, 31]\n"
               "time_dimensions = [\"a\", \"b\"]\nfrequencies = " +
               frequencies + "\ninitial_datetime = \"2025-01-01T00:00:00\"\nunit = \"MW\"\nlabels = [\"val\"]\n";
    };
    auto message_of = [](const std::string& toml) -> std::string {
        try {
            BinaryMetadata::from_toml_content(toml);
        } catch (const std::runtime_error& e) {
            return e.what();
        }
        return "no exception";
    };
    EXPECT_EQ(message_of(toml_with(R"(["monthly", "yearly"])")),
              "Time dimension frequencies must be ordered from lowest to highest frequency.");
    EXPECT_EQ(message_of(toml_with(R"(["daily", "daily"])")),
              "Time dimension frequencies must be unique. Duplicate: daily");
}
```

Fail before the fix:
- The weekly test gets 4 and 79 (`(4 - 1) * 24 + 7`).
- The layout test fails with an uncaught `std::logic_error`, which `catch (const std::runtime_error&)` does not catch.

### C++: `tests/test_binary_file.cpp` (the eight full walks)

a) Add includes next to the existing ones: `#include <optional>`, `#include <quiver/binary/iteration.h>`, `#include <string>`, `#include <unordered_map>`, `#include <vector>`.

b) Inside `class BinaryTempFileFixture`, after `make_time_metadata()` (currently ~L41-50), add:

```cpp
    // One-label metadata whose dimensions are all time dimensions, each named after its frequency.
    static BinaryMetadata make_time_layout(const std::vector<std::string>& frequencies,
                                           const std::vector<int64_t>& sizes,
                                           const std::string& initial_datetime) {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", initial_datetime)
                                                .set("unit", "MW")
                                                .set("dimensions", frequencies)
                                                .set("dimension_sizes", sizes)
                                                .set("time_dimensions", frequencies)
                                                .set("frequencies", frequencies)
                                                .set("labels", {"val"}));
    }

    // Writes a distinct value to every cell first_dimensions/next_dimensions visits, then reopens the file and
    // reads each one back. Returns the number of cells.
    size_t write_and_read_every_cell(const BinaryMetadata& md) {
        std::vector<std::vector<int64_t>> cells;
        for (std::optional<std::vector<int64_t>> cell = first_dimensions(md); cell; cell = next_dimensions(md, *cell)) {
            cells.push_back(*cell);
        }
        auto dims_of = [&md](const std::vector<int64_t>& cell) {
            std::unordered_map<std::string, int64_t> dims;
            for (size_t i = 0; i < cell.size(); ++i) {
                dims[md.dimensions[i].name] = cell[i];
            }
            return dims;
        };
        {
            auto writer = BinaryFile::open_file(path, 'w', md);
            for (size_t n = 0; n < cells.size(); ++n) {
                writer.write({static_cast<double>(n)}, dims_of(cells[n]));
            }
        }
        auto reader = BinaryFile::open_file(path, 'r');
        size_t mismatches = 0;
        for (size_t n = 0; n < cells.size(); ++n) {
            if (reader.read(dims_of(cells[n]))[0] != static_cast<double>(n)) {
                ++mismatches;
            }
        }
        EXPECT_EQ(mismatches, 0u);
        return cells.size();
    }
```

c) Change the comment in `InvalidTimeDimensionCoordinates` (currently ~L452-453). Old:

```cpp
    // stage=2 (Feb), block=30 → Feb doesn't have 30 days: datetime implies day 30 but month says day 2
    // The datetime accumulation would compute Feb + 29 days offset = March 2nd, datetime_to_int(March) != 2
```

New:

```cpp
    // stage=2 (Feb), block=30: February 1 + 29 days is March 2, whose day of month (2) is not the 30 given
```

The assertion stays as is and still passes.

d) After `TEST_F(BinaryTempFileFixture, InitialDatetimeYear1960)` (ends currently ~L494), add a new section:

```cpp
// ============================================================================
// BinaryTimeLayouts -- every cell of each of the 8 parent/child pairs validate() accepts, from Saturday
// 2025-03-15T06:00:00: not midnight, not the start of any period. Two-level only: three-level mid-period walks
// depend on next_dimensions' restore rule (plan 10).
// ============================================================================

TEST_F(BinaryTempFileFixture, EveryCellMonthlyUnderYearly) {
    auto md = make_time_layout({"yearly", "monthly"}, {2, 12}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 3}));
    EXPECT_EQ(write_and_read_every_cell(md), 22u);  // March-December 2025, then all of 2026
}

TEST_F(BinaryTempFileFixture, EveryCellDailyUnderYearly) {
    auto md = make_time_layout({"yearly", "daily"}, {2, 366}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 74}));
    EXPECT_EQ(write_and_read_every_cell(md), 657u);  // 292 days left in 2025 + 365
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write({1.0}, {{"yearly", 1}, {"daily", 366}}), std::invalid_argument);  // 2025 has 365
}

TEST_F(BinaryTempFileFixture, EveryCellHourlyUnderYearly) {
    auto md = make_time_layout({"yearly", "hourly"}, {2, 8784}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 1759}));  // 73 * 24 + 6 + 1
    EXPECT_EQ(write_and_read_every_cell(md), 15762u);                  // 7002 hours left in 2025 + 8760
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write({1.0}, {{"yearly", 1}, {"hourly", 8761}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, EveryCellDailyUnderMonthly) {
    auto md = make_time_layout({"monthly", "daily"}, {12, 31}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 15}));
    EXPECT_EQ(write_and_read_every_cell(md), 351u);  // 2025-03-15 to 2026-02-28
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write({1.0}, {{"monthly", 2}, {"daily", 31}}), std::invalid_argument);  // April 31
}

TEST_F(BinaryTempFileFixture, EveryCellHourlyUnderMonthly) {
    auto md = make_time_layout({"monthly", "hourly"}, {12, 744}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 343}));  // 14 * 24 + 6 + 1
    EXPECT_EQ(write_and_read_every_cell(md), 8418u);                  // 402 hours of March + 334 days * 24
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write({1.0}, {{"monthly", 2}, {"hourly", 721}}), std::invalid_argument);  // April: 720
}

TEST_F(BinaryTempFileFixture, EveryCellDailyUnderWeeklyAcrossYearEnd) {
    // A week is seven days from the day of initial_datetime, so week 42 day 6 is 2026-01-01 and the grid does
    // not restart on January 1
    auto md = make_time_layout({"weekly", "daily"}, {60, 7}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 1}));
    EXPECT_EQ(write_and_read_every_cell(md), 420u);
}

TEST_F(BinaryTempFileFixture, EveryCellHourlyUnderWeeklyAcrossYearEnd) {
    auto md = make_time_layout({"weekly", "hourly"}, {60, 168}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 7}));
    EXPECT_EQ(write_and_read_every_cell(md), 10074u);  // 162 hours of week 1 + 59 * 168
}

TEST_F(BinaryTempFileFixture, EveryCellHourlyUnderDaily) {
    auto md = make_time_layout({"daily", "hourly"}, {3, 24}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 7}));
    EXPECT_EQ(write_and_read_every_cell(md), 66u);  // 18 + 24 + 24
}

TEST_F(BinaryTempFileFixture, MonthlyUnderYearlyFromTheThirtyFirst) {
    // January 31 + one month used to be March 3, so month 2 was rejected
    auto md = make_time_layout({"yearly", "monthly"}, {1, 12}, "2025-01-31T00:00:00");
    EXPECT_EQ(write_and_read_every_cell(md), 12u);
}

TEST_F(BinaryTempFileFixture, LeapDayStartUnderYearlyMonthlyDaily) {
    // From 2024-02-29, (2, 3, 1) is 2025-03-01: the missing 2025-02-29 must not shift it
    auto md = make_time_layout({"yearly", "monthly", "daily"}, {2, 12, 31}, "2024-02-29T00:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 2, 29}));
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
        writer.write({1.0}, {{"yearly", 1}, {"monthly", 2}, {"daily", 29}});
        writer.write({2.0}, {{"yearly", 2}, {"monthly", 2}, {"daily", 28}});
        writer.write({3.0}, {{"yearly", 2}, {"monthly", 3}, {"daily", 1}});
        EXPECT_THROW(writer.write({4.0}, {{"yearly", 2}, {"monthly", 2}, {"daily", 29}}), std::invalid_argument);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_DOUBLE_EQ(reader.read({{"yearly", 1}, {"monthly", 2}, {"daily", 29}})[0], 1.0);
    EXPECT_DOUBLE_EQ(reader.read({{"yearly", 2}, {"monthly", 2}, {"daily", 28}})[0], 2.0);
    EXPECT_DOUBLE_EQ(reader.read({{"yearly", 2}, {"monthly", 3}, {"daily", 1}})[0], 3.0);
}

TEST_F(BinaryTempFileFixture, NonMidnightStartUnderMonthlyDailyHourly) {
    // The first cell is 06:00 on January 1; the monthly step used to drop the time of day and reject it
    auto md = make_time_layout({"monthly", "daily", "hourly"}, {12, 31, 24}, "2025-01-01T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 1, 7}));
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
        writer.write({1.0}, {{"monthly", 1}, {"daily", 1}, {"hourly", 7}});
        writer.write({2.0}, {{"monthly", 2}, {"daily", 28}, {"hourly", 24}});
        EXPECT_THROW(writer.write({3.0}, {{"monthly", 2}, {"daily", 29}, {"hourly", 1}}), std::invalid_argument);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_DOUBLE_EQ(reader.read({{"monthly", 1}, {"daily", 1}, {"hourly", 7}})[0], 1.0);
    EXPECT_DOUBLE_EQ(reader.read({{"monthly", 2}, {"daily", 28}, {"hourly", 24}})[0], 2.0);
}
```

Fail before the fix (each `write` throws `std::invalid_argument`, uncaught by the helper):
- `EveryCellDailyUnderYearly`, at its first cell: day 74 is March 15, and the old check compares against day-of-month 15.
- `EveryCellHourlyUnderYearly` (1759) and `EveryCellHourlyUnderMonthly` (343), at their first cell: the yearly/monthly step dropped 06:00, so the old check sees hour 1.
- `EveryCellDailyUnderWeeklyAcrossYearEnd` and `EveryCellHourlyUnderWeeklyAcrossYearEnd`. They fail the `first_dimensions` assertion first (old initial values 4 and 79), then their first cell (day-of-month 15, not 4; hour 7, not 79).
- `MonthlyUnderYearlyFromTheThirtyFirst` at (1,2), `LeapDayStartUnderYearlyMonthlyDaily` at (2,3,1), and `NonMidnightStartUnderMonthlyDailyHourly` at (1,1,7).

`EveryCellMonthlyUnderYearly`, `EveryCellDailyUnderMonthly` and `EveryCellHourlyUnderDaily` already pass today; they pin the three pairs that worked.

### C++: `tests/test_csv_converter.cpp`

After `TEST_F(CSVConverterFixture, HourlyMetadataRowCount)` (currently ~L237-248) add:

```cpp
TEST_F(CSVConverterFixture, AggregatedDateIsTheStartOfEachMonth) {
    // A monthly file starting January 31 is labelled by calendar month; it used to read 2025-01-31, 2025-03-03,
    // 2025-03-31, 2025-05-01
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-31T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"month"})
                                               .set("dimension_sizes", {4})
                                               .set("time_dimensions", {"month"})
                                               .set("frequencies", {"monthly"})
                                               .set("labels", {"val"}));
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path, true);
    auto lines = csv_lines();
    ASSERT_EQ(lines.size(), 5u);
    EXPECT_EQ(lines[1], "2025-01-01,null");
    EXPECT_EQ(lines[2], "2025-02-01,null");
    EXPECT_EQ(lines[3], "2025-03-01,null");
    EXPECT_EQ(lines[4], "2025-04-01,null");
}

TEST_F(CSVConverterFixture, AggregatedDatetimeKeepsANonMidnightStart) {
    // Monthly x hourly from 2025-01-15T06:00: the first row is the start itself (it used to be rejected, since the
    // monthly step dropped the time of day), and rows advance one hour at a time to the end of January
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-15T06:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"month", "hour"})
                                               .set("dimension_sizes", {1, 744})
                                               .set("time_dimensions", {"month", "hour"})
                                               .set("frequencies", {"monthly", "hourly"})
                                               .set("labels", {"val"}));
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path, true);
    auto lines = csv_lines();
    ASSERT_EQ(lines.size(), 1u + 402u);  // header + hours 343..744 of January
    EXPECT_EQ(lines[1], "2025-01-15T06:00:00,null");
    EXPECT_EQ(lines[2], "2025-01-15T07:00:00,null");
    EXPECT_EQ(lines.back(), "2025-01-31T23:00:00,null");
    EXPECT_NO_THROW(CSVConverter::csv_to_bin(path));  // re-reads the labels bin_to_csv wrote
}
```

Both fail before the fix: the first on its labels, the second because `bin_to_csv` throws at the first cell. Unwritten cells are NaN and print as `null`, so neither test depends on the float formatting that plan 13 changes.

### C API: `tests/test_c_api_binary_file.cpp`

a) In `class BinaryCApiFixture`, after `make_simple_metadata()` (currently ~L31-49), add:

```cpp
    // monthly(2) x hourly(744) from 2025-03-15T06:00:00: month 1 is March, month 2 is April (720 hours)
    quiver_binary_metadata_t* make_monthly_hourly_metadata() {
        quiver_element_t* el = nullptr;
        quiver_element_create(&el);
        quiver_element_set_string(el, "version", "1");
        quiver_element_set_string(el, "initial_datetime", "2025-03-15T06:00:00");
        quiver_element_set_string(el, "unit", "MW");

        const char* dims[] = {"month", "hour"};
        quiver_element_set_array_string(el, "dimensions", dims, 2, nullptr);
        int64_t sizes[] = {2, 744};
        quiver_element_set_array_integer(el, "dimension_sizes", sizes, 2, nullptr);
        quiver_element_set_array_string(el, "time_dimensions", dims, 2, nullptr);
        const char* frequencies[] = {"monthly", "hourly"};
        quiver_element_set_array_string(el, "frequencies", frequencies, 2, nullptr);
        const char* labels[] = {"val"};
        quiver_element_set_array_string(el, "labels", labels, 1, nullptr);

        quiver_binary_metadata_t* md = nullptr;
        quiver_binary_metadata_from_element(el, &md);
        quiver_element_destroy(el);
        return md;
    }
```

b) After `TEST_F(BinaryCApiFixture, ReadUnwrittenPositionFails)` (currently ~L434-468), add:

```cpp
TEST_F(BinaryCApiFixture, HourlyUnderMonthlyFromNonMidnightStart) {
    auto* md = make_monthly_hourly_metadata();
    ASSERT_NE(md, nullptr);

    // 06:00 on March 15 is hour 14 * 24 + 7 = 343 of March
    quiver_dimension_t hour_dim = {};
    EXPECT_EQ(quiver_binary_metadata_get_dimension(md, 1, &hour_dim), QUIVER_OK);
    EXPECT_EQ(hour_dim.time_properties.initial_value, 343);
    quiver_binary_metadata_free_dimension(&hour_dim);

    const char* dim_names[] = {"month", "hour"};
    const int64_t cells[][2] = {{1, 343}, {1, 744}, {2, 1}, {2, 720}};  // start, end of March, April's first/last

    quiver_binary_file_t* writer = nullptr;
    ASSERT_EQ(quiver_binary_file_open_file(path.c_str(), 'w', md, &writer), QUIVER_OK);
    for (const auto& cell : cells) {
        double data[] = {static_cast<double>(cell[0] * 1000 + cell[1])};
        EXPECT_EQ(quiver_binary_file_write(writer, dim_names, cell, 2, data, 1), QUIVER_OK) << quiver_get_last_error();
    }
    int64_t past_april[] = {2, 721};
    double one[] = {1.0};
    EXPECT_EQ(quiver_binary_file_write(writer, dim_names, past_april, 2, one, 1), QUIVER_ERROR);
    EXPECT_NE(std::string(quiver_get_last_error()).find("dimension 'hour' has value 721"), std::string::npos);
    quiver_binary_file_close(writer);
    quiver_binary_metadata_free(md);

    quiver_binary_file_t* reader = nullptr;
    ASSERT_EQ(quiver_binary_file_open_file(path.c_str(), 'r', nullptr, &reader), QUIVER_OK);
    for (const auto& cell : cells) {
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_binary_file_read(reader, dim_names, cell, 2, 0, &out_data, &out_count), QUIVER_OK);
        ASSERT_EQ(out_count, 1u);
        EXPECT_DOUBLE_EQ(out_data[0], static_cast<double>(cell[0] * 1000 + cell[1]));
        quiver_binary_file_free_float_array(out_data);
    }
    quiver_binary_file_close(reader);
}
```

It fails before the fix: the first write returns `QUIVER_ERROR` (`'hour' has value 343 ... implies 1`). The file already includes `<string>`. This test frees every handle it creates, in line with plan 69.

### Lua: `tests/test_lua_binary.cpp`

After `TEST_F(LuaBinaryTest, TimeDimensionWriteRead)` (currently ~L144-160) add:

```cpp
TEST_F(LuaBinaryTest, WeeklyDailyCountsDaysFromInitialDatetimeAcrossYearEnd) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    // 60 weeks from Saturday 2025-03-15: a week is seven days from that day, so week 42 day 6 is 2026-01-01
    lua.run(R"(
        local md = quiver.metadata{ initial_datetime='2025-03-15T00:00:00', unit='MW',
            labels={'v'}, dimensions={'week','day'}, dimension_sizes={60,7},
            time_dimensions={'week','day'}, frequencies={'weekly','daily'} }
        assert(md:get_dimensions()[2].initial_value == 1, 'a day under a week starts at 1')
        local f = db:open_file('bin_w', 'w', md)
        for week=1,60 do for day=1,7 do f:write({week*10+day}, {week=week, day=day}) end end
        f:close()
        local r = db:open_file('bin_w', 'r')
        for week=1,60 do for day=1,7 do
          assert(r:read({week=week, day=day})[1] == week*10+day, 'cell '..week..','..day)
        end end
        r:close()
    )");
}
```

It fails before the fix: the `initial_value` is 4.

### Julia: `bindings/julia/test/test_binary_file.jl`

After `@testset "Invalid time dimension coordinates" begin ... end` (currently ~L653-664), add:

```julia
    @testset "Hourly under monthly from a non-midnight mid-month start" begin
        path = make_binary_file_path()
        try
            md = Quiver.Binary.Metadata(;
                initial_datetime = "2025-03-15T06:00:00",
                unit = "MW",
                labels = ["val"],
                dimensions = ["month", "hour"],
                dimension_sizes = Int64[2, 744],
                time_dimensions = ["month", "hour"],
                frequencies = ["monthly", "hourly"],
            )
            # 06:00 on March 15 is hour 14 * 24 + 7 = 343 of March; April has 720 hours
            @test Quiver.Binary.get_dimensions(md)[2].initial_value == 343
            cells = vcat([(1, h) for h in 343:744], [(2, h) for h in 1:720])
            @test length(cells) == 1122

            file = Quiver.Binary.open_file(path; mode = 'w', metadata = md)
            for (month, hour) in cells
                Quiver.Binary.write!(file; data = [month * 1000.0 + hour], month = month, hour = hour)
            end
            @test_throws Quiver.DatabaseException Quiver.Binary.write!(file; data = [1.0], month = 2, hour = 721)
            Quiver.Binary.close!(file)

            reader = Quiver.Binary.open_file(path; mode = 'r')
            @test all(Quiver.Binary.read(reader; month = m, hour = h) == [m * 1000.0 + h] for (m, h) in cells)
            Quiver.Binary.close!(reader)
        finally
            cleanup_binary_file(path)
        end
    end
```

It fails before the fix: `write!` at (1, 343) raises `DatabaseException`.

### Dart, Python, JS

These bindings do not expose the binary subsystem (root Design Decision), so there are no tests to add.

## Docs and changelog

### `src/AGENTS.md`

1. File map. Old: `  binary_utils.h              # Shared file-extension constants`. New: `  binary_utils.h              # Shared file-extension constants, day_of_year, position_in_parent`.
2. File map. Old: `  time_properties.cpp         # TimeFrequency string conversion`. New: `  time_properties.cpp         # TimeFrequency string conversion, add_offset_from_int`.
3. In `## Binary Subsystem`, directly after the bullet ``- `TimeFrequency` enum: `Yearly`, `Monthly`, `Weekly`, `Daily`, `Hourly` `` and before `### Iteration Helpers`, insert:

```markdown
### Time Coordinates

A coordinate names a **cell**, and its datetime is the cell's start. `TimeProperties::add_offset_from_int(base, value)`
floors `base` to the start of its own period (January 1, the 1st, the day for Weekly and Daily, the hour) and adds
`value - 1` periods. Folded over the time dimensions outermost-first from `initial_datetime` (what
`validate_dimension_values`, `dimension_sizes_at_values` and the CSV datetime column do), every inner step starts
from its parent's period start, so no calendar step overflows (January 31 + one month is not March 3) and
`initial_value` plays no part. `add_offset_from_int(datetime, 1)` is therefore the start of that frequency's period
holding `datetime`. Yearly and monthly periods are calendar-aligned; **a week is seven days counted from the day of
`initial_datetime`**, never from January 1, so a weekly file crosses year ends on one grid. `position_in_parent`
(`binary_utils.h`) is the inverse — a datetime's position inside a dimension's parent period — and the one rule
for it: `from_toml_content` sets each inner `initial_value` to the position of `initial_datetime` (the outermost gets
1), and `validate_dimension_values` rejects a coordinate whose cell start sits at a different position than the
value given (day 30 of February spills into March). `from_toml_content` runs `validate()` **before** computing
initial values, since a position exists only for the eight parent/child layouts it admits: Monthly under Yearly;
Daily under Yearly, Monthly or Weekly; Hourly under Yearly, Monthly, Weekly or Daily. The `EveryCell*` tests in
`tests/test_binary_file.cpp` walk every cell of each pair from a mid-period, non-midnight start.
```

4. Performance Bottlenecks item 2. Old: ``(date arithmetic via `add_offset_from_int`/`datetime_to_int`)``. New: ``(date arithmetic via `add_offset_from_int`/`position_in_parent`)``.

### Root `AGENTS.md`

In `## Design Decisions`, directly after the bullet `- **`BinaryMetadata::number_of_time_dimensions()` is derived** from `dimensions`, never stored.`, insert:

```markdown
- **A binary time coordinate names a calendar cell, and a week starts on the day of `initial_datetime`.** An inner
  time value is its position inside the parent's period (day of month/year/week, hour of day/month/year/week);
  `add_offset_from_int` steps from period starts, never from `initial_datetime`'s own day, so month ends and leap
  years cannot shift a cell, and a week is seven days counted from `initial_datetime`'s day, never from January 1.
  One function, `position_in_parent` (`src/binary/binary_utils.h`), yields both the initial values and the
  read/write check. Details in `src/AGENTS.md` ("Time Coordinates").
```

### Other docs

- `tests/AGENTS.md` lists the binary test files by name, and none is added or removed, so it needs no edit.
- `bindings/js/src/lua-api.ts`, `docs/*.md` and the READMEs say nothing about time-coordinate semantics (grep for `initial_value`, `weekly`, `initial_datetime`). Nothing there becomes false, so they need no edit.

### `CHANGELOG.md`, under `## [0.12.0] — unreleased`

Append to `### Changed`, after the `export_csv()` quoting bullet (its last line is `  *Adapt:* regenerate golden files and any byte-for-byte comparisons over exported CSVs.`), before `### Fixed`:

```markdown
- **BREAKING — a binary file's time coordinate names a calendar cell, and a week starts on the day of
  `initial_datetime`.** Each inner time value is its position inside the parent's period (day of
  month, year or week; hour of day, month, year or week), and the date `bin_to_csv` writes — and
  `csv_to_bin` checks — is the start of that cell. Two things change for callers:
  - When the finest time dimension is monthly or yearly and `initial_datetime` falls mid-period, rows
    are labelled from the period start: a monthly file from `2025-01-15` reads
    `2025-01-01, 2025-02-01, …` (it read `2025-01-15, 2025-02-15, …`; from `2025-01-31` it read
    `2025-01-31, 2025-03-03, 2025-03-31, 2025-05-01`). An hourly cell starts on the hour, so a start
    of `…T06:30:00` labels its first row `…T06:00:00`.
  - Under a weekly dimension, a week is seven days counted from the day of `initial_datetime`, not
    from January 1: a daily child's `initial_value` is always 1 (it was 4 for a file starting
    Saturday 2025-03-15) and an hourly child's is the hour of day + 1. A weekly file with a daily or
    hourly dimension could not be written past its first week before this release, so no stored data
    depends on the old numbering.

  C++ only: `TimeProperties::datetime_to_int` is removed, and `TimeProperties::add_offset_from_int`
  now returns the start of the `value`-th period counted from the one holding its base, ignoring
  `initial_value`.

  *Adapt:* re-run `bin_to_csv` on such files before editing and re-importing their CSVs. C++ code
  that called `datetime_to_int` has no replacement: the coordinate is the position, and
  `BinaryFile::read`/`write` validate it.
```

Append to `### Fixed`, after the last bullet (the one ending `tracks `Artifacts.toml` as a precompile dependency and refreshes the hash when it changes.`):

```markdown
- **Binary files accept every cell of every time layout their metadata accepts.** `read` and `write`
  (and so `bin_to_csv`, `csv_to_bin` and `Expression::save`, in Julia and Lua too) rejected valid
  cells for five of the eight parent/child layouts, with `Invalid values for time dimensions:
  dimension 'hour' has value 25 but the resulting datetime implies 1`: hourly under monthly, yearly
  or weekly past the first day, daily under yearly past January, and daily under weekly past the
  first week. A non-midnight `initial_datetime` under a monthly or yearly dimension rejected the
  file's own first cell, and a start on the 29th-31st broke monthly layouts (yearly + monthly from
  January 31 rejected February; yearly + monthly + daily from 2024-02-29 rejected 2025-03-01).
- **Binary metadata with an invalid frequency layout reports why.** `from_toml_content` and
  `from_element` (so Julia `Metadata` and Lua `quiver.metadata`) computed initial values before
  validating, so frequencies `["monthly", "yearly"]` failed with `YEARLY frequency not implemented.
  This function should only be used for inner time dimensions.` and `["daily", "daily"]` with
  `Invalid parent frequency daily for DAILY dimension.`. They now report `Time dimension frequencies
  must be ordered from lowest to highest frequency.` and `Time dimension frequencies must be unique.
  Duplicate: daily`.
```

## Verification

From the repo root (`C:\Development\Quiver\quiver1`), in order:

1. `cmake --build build --config Debug`. It must compile with no new warnings in `src/binary/`.
2. `./build/bin/quiver_tests.exe --gtest_filter="TimeFrequencyConversion.*:TimePropertiesSetters.*:TimePropertiesAddOffset.*:BinaryMetadata*:BinaryTempFileFixture.*:CSVConverterFixture.*:IterationTest.*:LuaBinaryTest.*:ExpressionFixture.*:LuaExpressionTest.*"`. All must pass, including these new tests:
   - `TimePropertiesAddOffset.IgnoresInitialValue`, `.MonthlyStartsFromTheFirstOfTheBaseMonth`, `.YearlyStartsFromJanuaryFirstOfTheBaseYear`, `.WeeklyStartsOnTheBaseDay`, `.DailyStartsAtMidnight`, `.HourlyStartsOnTheHour`, `.ValueOneIsThePeriodHoldingTheBase`
   - `BinaryMetadataFromTomlContent.InitialValuesUnderWeeklyCountFromInitialDatetime`, `.InvalidFrequencyLayoutReportsTheValidatorMessage`
   - `BinaryTempFileFixture.EveryCellMonthlyUnderYearly`, `.EveryCellDailyUnderYearly`, `.EveryCellHourlyUnderYearly`, `.EveryCellDailyUnderMonthly`, `.EveryCellHourlyUnderMonthly`, `.EveryCellDailyUnderWeeklyAcrossYearEnd`, `.EveryCellHourlyUnderWeeklyAcrossYearEnd`, `.EveryCellHourlyUnderDaily`, `.MonthlyUnderYearlyFromTheThirtyFirst`, `.LeapDayStartUnderYearlyMonthlyDaily`, `.NonMidnightStartUnderMonthlyDailyHourly`
   - `CSVConverterFixture.AggregatedDateIsTheStartOfEachMonth`, `.AggregatedDatetimeKeepsANonMidnightStart`
   - `LuaBinaryTest.WeeklyDailyCountsDaysFromInitialDatetimeAcrossYearEnd`
   - The existing `BinaryTempFileFixture.InvalidTimeDimensionCoordinates`, `.InitialDatetimeYear1960` and `.SingleTimeDimensionSkipsConsistencyCheck` still pass. No `TimePropertiesDatetimeToInt.*` test exists any more.
3. `./build/bin/quiver_c_tests.exe --gtest_filter="BinaryCApi*:ExpressionCApiFixture.*"`. All must pass, including `BinaryCApiFixture.HourlyUnderMonthlyFromNonMidnightStart`.
4. `bindings/julia/test/test.bat`. It must pass, including the testset "Hourly under monthly from a non-midnight mid-month start". The Julia generator is **not** run, because no C signature changed.
5. `git grep -n -e "datetime_to_int" -e "day_of_week" -e "compute_time_dimension_initial_values" -- src include tests bindings` must print nothing.
6. `scripts/format.bat`, then `git diff --stat` must show only the files this plan names.
7. `scripts/test-all.bat` (all six suites + the CLI smoke test) must be green. Dart, Python and JS are unaffected but run there anyway.

## Acceptance criteria

- [x] `TimeProperties::datetime_to_int` and `quiver::day_of_week` are gone from headers, sources and tests.
- [x] `add_offset_from_int` implements the period-start contract documented in `time_properties.h` and reads no `initial_value`.
- [x] `position_in_parent` in `src/binary/binary_utils.h` is the only position calculation; it is called by `set_time_dimension_initial_values` and `BinaryFile::validate_dimension_values`, and nowhere else.
- [x] `from_toml_content` calls `metadata.validate()` before `set_time_dimension_initial_values(metadata)`; the four unreachable `std::logic_error` branches are deleted. *Four messages, five throw sites; all five are gone with the function (Implementation notes, deviation 6).*
- [x] Under a weekly parent the day's `initial_value` is 1 and the hour's is hour-of-day + 1.
- [x] Every cell of all eight parent/child pairs, walked with `first_dimensions`/`next_dimensions` from 2025-03-15T06:00:00, writes and reads back (weekly layouts span 60 weeks across Dec 31).
- [x] The validator's `Invalid values for time dimensions: ...` message text is unchanged.
- [x] New tests exist in C++ (time properties, metadata, binary file, CSV), the C API, Lua and Julia, and each is listed in Verification.
- [x] `src/AGENTS.md` (file map, "Time Coordinates", performance note) and a root AGENTS.md Design Decision bullet are updated.
- [x] CHANGELOG `[0.12.0]` has the BREAKING `Changed` entry and the two `Fixed` entries. *Under a new `[0.12.4]` instead (Implementation notes, deviation 1).*
- [x] No C API, FFI declaration or binding wrapper changed; `c_api.jl` is untouched.
- [x] `scripts/test-all.bat` is green.

## Pitfalls

- **Do not add a 3- or 4-level full walk with a mid-period start.** `next_dimensions` restores a child's initial value when its *immediate* parent is at its initial value, not when every ancestor is (plan 10). Such a walk visits invented cells, for example Feb 29 2025 from a 2024-02-29 start, or skips real ones, and the walk fails for a reason this plan does not own. The three-level tests here write named cells on purpose.
- **Plan 09 is written for the old contract.** See Overlaps: its loop `dim.time->add_offset_from_int(output_meta_.initial_datetime, 1)` over the remaining dimensions no longer rebases. Pass this on; do not "fix" it here.
- **`EXPECT_THROW` / `EXPECT_EQ` with braced maps:** keep the braces inside the call parentheses (`writer.write({1.0}, {{"a", 1}, {"b", 2}})`), as the existing tests do. Wrap vector literals in parentheses: `EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 3}))`.
- **`std::chrono` duration braces with `int64_t`:** `std::chrono::years{steps}` and `months{steps}` go through the duration constructor template, so there is no narrowing error. The old code already did this with `int64_t relative_value`.
- **The switch in `add_offset_from_int` has no trailing `return`**, exactly like the old function. Keep all five `case`s; adding a `default:` would hide a future enumerator from `-Wswitch`. MSVC's C4715 behaves the same as before.
- **`binary_utils.h` now includes `binary_metadata.h`.** Only `src/binary/*.cpp` include it, so there is no cycle. Do not include it from a public header.
- **Test runtime:** the yearly × hourly walk writes and reads 15,762 cells, and weekly × hourly 10,074. That is well under a second in Debug. Do not shrink the sizes: the weekly walks must cross 2026-01-01 (week 42).
- **Julia:** `@test all(... for (m, h) in cells)` is one assertion. Do not put `@test` inside the loop, which would add over a thousand passes to the report.
- **Formatting:** run `scripts/format.bat` (clang-format, JuliaFormatter). Do not edit any `.bat` file, since working-tree `.bat` files are CRLF.
- **Line numbers are approximate:** plans 01-07 do not touch `src/binary/`, but anchor on the quoted code anyway.

## Out of scope

- Promoting the initial-value calculation to a `BinaryMetadata` member, and the stale initial values and rebase after `ExpressionAggregate`: **plan 09**.
- The immediate-parent restore rule in `next_dimensions` and `ExpressionAggregate::compute_row`: **plan 10**.
- Length checks, missing-key errors and the TOML round trip in `from_element`/`from_toml_content`: **plan 11**.
- Deleting the C API metadata builders and `add_dimension`/`add_time_dimension`, which store `initial_value` 0: **plan 12**.
- `csv_to_bin` float parsing, `bin_to_csv` float formatting (**plan 13**) and the CSV field-count check (**plan 14**).
- The unreachable "Weekly under Yearly" case in `validate_time_dimension_sizes`. It is harmless and no plan owns it.
- Making `validate_dimension_values` opt-in for speed (the src/AGENTS.md performance note).
- Sub-hour cells: an hourly cell is a clock hour by design, so minutes in `initial_datetime` are floored, not preserved.

## Implementation notes

Implemented on `rs/plan8`. Before any edit, `origin/master` was merged in, which fast-forwarded to `41748e8`: plan 07's merge `ed824f5`, tagged `v0.12.3`, then the 0.12.4 manifest bump. Nothing in that merge touches the binary subsystem.

A read-only verification workflow ran in plan mode first, before any edit. Four reviewers took one angle each (a Python model of the old and new code, a breakage sweep, anchors/APIs, a design critique), and a refuter attacked every finding.
- Every quoted excerpt, symbol, test name and API in this plan still matched the code. No file under `src/binary` or `include/quiver/binary` has changed since `58dfe7a`.
- Every number in Tests was reproduced by the model: first cells, cell counts, rejected cells, CSV labels, 343, 1122, and the weekly initial values 1 and 7 (4 and 79 before).
- 456k two-level coordinates from 15 starts showed zero acceptance mismatches under the new contract.
- No other existing test changes. The deleted functions have no caller outside `src/binary`.
- Verdict: implement as written, with the text corrections below.

No C API, FFI or binding code changed, so the generators were not run and `c_api.jl` is untouched.

**Red/green.** Every new test was added first and run against the unfixed code.
- C++ (the Verification step 2 filter, plus `TimePropertiesDatetimeToInt.*`, which still existed then): 19 failed and 346 passed. The 19 are exactly the "Fail before the fix" list, each with the predicted message:
  - `'daily' has value 74 ... implies 15`
  - `'hourly' has value 1759 ... implies 1`
  - `'hourly' has value 343 ... implies 1`
  - weekly `first_dimensions` `{1, 4}` / `{1, 79}`
  - `'monthly' has value 2 ... implies 3`
  - `'daily' has value 1 ... implies 4`
  - `'hourly' has value 7 ... implies 1`
  - CSV labels `2025-01-31, 2025-03-03, 2025-03-31, 2025-05-01`
  - the metadata initial values 4 and 79, and an uncaught `YEARLY frequency not implemented...`
  - the six `add_offset_from_int` results `2025-01-10`, `03-03`, `03-01`, `03-22 06:00`, `03-15 06:00`, `03-15 07:30`
  - Lua `a day under a week starts at 1`

  `EveryCellMonthlyUnderYearly`, `EveryCellDailyUnderMonthly`, `EveryCellHourlyUnderDaily` and `ValueOneIsThePeriodHoldingTheBase` passed, as predicted.
- C API: `HourlyUnderMonthlyFromNonMidnightStart` failed alone (write returned `QUIVER_ERROR`, `'hour' has value 343 ... implies 1`). The other 139 tests passed.
- Julia: the new testset errored at `write!` (1, 343) with `... implies 1`. The runner is fail-fast.

After the fix:
- The Verification step 2 filter: 354/354 (365 − the 11 deleted tests). Every test named in step 2 is `OK`, and no `TimePropertiesDatetimeToInt.*` test remains.
- Step 3 filter: 140/140.
- Step 5 `git grep`: prints nothing. `position_in_parent` is called only from `set_time_dimension_initial_values` and `validate_dimension_values`.
- `scripts/test-all.bat` (six steps): `All tests PASSED`. C++ 1328 (plan 07's 1318 + 21 − 11), C API 567, Julia 1465 (1461 + 4), Dart 426, JS 212, Python 309.
- One earlier full Julia run spent 38 min in `Expression`. It did not reproduce: that testset alone took 47 s, and in `test-all` it took 1m06s (3m17s for the whole Julia suite). The cause was environmental.
- Build: no new warnings. The HEAD version of `time_properties.cpp` compiled with the same flags gives C4458 plus three C4715s. The new file gives the C4458 plus two C4715s (`frequency_to_string`, `add_offset_from_int`); `datetime_to_int`'s went with it.
- The Reproduction table was re-run through `quiver_cli.exe` with a scratch Lua script. All eight cells now write and read back, and both invalid layouts report the validator messages. A monthly-only file from 2025-01-31 exports `2025-01-01 … 2025-04-01`.

**Deviations:**
1. **The CHANGELOG entries are in a new `## [0.12.4] — unreleased` section**, above `[0.12.3]`, with its own `### Changed` and `### Fixed`. The plan said `[0.12.0]`, but `v0.12.0` through `v0.12.3` are tagged and the manifests are at 0.12.4. The anchor bullets it names (the `export_csv()` quoting bullet and the `Artifacts.toml` one) now sit in the released `[0.11.0]`. No manifest bump.
2. **The CHANGELOG weekly sentence is rewritten.** "…could not be written past its first week before this release, so no stored data depends on the old numbering" is false. `BinaryFile::write` with named coordinates (Julia `write!`, Lua `file:write`, the C API) accepted two kinds of cell under the old check:
   - hours 1–24 of every week of a weekly × hourly file;
   - the weeks of a weekly × daily file whose start on the old January-1 grid fell on the 1st of a month.

   Those stored cells now name a moment `(day of year − 1) mod 7` days later. From 2025-03-15, old (1,1) is Mar 12 and new (1,1) is Mar 15. The entry now says so and tells the caller to rewrite such files. The Fixed bullet's "daily under weekly past the first week" was fixed the same way.

   A first correction, made while planning, still read "only a weekly × daily file started on the 2nd–7th of January…". The post-implementation review showed that it held only for the walk-based writers (`csv_to_bin`, `save`).
3. **The CHANGELOG Fixed bullet is narrowed** in two places:
   - A non-midnight start rejected the first cell only with an **hourly** dimension under a monthly or yearly one. Monthly × daily from 06:00 worked.
   - The 29th–31st broke **yearly + monthly** layouts. Monthly × daily from Jan 31 worked.
4. **Doc text corrections.**
   - `src/AGENTS.md` "Time Coordinates": "`add_offset_from_int(datetime, 1)` is the start of that frequency's period holding `datetime`" is wrong for Weekly. A qualifier was added: the week starts on `datetime`'s day, which is on the file's grid only when that day is a whole number of weeks after `initial_datetime`'s day.
   - The root Design Decision bullet said steps are "never from `initial_datetime`'s own day", which contradicts outermost Weekly and Daily. It now says the yearly and monthly steps start from January 1 / the 1st, never from `initial_datetime`'s day of the month.
   - The `BinaryTimeLayouts` banner no longer says 06:00:00 is "not the start of any period" (it starts an hourly one). It now reads "not the start of any day, week, month or year".
5. **`HourlyUnderMonthlyFromNonMidnightStart` (C API) uses `EXPECT_EQ(out_count, 1u)` plus a guard instead of `ASSERT_EQ`.** The red run showed the problem: the `ASSERT` returned early and left the reader open, so later `BinaryCApiFixture` tests (`WriterBlocksReader`, `ClosedWriterAllowsReader`, …) failed in `TearDown` (`remove: The process cannot access the file`). The plan's "frees every handle, in line with plan 69" claim held only on the passing path.
6. **Small drift.**
   - There are five `std::logic_error` throw sites, not four: "Invalid parent frequency" is thrown for both DAILY and HOURLY. All five went with `compute_time_dimension_initial_values`.
   - Verification step 7's "+ the CLI smoke test" is stale: `test-all.bat` has had six steps since `01e78d7` (plan 65).
   - Line numbers drifted by a line or two; edits were anchored on names.

**Post-implementation review.** A workflow looked at the diff through three lenses: plan conformance and acceptance criteria, C++ correctness, and prose accuracy. A refuter then attacked each finding.
- Conformance and correctness found nothing. The correctness lens also covered the weekly modulo sign, types, includes, ODR, callers in `iteration.cpp`, `csv_converter.cpp`, the expression code, Lua and the C API, and test hygiene.
- Prose raised five findings. Four survived the refuters and were fixed (deviations 2 and 4).
- The fifth was refuted as a CHANGELOG defect. It is the 08 → 09 transient below.

**For later plans:**
- **09:**
  - Use this plan's contract. The rebase is one call on the **reduced** dimension: `operand_meta.dimensions[reduced].time->add_offset_from_int(operand_meta.initial_datetime, 1)`, then derive the initial values. 09's loop over the remaining dimensions no longer rebases anything.
  - **Known transient until 09 lands:** `ExpressionAggregate` copies the operand's stale initial values (`output_meta_ = operand_meta;`), and `add_offset_from_int` no longer subtracts `initial_value`.
    - Aggregating away the year of a yearly × monthly × daily file with a mid-year start now throws in `save()` from many starts. From 2025-03-15 it reads operand June 31 (`Invalid values for time dimensions: dimension 'month' has value 6 ... implies 7`) after 108 rows.
    - From some other starts it silently drops days.
    - Before this plan the same save completed but wrote a file shifted by two months (09's bug).
    - Suggested test for 09: yearly(2) × monthly(12) × daily(31) from 2025-03-15, `aggregate("year", Sum)`, `save` must not throw, the output `initial_datetime` is 2025-01-01, and 365 cells are walked.
    - If 0.12.4 is tagged before 09 lands, add a known-gap sentence to its CHANGELOG.
- **10:** the immediate-parent restore rule still limits walks of three or more levels with a mid-period start. The `EveryCell*` walks here are two-level, and the three-level tests write named cells.
- **11:** keep `metadata.validate();` then `set_time_dimension_initial_values(metadata);` at the tail of `from_toml_content`.
- **12:** `add_dimension` / `add_time_dimension` are untouched and still store `initial_value` 0.
- **13/14:** the new `test_csv_converter.cpp` tests assert only `null` cells and dates, so they do not depend on float formatting.
- **Pre-existing, not owned by any plan:** `dimension_sizes_at_values` replaces an inner daily or hourly size with the real length of the parent period. It never compares that with `dim.size`, while `validate_dimension_values` rejects any value above `dim.size`. So a layout `validate()` accepts with a minimum inner size (daily under monthly of size 28) cannot be walked past day 28 of a 31-day month.
- **`scripts/format.bat`:** biome rewrote 24 untouched CRLF JS files to LF again, with no content change under `git diff --ignore-cr-at-eol`. They were checked out again.
