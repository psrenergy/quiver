# 45 — Lua: close `BinaryFile` handles (readers and writers) when `run()` returns

**Batch** 5 · **Severity** medium · **Breaking** yes — a Lua script that reuses a `db:open_file` handle from an earlier `run()` (kept in a global) now gets a closed handle; reopen the file in each run · **Size** S · **Layers** C++ Lua runner (`src/lua_runner.cpp`), C++ Lua tests, `src/AGENTS.md`, `bindings/js/src/lua-api.ts`, CHANGELOG
**Depends on** none · **Overlaps with** 43/44 (same lua-api.ts file; add one sentence in the binary section), 51 (operator metamethods for BinaryFile in the same file; independent), 16 (aggregation parser, same file)

## Why

`LuaRunner::run` flushes CSV writers the script left open at scope exit, even ones held in a Lua
**global**. Globals are GC roots, so collection alone never finalizes them. See `src/lua_runner.cpp`
(`struct GcGuard`, currently ~L2287-2305):

```cpp
    struct GcGuard {
        ...
        ~GcGuard() {
            impl.close_open_writers();
            ...collect_garbage();
```

The CSV mechanism is `Impl::open_writers` (~L279), a list of `weak_ptr<csv_write::Writer>`, plus
`close_open_writers()` (~L303-315). Its motivation is in the comment above it: *"A CsvWriter the
script left reachable -- `w = db:write_csv(...)` without `local`, the Lua default -- is a GC root,
so collect_garbage() never finalizes it"*. `LuaRunner_WriteCsv.UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns`
(`tests/test_lua_runner_write_csv.cpp` ~L1396) pins it.

`db:open_file` has no such registration (~L693-703):

```cpp
        bind.set_function(
            "open_file",
            [](Database& self, const std::string& path, const std::string& mode, sol::optional<BinaryMetadata> metadata)
                -> std::unique_ptr<BinaryFile> {
                ...
                const auto resolved = resolve_sandboxed_path(self, "open_file", path);
                std::optional<BinaryMetadata> md = metadata ? std::optional<BinaryMetadata>(*metadata) : std::nullopt;
                return std::make_unique<BinaryFile>(BinaryFile::open_file(resolved, mode[0], md));
            });
```

A `BinaryFile` opened for writing registers its canonical path in the process-global write
registry (`src/binary/binary_file.cpp`, `write_registry`). `BinaryFile::close()` (~L110-121)
unregisters it. So `f = db:open_file('bin_a', 'w', md); f:write(...)` without `local` and without
`f:close()` leaves the path **blocked for reading and writing in the whole process** after `run()`
returns, and the data may not be flushed. That holds until the `LuaRunner` (and its Lua state) is
destroyed. The next script's `db:open_file('bin_a', 'r')` throws.

Principle: one uniform rule — "a file handle does not outlive its `run()`", the rule CSV writers
already follow.

## Constraints and decisions

- **Maintainer notes (binding):**
  - BREAKING, with a CHANGELOG entry.
  - A `weak_ptr` list next to `open_writers`. No generic or type-erased registry.
  - Add a line to the lua-api.ts binary section.
- Close **readers as well as writers**. One rule is simpler to state, and a reader still holds an
  OS handle.
- `BinaryFile::close()` is idempotent: it returns early when `!impl_->io`. So closing a file the
  script already closed is harmless.
- sol2 handles a usertype held by `std::shared_ptr` transparently. The existing `BinaryFile&`
  method bindings (`f:write`, `f:read`, `f:close`, `f:get_metadata`, `quiver.expression(f)`, the
  operator metamethods) keep working. Verify with the tests below.

## Changes — `src/lua_runner.cpp`

### 1. A second registry next to `open_writers` (~L279)

Current:
```cpp
    std::vector<std::pair<std::string, std::weak_ptr<quiver::csv_write::Writer>>> open_writers;
```
Add directly below it:
```cpp
    // Every BinaryFile db:open_file handed out during the current run(), readers and writers
    // alike, so close_open_writers() can close it at run()'s exit even when the script keeps it in
    // a global (a GC root). A writer left open would otherwise keep its path in the process-wide
    // write registry until the LuaRunner is destroyed.
    std::vector<std::weak_ptr<BinaryFile>> open_binary_files;
```

### 2. `close_open_writers()` also closes binary files (~L303-315)

Append before the closing brace, after `open_writers.clear();`:
```cpp
        for (const auto& weak : open_binary_files) {
            if (const auto file = weak.lock()) {
                try {
                    file->close();
                } catch (const std::exception&) {
                    // Same as the CSV loop above: nobody to report a flush failure to at scope exit.
                }
            }
        }
        open_binary_files.clear();
```
Keep the function name. Update its comment's first sentence so it says "every writer and binary
file handle".

