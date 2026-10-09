---
last_mapped_commit: cf9ca58aceab305fe6a08461f78da8556a6c95d0
last_mapped_at: 2026-10-07
---
# Codebase Concerns

**Analysis Date:** 2026-10-07

This audit separates confirmed source/configuration discrepancies, accepted design limits, and unmeasured risks. It is a static mapping; builds, tests, benchmarks, and vulnerability scans are not run. Root policy is in `AGENTS.md`; subsystem constraints are in the adjacent `AGENTS.md` files.

## Tech Debt

**Version metadata and lockfile drift — confirmed:**

- Issue: `CMakeLists.txt`, `bindings/julia/Project.toml`, `bindings/dart/pubspec.yaml`, and `bindings/python/pyproject.toml` declare 0.13.1; `bindings/js/package.json` declares 0.13.2. The editable quiverdb package entry in `bindings/python/uv.lock` declares 0.13.0.
- Files: `scripts/assert_version.py`, `CMakeLists.txt`, `bindings/js/package.json`, `bindings/python/pyproject.toml`, `bindings/python/uv.lock`, `bindings/dart/pubspec.yaml`, `bindings/julia/Project.toml`.
- Impact: The existing version checker rejects the five-manifest disagreement, blocking release workflows that consume it. The Python lock's project metadata is stale; its exact effect on lock-aware tooling is not verified.
- Fix approach: Align the five manifests with the intended release using the existing canonical CMake version policy, then regenerate the Python lock. Do not create a second version authority; `scripts/assert_version.py` owns the five manifest checks and does not inspect `uv.lock`.

**Formatter documentation disagrees with implementation — confirmed:**

- Issue: Root tooling documentation names JuliaFormatter, while the Julia formatter imports Style and pins Style 0.1.0.
- Files: `AGENTS.md`, `bindings/julia/format/Project.toml`, `bindings/julia/format/format.jl`.
- Impact: Agents following the documentation can choose the wrong formatter or diagnose the wrong dependency.
- Fix approach: Describe the actual Style-based command in the applicable documentation; no formatter replacement is justified by this discrepancy.

**Dart generator has two configuration locations — documented maintenance trap:**

- Issue: The live configuration is the `ffigen:` block in `pubspec.yaml`; the sibling configuration is inactive without an explicit `--config`.
- Files: `bindings/dart/pubspec.yaml`, `bindings/dart/ffigen.yaml`, `bindings/dart/generator/generator.bat`, `bindings/dart/AGENTS.md`.
- Impact: Editing only the inactive file produces no generated ABI change. Generated enum representation and duplicate opaque-type names are also configuration-sensitive.
- Fix approach: Change the live block and regenerate `bindings/dart/lib/src/ffi/bindings.dart`; follow the documented canonical type aliases. Consolidate the duplicate only if requested.

## Known Bugs

**Aggregate local test command can report success with native suites missing — confirmed control-flow defect:**

- Symptoms: C++ and C API results remain SKIP, yet the final message says All tests PASSED and exits zero if all invoked binding commands succeed.
- Files: `scripts/test-all.bat`.
- Trigger: Either `build/bin/quiver_tests.exe` or `build/bin/quiver_c_tests.exe` is absent and the binding test calls return success.
- Workaround: Build native tests first and inspect the summary for SKIP. A script fix should treat missing required suites as failure or state incomplete execution explicitly.
- Scope: This is a local aggregator issue, not proof that CI skips native tests; `.github/workflows/ci.yml` separately builds and runs native tests.

**Lua native comparison metamethods do not represent element-wise expression comparisons — documented unresolved behavior:**

- Symptoms: Root decisions record always-true equality between expressions and equality/less-than between binary files through sol2's automatic conversion of expression-returning C++ operators.
- Files: `AGENTS.md`, `src/sandbox/expression.cpp`, `include/quiver/expression/abstract_expression.h`, `tests/test_sandbox_expression.cpp`.
- Trigger: Use Lua native `==` or `<` on those userdata instead of the explicit expression comparison functions.
- Workaround: Use `quiver.eq`, `quiver.lt`, and the other explicit element-wise comparison functions.
- Classification: Documented open design issue; this audit does not independently reproduce it or propose changing the settled AbstractExpression hierarchy.

## Security Considerations

**Sandbox borrows its database — explicit lifetime contract with an unguarded misuse hazard:**

