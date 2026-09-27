# 45 — Lua: close `BinaryFile` handles (readers and writers) when `run()` returns

**Batch** 5 · **Severity** medium · **Breaking** yes — a Lua script that reuses a `db:open_file` handle from an earlier `run()` (kept in a global) now gets a closed handle; reopen the file in each run · **Size** S · **Layers** C++ Lua runner (`src/lua_runner.cpp`), C++ Lua tests, `src/CLAUDE.md`, `bindings/js/src/lua-api.ts`, CHANGELOG
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

- `src/CLAUDE.md`, the "A writer left open when the script returns is still flushed" paragraph.
  Add: "`db:open_file` handles, readers and writers, are recorded the same way (a `weak_ptr` in
  `Impl::open_binary_files`) and closed by `close_open_writers()`, so no binary file handle
  outlives its `run()` either. A writer left in a global would otherwise hold its path in the
  process-wide write registry until the `LuaRunner` is destroyed."
- `bindings/js/src/lua-api.ts`, binary file section. Find it with
  `grep -n "db:open_file" bindings/js/src/lua-api.ts`. Add one sentence: "A file handle does not
  outlive the \`run()\` that opened it: any handle still open when the script returns is closed
  (and a writer flushed), so reopen the file in each script." Escape backticks as `\``.
- `CHANGELOG.md`, under `## [0.11.0] — unreleased` → `### Changed`:
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

- [ ] `open_file` returns `std::shared_ptr<BinaryFile>` and registers a `weak_ptr`.
- [ ] `close_open_writers()` closes surviving binary files and clears the list.
- [ ] Both new tests pass, and the binary/expression/CSV Lua suites stay green.
- [ ] `src/CLAUDE.md`, lua-api.ts and the CHANGELOG are updated.

## Pitfalls

- Switching `std::unique_ptr` to `std::shared_ptr` changes sol2's holder type. If any other binding
  code does `o.as<std::unique_ptr<BinaryFile>&>()`, it breaks. Check with
  `grep -n "unique_ptr<BinaryFile>" src/lua_runner.cpp`.
- `close_open_writers()` also runs on exception unwinding (GcGuard). Keep the new loop
  exception-safe with try/catch, as the CSV loop is.

## Out of scope

- Thread-safety or multi-process safety of the binary write registry (documented as not provided).
- Julia's standalone `open_file` (not a Lua run).