### 3. `db:open_file` returns a `shared_ptr` and registers it (~L693-703)

New:
```cpp
        bind.set_function(
            "open_file",
            [this](Database& self, const std::string& path, const std::string& mode, sol::optional<BinaryMetadata> metadata)
                -> std::shared_ptr<BinaryFile> {
                if (mode.size() != 1 || (mode[0] != 'r' && mode[0] != 'w')) {
                    throw std::runtime_error("Cannot open_file: mode must be \"r\" or \"w\"");
                }
                const auto resolved = resolve_sandboxed_path(self, "open_file", path);
                std::optional<BinaryMetadata> md = metadata ? std::optional<BinaryMetadata>(*metadata) : std::nullopt;
                auto file = std::make_shared<BinaryFile>(BinaryFile::open_file(resolved, mode[0], md));
                open_binary_files.push_back(file);
                return file;
            });
```
Check that `bind_database` is an `Impl` member, as `write_csv`'s lambda is: that lambda pushes into
`open_writers` directly. If `open_file` is bound in a free function or a different member, capture
the `Impl&` the same way `write_csv` does.

### 4. Comments

- The `GcGuard` comment (~L2287-2298): where it says `close_open_writers()` closes CSV writers,
  make it "CSV writers and binary files".
- The CsvWriter/BinaryFile ownership comments near ~L251-257 and ~L817: say that binary files use
  the same mechanism. Check with `grep -n "BinaryFile" src/lua_runner.cpp | head -20`.

## Tests — `tests/test_lua_binary.cpp`

Model the test on `UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns` and use the file's
`LuaBinaryTest` fixture (`db_path()`, `schema` and `sandbox` members, as in `WriteReadRoundTrip`,
~L75):

```cpp
TEST_F(LuaBinaryTest, WriterHeldInAGlobalIsClosedWhenRunReturns) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    // No `local`, no f:close(): f is a GC root when run() returns.
    lua.run(R"(
        local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW',
            labels={'v1'}, dimensions={'row'}, dimension_sizes={2} }
        f = db:open_file('bin_global', 'w', md)
        f:write({42.0}, {row=1})
        f:write({43.0}, {row=2})
    )");
    // A second run can open it for reading: the path is no longer in the write registry, and
    // the data was flushed.
    lua.run(R"(
        local r = db:open_file('bin_global', 'r')
        assert(r:read({row=1})[1] == 42.0, 'row 1')
        assert(r:read({row=2})[1] == 43.0, 'row 2')
        r:close()
    )");
}

TEST_F(LuaBinaryTest, HandleFromAnEarlierRunIsClosed) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW',
            labels={'v1'}, dimensions={'row'}, dimension_sizes={1} }
        local w = db:open_file('bin_reuse', 'w', md); w:write({1.0}, {row=1}); w:close()
        g = db:open_file('bin_reuse', 'r')
    )");
    lua.run(R"(assert(not g:is_open(), 'a handle must not outlive its run()'))");
}
```

Check the metadata keyword names against the existing `WriteReadRoundTrip` test. That test uses
`labels`, `dimensions` and `dimension_sizes`, which this test copies. Before the change, the first
test's second `run()` throws "... is currently open for writing" (the registry message; get its
exact wording with `grep -n "open for writing" src/binary/binary_file.cpp`). The second test's
assertion fails.

Also run the whole binary and expression Lua suites, which exercise `f:` methods, metamethods and
`quiver.expression(f)` on the now shared-ptr-held usertype:
`--gtest_filter=LuaBinaryTest*:LuaExpression*`.

## Docs and changelog

- `src/AGENTS.md`, the "A writer left open when the script returns is still flushed" paragraph.
  Add: "`db:open_file` handles, readers and writers, are recorded the same way (a `weak_ptr` in
  `Impl::open_binary_files`) and closed by `close_open_writers()`, so no binary file handle
  outlives its `run()` either. A writer left in a global would otherwise hold its path in the
  process-wide write registry until the `LuaRunner` is destroyed."
- `bindings/js/src/lua-api.ts`, binary file section. Find it with
  `grep -n "db:open_file" bindings/js/src/lua-api.ts`. Add one sentence: "A file handle does not
  outlive the \`run()\` that opened it: any handle still open when the script returns is closed
  (and a writer flushed), so reopen the file in each script." Escape backticks as `\``.