- Risk: Running database methods after closing/destroying the borrowed Database can access freed native memory. A non-null Sandbox pointer does not prove its database is alive.
- Files: `src/sandbox/sandbox.cpp`, `src/c/sandbox.cpp`, `bindings/python/src/quiverdb/sandbox.py`, `bindings/js/src/sandbox.ts`, `AGENTS.md`.
- Current mitigation: Core ownership is explicit as `Database&`. Python keeps a Python reference to prevent GC, but explicit `Database.close()` still releases the native object. Sandbox open checks validate the Sandbox itself.
- Recommendations: Keep Sandbox inside Database lifetime scopes and close it first. If safe detection of misuse becomes required, solve it in the shared native ownership boundary rather than creating unrelated checks in every binding.
- Classification: Caller-contract hazard, not an in-contract use-after-free demonstrated by this audit.

**Lua execution has filesystem/library restrictions but no execution budget — source-observed resource risk:**

- Risk: An infinite loop or very large Lua allocation can monopolize CPU/memory in the hosting process. JSON output limits do not bound script execution before encoding.
- Files: `src/sandbox/sandbox.cpp`, `src/sandbox/return_json.cpp`.
- Current mitigation: Only base/string/table/math/coroutine/utf8 libraries are enabled; io/os/package/debug and disk source loading are unavailable. Script/load inputs are restricted to text. Returned JSON has depth 32 and size 64 MiB caps.
- Recommendations: Hosts requiring hostile-script resource isolation should impose process-level time/memory limits. Add native execution controls only when such a host requirement exists.
- Classification: No exploit or measured capacity threshold is established; the Sandbox is not a process isolation boundary.

**Filesystem containment is a path-resolution boundary — mitigated ordinary escapes, unverified race risk:**

- Risk: Canonicalize-then-open is not an atomic filesystem operation. A concurrently changed directory/symlink can invalidate the checked path before a later open.
- Files: `src/sandbox/path_policy.cpp`, `src/sandbox/csv.cpp`, `src/sandbox/xlsx.cpp`, `tests/test_sandbox_path.cpp`.
- Current mitigation: Strict containment after `weakly_canonical` rejects traversal, outside absolute paths, outside symlinks, and the root itself; in-memory databases reject file operations. Existing tests cover these cases.
- Recommendations: Preserve the central gate. Hosts needing protection against another actor changing the directory concurrently require OS-level filesystem/process isolation or handle-relative opening.
- Classification: Architectural race inference, not a demonstrated exploit; no extra approval workflow is warranted for normal trusted-directory use.

**Dry run is a transaction convenience, not a security guarantee — accepted limit:**

- Risk: Raw transaction-control SQL through query methods can commit changes before `end_dry_run`.
- Files: `src/database.cpp`, `src/database_query.cpp`, `tests/test_database_transaction.cpp`, `AGENTS.md`.
- Current mitigation: Public begin/commit/rollback calls are absorbed during dry run; end checks SQLite autocommit before rollback and preserves the flag if rollback fails.
- Recommendations: Use documented transaction APIs inside dry runs. Do not present dry runs as containment for arbitrary hostile SQL or filesystem writes.
- Classification: Explicit settled limitation; do not add SQL interception or change transaction semantics without a design request.

## Performance Bottlenecks

**Collection CSV export materializes query results and the full encoded file — confirmed allocation pattern, unmeasured cost:**

- Problem: Export can hold both the complete Result and an additional complete CSV string before writing.
- Files: `src/database_csv_export.cpp`, `src/database.cpp`, `src/csv/csv_write.cpp`.
- Cause: `execute` returns a materialized Result; `write_csv` builds `std::string out` by appending every record.
- Improvement path: If measured export sizes demand it, stream rows into the existing CSV emitter rather than introducing another quoting implementation.
- Classification: Deliberate materialization, with no measured latency or memory ceiling.

**XLSX callback reads still hold worksheet XML and shared strings in memory — accepted limit:**

- Problem: Streaming avoids a full Lua rows table but does not provide bounded workbook-parser memory.
- Files: `src/xlsx/xlsx_read.cpp`, `src/sandbox/xlsx.cpp`, `tests/test_sandbox_read_xlsx.cpp`, `AGENTS.md`.
- Cause: OpenXLSX loads a DOM; the reader traverses that DOM, including a validation/width pass.
- Improvement path: The existing ponytail comment names an XML streaming backend if workbook size makes this costly. Keep current cached-formula and malformed-XML checks when changing backend.
- Classification: Explicit accepted tradeoff; no workbook-size threshold is measured.

