# 61 — Remove the unused includes left by the rapidcsv drop

**Batch** 6 · **Severity** low · **Breaking** no · **Size** S · **Layers** C++ core only
**Depends on** none. Run it **last** among the CSV plans (01, 58, 60), because those add code to the same files and may start using one of these headers. · **Overlaps with** 01 (adds a second use of `<set>` in `database_csv_import.cpp`), 58 (rewrites `database_csv_import.cpp` and `database_csv_export.cpp`), 13 (moves `parse_float` out of `database_csv_import.cpp` into `utils/number.h`, taking its `<cerrno>`/`<clocale>`/`<cmath>`/`<cstdlib>` uses with it)

## Why

Commit 713f501 ("refactor!: drop rapidcsv") left standard headers that nothing uses any more.

`src/database_csv_export.cpp` includes (currently L8-15):
```cpp
#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
```
At HEAD, a grep of the file for the symbols each header provides finds **none** for `<algorithm>`
(`std::sort|find|transform|all_of|any_of|remove|count`), `<cctype>`
(`isdigit|isalpha|isspace|toupper|tolower`), `<cstring>` (`strlen|memcpy|strcmp|strerror`),
`<ctime>` (`std::tm|mktime|strftime|localtime|gmtime|time_t`), `<iomanip>`
(`setw|put_time|get_time|setprecision`) or `<set>` (`std::set<`).

`src/database_csv_import.cpp` includes `<cstdio>` (L13). The file uses no
`printf|fopen|FILE*|snprintf|std::remove(`.

Principle: delete unused code. Stale includes mislead readers about what a file depends on.

## Constraints and decisions

- **Re-verify at the time you run this plan**, because 01, 13 and 58 change these files. For each
  candidate header, grep the file for its symbols (the regexes above). Remove the header only when
  the grep is empty **and** the file still compiles on MSVC. MSVC is the local compiler, and Linux
  or macOS CI catches a libstdc++/libc++ transitive-include difference.
- Remove nothing that `<filesystem>` / `<fstream>` / `<optional>` users still need.
- For `database_csv_import.cpp`, only `<cstdio>` is in scope. Its `<cerrno>`, `<clocale>`,
  `<cmath>` and `<cstdlib>` serve `parse_float` / `parse_integer`. If plan 13 moved `parse_float`
  into `src/utils/number.h`, re-check those four with
  `grep -n "errno\|setlocale\|localeconv\|std::isfinite\|strtod\|strtoll" src/database_csv_import.cpp`,
  and remove any that are now unused.

## Changes

1. `src/database_csv_export.cpp`: delete the `#include` lines for `<algorithm>`, `<cctype>`,
   `<cstring>`, `<ctime>`, `<iomanip>` and `<set>` that the re-verification shows unused.
2. `src/database_csv_import.cpp`: delete `#include <cstdio>`, plus any of the four
   `parse_float` headers that step "Constraints" shows unused.

## Tests

None. The build is the test.

## Docs and changelog

None.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=*Csv*:*CSV*`
3. Rely on the CI Linux/macOS builds for the other standard libraries. If you have WSL, run
   `cmake -S . -B build-linux -G Ninja && cmake --build build-linux` there too.
4. `scripts/format.bat` (clang-format regroups includes)

## Acceptance criteria

- [x] Every remaining `#include` in both files is used, as shown by the symbol grep.
- [x] Debug build and CSV tests green.

## Pitfalls

- `<cctype>` can look unused while `std::isdigit` is called unqualified as `isdigit`. The grep above
  covers both spellings.
- `<ctime>` may be needed by `utils/datetime.h` users. That header includes what it needs itself,
  so it does not justify keeping it here.

## Out of scope

- Include cleanup in any other file.

## Implementation notes

- **Removed.** From `src/database_csv_export.cpp`: `<algorithm>`, `<cctype>`, `<cstring>`,
  `<ctime>`, `<iomanip>` and `<set>`. Only `<filesystem>` (`fs::create_directories`) and `<fstream>`
  (`std::ofstream`) are left. From `src/database_csv_import.cpp`: `<cstdio>`. Its other standard
  includes are all used: `<algorithm>` (`std::transform`/`find`/`max`), `<cctype>` (`tolower`),
  `<ctime>` (`std::tm`/`strftime`), `<filesystem>`, `<fstream>`, `<iomanip>` (`get_time`),
  `<optional>`, `<set>`, `<sstream>` and `<string_view>`. Every project include in both files is
  used.
- **The greps.** The regexes above are written with `std::` prefixes. Without the prefix, `find`
  matches the member `.find()` calls on `std::unordered_map` in the export file, which are not
  `<algorithm>`.
- **Drift fixed.**
  - In export, the include block is at L9-16, not L8-15.
  - In import, `<cstdio>` was at L12, not L13.
  - Plan 13 (`ea886ad`) had already removed import's `<cerrno>`, `<clocale>`, `<cmath>` and
    `<cstdlib>`, so the conditional step found nothing left to do.
- **Order.** Plans 01, 13 and 58 had landed when this started, and plan 60 (`85e92d7`) landed on
  master during the work. It edits import_csv's write tail, not the include block, and uses none of
  the removed headers (greps re-run on HEAD). The build and the test runs below include it.
- **Verification.**
  - Debug build is green on MSVC.
  - `quiver_tests --gtest_filter=*Csv*:*CSV*`: 303 passed. `quiver_c_tests` with the same filter:
    58 passed.
  - No local Linux build: WSL has only the `docker-desktop` distro, so the Linux/macOS CI jobs
    cover libstdc++/libc++.
  - `scripts/format.bat` passed its C++, Julia and Dart steps and changed no file. Its Python step
    failed in the environment, before ruff ran: uv could not build its `quiverdb` venv ("Failed to
    update Windows PE resources ... uv-trampoline.exe"). The script stops there, so the JS step
    never ran. This change touches no Python or JS.