- `CHANGELOG.md`, under `## [0.12.0] — unreleased` → `### Changed`:
  ```markdown
  - **BREAKING — Lua: `db:open_file` handles are closed when `run()` returns.** A binary file a
    script left open (for example in a global, without `f:close()`) used to stay open, so a writer
    kept its path blocked for reading and writing in the whole process. Readers and writers now
    follow the rule CSV writers already did. *Adapt:* reopen the file in each `run()` instead of
    reusing a handle kept in a global.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaBinaryTest*:LuaExpression*:LuaRunner_WriteCsv*`
3. `./build/bin/quiver_tests.exe` (the full C++ suite)
4. `bindings/js/test/test.bat test/lua-api-sync.test.ts`
5. `bindings/julia/test/test.bat test_lua_runner.jl` (the Julia LuaRunner tests run Lua scripts)
6. `scripts/format.bat`

## Acceptance criteria

- [x] `open_file` returns `std::shared_ptr<BinaryFile>` and registers a `weak_ptr`.
- [x] `close_open_writers()` closes surviving binary files and clears the list.
- [x] Both new tests pass, and the binary/expression/CSV Lua suites stay green.
- [x] `src/AGENTS.md`, lua-api.ts and the CHANGELOG are updated.

## Pitfalls

- Switching `std::unique_ptr` to `std::shared_ptr` changes sol2's holder type. If any other binding
  code does `o.as<std::unique_ptr<BinaryFile>&>()`, it breaks. Check with
  `grep -n "unique_ptr<BinaryFile>" src/lua_runner.cpp`.
- `close_open_writers()` also runs on exception unwinding (GcGuard). Keep the new loop
  exception-safe with try/catch, as the CSV loop is.

## Out of scope

- Thread-safety or multi-process safety of the binary write registry (documented as not provided).
- Julia's standalone `open_file` (not a Lua run).

## Implementation notes