**Group writes prepare and execute one INSERT per row — source-observed, unmeasured:**

- Problem: Large groups repeat SQL construction/preparation and result handling for every row.
- Files: `src/database_impl.h` (`insert_rows_into_group_table`), `src/database.cpp` (`Impl::execute`).
- Cause: The row loop constructs INSERT SQL and invokes the general statement runner separately.
- Improvement path: Benchmark realistic group sizes before changing it; if necessary reuse one prepared statement inside the existing transaction and preserve prevalidation and error messages.

## Fragile Areas

**Writes inside caller-owned transactions — settled atomicity boundary:**

- Files: `src/database_impl.h`, `src/database_create.cpp`, `src/database_update.cpp`, `src/database_time_series.cpp`, `tests/test_sandbox_transaction.cpp`.
- Why fragile: TransactionGuard is a no-op when a transaction exists. Input validation completes before writes, but SQLite-only UNIQUE/NOT NULL/CHECK/FK or trigger failures can occur after earlier writes in the call.
- Safe modification: Prevalidate everything the core can validate before the first write. Callers catching a SQLite write failure must decide whether to roll back the owning transaction. Do not add SAVEPOINTs; their rejection is explicit in `AGENTS.md`.
- Test coverage: `TransactionBlockCaughtRejectedUpdateWritesNothing` covers prevalidation; it is not a promise of per-call rollback on every SQLite constraint failure.

**NULL shape and alignment across Lua and FFI — accepted representational limits:**

- Files: `src/sandbox/internal.h`, `src/sandbox/database_create.cpp`, `src/sandbox/database_read.cpp`, `src/sandbox/return_json.cpp`, `src/database_read.cpp`, `AGENTS.md`.
- Why fragile: Lua nil holes have no reliable list length; trailing NULL group cells cannot convey exact row counts without an authoritative dimension/non-null column. Sparse return tables encode as JSON objects. Element arrays require dense Lua tables, while group/time-series writers have their documented sparse rules.
- Safe modification: Preserve positional masks for numeric C/FFI readers, nullptr strings, and LEFT JOIN presence columns. Use host whole-group readers when exact nullable row shape is needed; keep a read transaction around multiple aligned reads under concurrent writes.
- Test coverage: `tests/test_sandbox_read.cpp`, `tests/test_database_read_vector.cpp`, `tests/test_database_read_set.cpp`, and `tests/test_sandbox_return.cpp` pin nullability/alignment behavior. The missing Lua whole-group readers are deliberate, not a feature omission to fill automatically.

**CSV import quote prepass depends on parser tokenization — documented dependency coupling:**

- Files: `src/database_csv_import.cpp`, `src/csv/csv_read.cpp`, `tests/test_sandbox_read_csv.cpp`, `cmake/Dependencies.cmake`.
- Why fragile: The destructive-import guard independently checks quote syntax while actual tokenization is delegated to csv-parser.
- Safe modification: Recheck both semantics on a csv-parser version change and preserve the shared Reader configuration's header-before-variable-columns order.
- Test coverage: `StrayQuotesTokenizeAsTheImportPrePassAssumes` pins the relevant upstream assumption. Import refuses nested transactions and owns rollback; preserve that contract.

**Run-exit handle cleanup suppresses close failures — accepted reporting limit:**

- Files: `src/sandbox/sandbox.cpp`, `src/sandbox/csv.cpp`, `tests/test_sandbox_write_csv.cpp`, `tests/test_sandbox_binary.cpp`.
- Why fragile: Scope-exit cleanup closes open CSV/binary handles but swallows close exceptions, including flush failures, because it also runs during unwinding.
- Safe modification: Scripts that need to observe completion failures should call explicit close. Preserve automatic cleanup of global/reachable handles.
- Test coverage: Existing suites verify automatic flushing and registry release; they do not establish successful flush under storage exhaustion.

## Scaling Limits

**Binary/expression concurrency — explicit non-thread-safe boundary:**

- Current capacity: No throughput number measured. Binary writes use one process-global unsynchronized path registry.
- Files: `src/binary/binary_file.cpp`, `src/expression/expression_file.cpp`, `src/AGENTS.md`.
- Limit: The registry is neither thread-safe nor multi-process protection; expression leaf computation reuses mutable state. SQLite serialized mode in `cmake/Dependencies.cmake` does not make these subsystems safe to share concurrently.
- Scaling path: Keep access serialized under current contracts. Add synchronization/process coordination only for an explicit concurrent-use requirement.