- **Starting point.** At planning time `rs/plan45` held `origin/master` `b4c62bb`, so
  `git merge origin/master` was a no-op. The branch was then fast-forwarded to `b7fe57e` (plans
  43 and 44 merged, #355/#356) at 12:53 and 12:55, before the first edit to `src/lua_runner.cpp`
  and before the red build. So every edit, the red run and the green run are on top of 43/44. A
  final `git fetch && git merge origin/master` reported "Already up to date".
- **Should this be implemented? Yes.**
  - The only downstream consumer, claw (`src/core/lua-worker.ts`), builds one `LuaRunner` per
    worker process. It runs a sandbox preamble, then the agent's script in a single `run()`, so it
    never reuses a handle across runs.
  - `Expression(const BinaryFile&)` copies only the path (`src/expression/expression.cpp:18`).
    `ExpressionFile` opens and closes its own reader inside `save()`. So an expression built from a
    handle in an earlier run still saves.
  - No test in any layer reused a `db:open_file` handle across `run()` calls.
- **Red first.** The two new tests, built against the unfixed `lua_runner.cpp`:
  - `WriterHeldInAGlobalIsClosedWhenRunReturns`: `Failed to run Lua script: Cannot open_file:
    file is already open for writing: C:\Users\...\quiver_lua_LuaBinaryTest_WriterHeldInAGlobalIsClosedWhenRunReturns\bin_global`.
  - `HandleFromAnEarlierRunIsClosed`: `Failed to run Lua script: [string "assert(not
    g:is_open(), ..."]:1: a handle must not outlive its run()`.
  - `[  FAILED  ] 2 tests`. Both pass after the fix.
- **Drift fixed:**
  - *CHANGELOG section.* `## [0.12.0] — unreleased` does not exist: 0.12.0 through 0.12.6 are
    tagged, and the manifests are at 0.12.7. Plan 43 had already opened `## [0.12.7] — unreleased`
    with a `### Fixed`. The entry is the plan's text verbatim, under a new `### Changed` placed
    before that `### Fixed` (Keep a Changelog order). There is no manifest bump. Like plans 42 and
    earlier, this BREAKING patch still contradicts root AGENTS.md's "breaking ⇒ 0.x minor bump"
    rule. That is left for the maintainer.
  - *Line numbers.* Every anchor matched the plan's ~line numbers before 43/44. Plan 44 added 8
    lines to `lua_runner.cpp`, which shifted them slightly. The functions were re-anchored by
    name.
  - *Comments.* The plan named the comments at ~L251 and ~L817 and the `GcGuard` block. Both of
    the first two said BinaryFile uses "the same ownership pattern" (a `unique_ptr`), which is now
    false. They now say BinaryFile's holder is a `std::shared_ptr` so that `Impl` can keep a
    `weak_ptr`. The `GcGuard` comment says `close_open_writers()` closes "CSV writers and binary
    files". The `run()` header comment ("any other unique_ptr + sol::no_constructor usertype, e.g.
    CsvWriter") is about GC finalization, is still true, and was left alone.
  - *src/AGENTS.md.* The plan's sentence went in, plus the two pinning test names.
  - *lua-api.ts.* The sentence is appended to the binary-section prose paragraph, next to the
    operators sentence (plan 44 kept that paragraph intact).
- **No change needed in the tests' fixtures or keys.** `labels` / `dimensions` /
  `dimension_sizes` match `WriteReadRoundTrip` and `md1()`. `bind_database()` is an `Impl`
  member, so `[this]` works exactly as in `write_csv`. `unique_ptr<BinaryFile>` appeared only in
  `open_file`. sol2 keys every unique holder, `unique_ptr` or `shared_ptr`, on the same
  `d::u<BinaryFile>` metatable, so `o.is<BinaryFile>()` / `o.as<BinaryFile&>()` in
  `to_expression` and every `f:` method and metamethod keep working.
- **Adversarial review.** One read-only Workflow ran three lenses over the diff, each with a
  refute-by-default verify stage:
  - code: sol2 holder semantics, noexcept `GcGuard`, idempotent `close()`, close-before-collect
    order, `[this]` across `LuaRunner` moves;
  - behaviour: expressions across runs, a handle the script already closed, a throwing script,
    `expr:save`'s own writer, weak_ptr growth, cross-run reuse in every binding's tests;
  - docs/CHANGELOG accuracy.

  **No findings.** Each lens returned an empty list, confirmed in `journal.jsonl`.
- **Verification (all on `b7fe57e` + this diff):**
  - `cmake --build build --config Debug`: OK.
  - `quiver_tests --gtest_filter=LuaBinaryTest*:LuaExpression*:LuaRunner_WriteCsv*`: **104/104**.
  - `quiver_tests` (full): **1382/1382**. `quiver_c_tests`: **571/571** (not in the plan's list;
    run because `test_c_api_lua_runner.cpp` drives `db:open_file`).
  - `bindings/js/test/test.bat test/lua-api-sync.test.ts`: `test.bat` passes `test` as a
    positional as well, so this runs the whole JS suite: **242 pass, 0 fail** (21 files, including
    lua-api-sync).
  - `bindings/julia/test/test.bat test_lua_runner.jl`: LuaRunner **24/24**.
  - `scripts/format.bat`: exits 0.
    - clang-format re-wrapped the `db:open_file` call (the capture made the lambda header too
      long) and nothing else of this diff. The sync regex `bind.set_function(\s*"open_file"` still
      matches the new shape, and lua-api-sync passes after the reformat.
    - One comment of mine had been corrupted by an edit that ended mid-line. It went over 120
      columns and got reflowed, so it was re-wrapped by hand to the file's 100 columns;
      `clang-format -i` then leaves the file byte-identical.
    - JuliaFormatter, dart format (44 files, 0 changed) and ruff (35 unchanged) changed nothing.
      The Python step rebuilds the quiverdb wheel, which took 11 min here.
    - Biome rewrote 43 JS files CRLF→LF. `git diff --ignore-cr-at-eol` showed only `lua-api.ts`
      with a content change, so the other 42 were restored with `git checkout` and `lua-api.ts`
      was set back to CRLF. No `.bat` file was touched.
    - After the reformat: rebuild, then the Lua suites 104/104, full C++ 1382/1382, C API 571/571,
      JS 242/242 and Julia LuaRunner 24/24 all passed again.
- **For later plans:**
  - **51** (operator metamethods registered once for BinaryFile and Expression) edits the
    `new_usertype<BinaryFile>` block. This plan left that block untouched; only `db:open_file`'s
    return type changed, and sol2 dispatches the metamethods the same way for a `shared_ptr` holder.
  - **16** (named in this plan's Overlaps header) landed long ago (`c753faf`), so there is nothing to
    coordinate with it.
  - **46-50, 52** edit other functions of `lua_runner.cpp`. The only shared names are
    `close_open_writers()`, which is now also the binary closer (its name was kept, per the plan),
    and `Impl::open_binary_files`.
  - **Batch-5 CHANGELOG.** `## [0.12.7] — unreleased` now has `### Changed` (this entry) above
    plan 43/44's `### Fixed`. A later BREAKING plan in this batch (48) should append to `### Changed`.