**JavaScript int64 output precision — confirmed representation ceiling:**

- Current capacity: Integer results are JS numbers; all integers are exact only through the safe-integer range, ±(2^53 − 1).
- Files: `bindings/js/src/ffi-helpers.ts`, `bindings/js/src/read.ts`, `bindings/js/src/query.ts`, `bindings/js/src/create.ts`.
- Limit: `Number(bigint)` conversion can round valid SQLite int64 values and ids outside that range.
- Scaling path: Keep application ids/data within the supported numeric range. A bigint result surface or explicit range gate is a contract/design change; assess every integer read and id-producing path together.

**Returned JSON has explicit finite limits:**

- Current capacity: Maximum nesting 32 levels and maximum output size 64 MiB.
- Files: `src/sandbox/return_json.cpp`, `tests/test_sandbox_return.cpp`.
- Limit: Encoding beyond either cap throws; cycles, invalid UTF-8, unsupported values, and colliding JSON object keys also fail.
- Scaling path: Return compact summaries or split host work rather than increasing limits speculatively.

## Dependencies at Risk

**Pinned native dependency behaviors require targeted upgrade checks:**

- Risk: No specific vulnerability or unsupported version is established by this static audit. Observable integration risk comes from library defaults and build behavior.
- Files: `cmake/Dependencies.cmake`, `src/csv/csv_read.cpp`, `src/xlsx/xlsx_read.cpp`, `bindings/dart/hook/build.dart`, `bindings/dart/AGENTS.md`.
- Impact: Re-enabling csv-parser SIMD can impose AVX2 on shipped binaries; changing CSV header/variable-column defaults can lose rows; OpenXLSX's XML-result handling requires CheckedArchive validation; Dart/macOS build workarounds affect native asset discovery.
- Migration plan: Keep pinned versions and existing mitigations. On an upgrade verify parser tests, malformed XLSX fixtures, portability gates, and native assets across relevant platforms; do not invent replacement dependencies without a concrete failure.

## Missing Critical Features

**Read-only/opened database inspection cannot load UI sidecars — documented capability gap:**

- Problem: UI metadata loads only through from_migrations; that factory rejects read-only options. Plain open/from_schema handles therefore show bare enum codes, and group-column annotations are not rendered.
- Files: `src/database.cpp`, `src/database_impl.h`, `src/database_describe.cpp`, `src/ui_metadata.cpp`, `src/AGENTS.md`, `tests/test_database_ui_metadata.cpp`.
- Blocks: Read-only inspection tools cannot obtain the same sidecar-enriched text as migration-created writable handles.
- Classification: Known design gap, not a broken lazy-schema loader. A solution requires a separate UI-metadata loading design or persistence; do not attach sidecar loading to require_schema.

## Test Coverage Gaps

**Dart native-assets macOS/Windows paths lack direct CI exercise — documented gap:**

- What's not tested: The Dart hook's macOS toolchain workarounds and Windows native asset loading are not covered by the Linux Dart coverage job.
- Files: `.github/workflows/ci.yml`, `bindings/dart/hook/build.dart`, `bindings/dart/lib/src/ffi/library_loader.dart`, `bindings/dart/AGENTS.md`.
- Risk: A green cross-platform C++ matrix does not verify Dart's distinct asset registration, hook cache, or packaged framework paths.
- Priority: High when modifying the hook, loader, or platform-dependent ABI.

**Borrowed database invalidation lacks a shared checked-error contract:**

- What's not tested: Safe rejection of Sandbox.run after its Database closes cannot be guaranteed by the current native implementation.
- Files: `src/c/sandbox.cpp`, `src/sandbox/sandbox.cpp`, `bindings/python/src/quiverdb/sandbox.py`, `tests/test_sandbox_lifecycle.cpp`.
- Risk: Lifecycle tests cover Sandbox moves, while the distinct borrowed-owner lifetime remains the caller's responsibility.
- Priority: High if lifetime misuse must become supported error handling; first define a native contract, then test it across bindings.

**Local test aggregation lacks a missing-suite failure assertion:**

- What's not tested: The SKIP-to-success branch of the Windows aggregate runner.
- Files: `scripts/test-all.bat`.
- Risk: Developers can mistake incomplete verification for a complete pass.
- Priority: Medium; fix aggregator behavior and add the smallest command-level check that asserts an absent required executable is reported as incomplete/failing.

---

*Concerns audit: 2026-10-07*
