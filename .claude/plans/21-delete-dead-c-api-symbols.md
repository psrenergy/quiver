# 21 — Delete quiver_clear_last_error and the element counter functions (C API + C++)

**Batch** 3 · **Severity** l · **Breaking** yes: C callers of `quiver_clear_last_error`, `quiver_element_has_scalars`, `quiver_element_has_arrays`, `quiver_element_scalar_count` and `quiver_element_array_count`, and C++ callers of `Element::has_scalars()` / `Element::has_arrays()`. No binding's public API changes. Only Julia's low-level `Quiver.C` wrappers and the internal Dart and Python declarations for these symbols go. · **Size** S · **Layers** C++ core, C API, Julia FFI (generated), Dart FFI (hand-edited), Python cdef, docs/changelog
**Depends on** none. If plan 12 has landed, its `### Removed` section in `CHANGELOG.md` gets this bullet; if not, this plan creates the section.
**Overlaps with**
- **12** (binary-metadata builders): it creates the `### Removed` section under 0.11.0 that this plan appends to, and it regenerates `bindings/julia/src/c_api.jl`. It edits the `## Memory Management` block of `src/c/AGENTS.md` (~L112). This plan edits the `## Return Codes` line (~L49) and the `## Error Handling` paragraph (~L67-72) of the same file.
- **16, 17, 18, 20, 22, 23**: they also change C headers and regenerate or hand-edit the same FFI files (`bindings/julia/src/c_api.jl`, `bindings/dart/lib/src/ffi/bindings.dart`, `bindings/python/src/quiverdb/_c_api.py`). This plan touches different declarations. Anchor every edit by symbol name, not line number.
- **30** (Python stale docstrings): it rewrites the header comment of `_c_api.py` (currently ~L5-6, "Phase 1 CFFI declarations..."). This plan deletes other lines in that file (~L16, ~L87-90) and one line of the `check()` docstring in `_helpers.py`. They do not conflict.

## Why

Five exported C functions and two public C++ methods have no consumer anywhere. Each exists only for its own tests.

`git grep -nE "clear_last_error|has_scalars|has_arrays|scalar_count|array_count"` over the repo (HEAD 58dfe7a) finds only these hits:

- **`quiver_clear_last_error`**
  - Declaration and definition: `include/quiver/c/common.h:34` and `src/c/common.cpp:24-26`.
  - FFI declarations that nothing calls: `bindings/julia/src/c_api.jl:87-89` (generated), `bindings/dart/lib/src/ffi/bindings.dart:38-43` and `bindings/python/src/quiverdb/_c_api.py:16`. JS dropped it from `loader.ts` in #198 because nothing used it.
  - Two text mentions: a Python docstring that says the function is *not* called (`_helpers.py:11`: `Does NOT call quiver_clear_last_error() (matches Julia/Dart behavior).`), and the exception list at `src/c/AGENTS.md:49`.
  - No test, script, doc or CI step calls it.
- **`quiver_element_has_scalars` / `_has_arrays` / `_scalar_count` / `_array_count`**
  - Declarations and definitions: `include/quiver/c/element.h:44-47` and `src/c/element.cpp:138-164`.
  - The generated Julia and Dart wrappers, plus four cdef lines in `_c_api.py:87-90` that `element.py` never calls.
  - Their only real caller is `tests/test_c_api_element.cpp`.
  - `tests/test_c_api_database_create.cpp:696` has a local variable named `scalar_count`. It is unrelated.
- **C++ `Element::has_scalars()` / `has_arrays()`** (`include/quiver/element.h:36-37`, `src/element.cpp:98-104`)
  - Called only by the four C wrappers above and by `tests/test_element.cpp`.
  - The core reads elements through `scalars()` / `arrays()` (`src/database_create.cpp:9`, `src/database_update.cpp:13-14`, `src/database_impl.h:178,183`, `src/binary/binary_metadata.cpp:119-120`).
- **Downstream:** I also searched `C:\Development\Hub` and `C:\Development\Claw\claw1`. Neither calls any of these symbols.

The header comment on the error channel also documents a contract the code does not implement. `include/quiver/c/common.h:31-33`:

```c
// Error message capture - returns detailed error message from last operation
// Returns empty string if no error occurred. Thread-local storage.
QUIVER_C_API const char* quiver_get_last_error(void);
```

`quiver_set_last_error` is the only writer of the thread-local `g_last_error` (`src/c/common.cpp:10-16`). Every call to it in `src/c/` is followed by `return QUIVER_ERROR`. The callers are catch blocks, `QUIVER_REQUIRE` (`src/c/internal.h`, `QUIVER_REQUIRE_1`) and explicit error branches such as `src/c/element.cpp:74` `"Invalid values/count combination"`. Nothing writes the message on success.

**Reproduction of the wrong comment:**
1. Call `quiver_element_destroy(nullptr)`. It returns `QUIVER_ERROR`, and the message is `"Null argument: element"`.
2. Call `quiver_element_create(&e)`. It returns `QUIVER_OK`.
3. `quiver_get_last_error()` still returns `"Null argument: element"`, not the `""` the comment promises.

Every binding already reads the message only after a non-OK return: Julia `exceptions.jl:14`, Dart `exceptions.dart:8` and `lua_runner.dart:71`, JS `errors.ts:14`, Python `_helpers.py:15`. So the honest contract is "read it only after `QUIVER_ERROR`", and `quiver_clear_last_error` has no job in it.

**Principles violated:**
- Root `AGENTS.md` Principles: "Delete unused code, do not deprecate."
- The header comment misstates behaviour, which cuts against "Human-Centric: Codebase optimized for human readability".

## Constraints and decisions

- **Maintainer decision (binding):** "Only quiver_clear_last_error and quiver_element_has_scalars/has_arrays/scalar_count/array_count plus C++ Element::has_scalars/has_arrays (binary builders are plan 12). BREAKING; CHANGELOG 0.11.0. Regenerate Julia/Dart FFI or hand-edit per binding AGENTS.md; update Python cdef and _helpers.py docstring."
- **Root `AGENTS.md` Principles:** "All public C++ methods should be bound to C API". This is why the C++ `has_scalars`/`has_arrays` go together with their C wrappers, and are not left public with no binding.
  - `Element::scalars()` / `arrays()` stay. The core uses them. The C API never exposed their contents, only their sizes. That size exposure is what this plan removes.
- **Root `AGENTS.md` Design Decisions, "One C API error channel":** everything reports via `quiver_get_last_error`. That function stays, and the decision is untouched.
- **Root `AGENTS.md` Versioning / task facts:** 0.11.0 is unreleased and already a minor bump over 0.10.9. The entry goes under `## [0.11.0] — unreleased`, prefixed **BREAKING**.
  - *Correction to the verifiers' proposals:* both asked for "a 0.x minor bump across all five manifests". That is already done. **Do not bump any manifest.**
- **`bindings/dart/AGENTS.md`, "The checked-in `bindings.dart` predates the pinned ffigen (20.1.1)":** regenerating rewrites the whole file and turns the int-constant enums into Dart `enum`s, which breaks hub. C API changes are hand-edited in "in the file's existing style".
  - *Correction to the policy verifier's proposal:* it said "Regenerate the Julia and Dart FFI with scripts/generator.bat". That would run ffigen. **Hand-delete the Dart entries instead, and do not run `scripts/generator.bat`**, because it runs all three generators.
- **`bindings/julia/AGENTS.md`:** "`src/c_api.jl` GENERATED low-level FFI module (do not hand-edit; regenerate)". Run `bindings/julia/generator/generator.bat` only.
- **`bindings/python/AGENTS.md`:** "`_c_api.py` Hand-written CFFI cdef declarations (kept in sync manually)". Hand-delete.
- **`bindings/js/AGENTS.md`:** "No generator". `src/loader.ts` has none of these symbols (verified by grep), so JS needs no change.
- **Lua:** it binds the C++ `Database` directly through sol2 and never touches `Element::has_*`, so it needs no change.
- **Root `AGENTS.md` Principles, "Self-Updating":** update `src/c/AGENTS.md` (the exception list and the error-channel contract) and `bindings/dart/AGENTS.md` (the hand-edit record).

Alternatives rejected:
- **Clearing the message on success at every C entry point, so the old comment becomes true.** That adds defensive boilerplate across the whole C API for no consumer, since every binding reads the message only after a failure. The policy verifier rejected it too.
- **Keeping `quiver_clear_last_error` "for C users".** It has no caller and no test, and the principle says delete, not deprecate.
- **Keeping the element counters so C tests can observe the setters.** `quiver_element_to_string` already exposes an element's contents, and `SetArrayWithNullMask` already uses it that way. The rewritten tests use it too.
- **Replacing the C++ `has_*` assertions with nothing.** `Clear` and `ClearAndReuse` would then assert nothing about `clear()`. Rewrite them on `scalars()` / `arrays()` instead, as the facts verifier said.
- **Adding a test that pins "a successful call does not reset the message".** No behaviour changes here. The new comment tells callers to read the message only after `QUIVER_ERROR`, so no caller may depend on reset or non-reset. A test would turn an incidental property into a contract. I verified the claim against every `quiver_set_last_error` call site instead.

## Changes

Do the test rewrites (steps 9-10) **first**, then build and run them against the unchanged code. They use only surviving API (`quiver_element_to_string`, `scalars()`, `arrays()`), so they must pass before the deletion. That proves they are valid. Then do steps 1-8 and rebuild.

All `.cpp`, `.h`, `.py`, `.dart` and `.jl` files here are **LF**. All `.md` files are **CRLF** in the working tree. Use the Edit tool.

### 1. `include/quiver/c/common.h`: delete `quiver_clear_last_error`, fix the comment

Current (~L28-34):

```c
// Utility functions
QUIVER_C_API const char* quiver_version(void);

// Error message capture - returns detailed error message from last operation
// Returns empty string if no error occurred. Thread-local storage.
QUIVER_C_API const char* quiver_get_last_error(void);
QUIVER_C_API void quiver_clear_last_error(void);
```

New:

```c
// Utility functions
QUIVER_C_API const char* quiver_version(void);

// Message of the most recent failed call on this thread (thread-local storage).
// A successful call does not reset it, so read it only after a call returns QUIVER_ERROR.
QUIVER_C_API const char* quiver_get_last_error(void);
```

This deletes the dead declaration and states the contract the code actually implements (see Why).

### 2. `src/c/common.cpp`: delete the definition

Current (~L20-30, inside `extern "C"`):

```cpp
QUIVER_C_API const char* quiver_get_last_error(void) {
    return g_last_error.c_str();
}

QUIVER_C_API void quiver_clear_last_error(void) {
    g_last_error.clear();
}

QUIVER_C_API const char* quiver_version(void) {
```

New:

```cpp
QUIVER_C_API const char* quiver_get_last_error(void) {
    return g_last_error.c_str();
}

QUIVER_C_API const char* quiver_version(void) {
```

Leave the two `quiver_set_last_error` overloads and `g_last_error` unchanged.

### 3. `include/quiver/c/element.h`: delete the `// Accessors` block

Current (~L41-50):

```c
                                                            const uint8_t* has_value);

// Accessors
QUIVER_C_API quiver_error_t quiver_element_has_scalars(quiver_element_t* element, int* out_result);
QUIVER_C_API quiver_error_t quiver_element_has_arrays(quiver_element_t* element, int* out_result);
QUIVER_C_API quiver_error_t quiver_element_scalar_count(quiver_element_t* element, size_t* out_count);
QUIVER_C_API quiver_error_t quiver_element_array_count(quiver_element_t* element, size_t* out_count);

// Pretty print (caller must free returned string with quiver_database_free_string)
QUIVER_C_API quiver_error_t quiver_element_to_string(quiver_element_t* element, char** out_string);
```

New:

```c
                                                            const uint8_t* has_value);

// Pretty print (caller must free returned string with quiver_database_free_string)
QUIVER_C_API quiver_error_t quiver_element_to_string(quiver_element_t* element, char** out_string);
```

The first line is the last parameter line of `quiver_element_set_array_string`. Keep it.

### 4. `src/c/element.cpp`: delete the four accessor definitions

Delete these four functions entirely (currently ~L138-164), between the end of `quiver_element_set_array_string` and `quiver_element_to_string`:

```cpp
QUIVER_C_API quiver_error_t quiver_element_has_scalars(quiver_element_t* element, int* out_result) {
    QUIVER_REQUIRE(element, out_result);

    *out_result = element->element.has_scalars() ? 1 : 0;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_has_arrays(quiver_element_t* element, int* out_result) {
    QUIVER_REQUIRE(element, out_result);

    *out_result = element->element.has_arrays() ? 1 : 0;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_scalar_count(quiver_element_t* element, size_t* out_count) {
    QUIVER_REQUIRE(element, out_count);

    *out_count = element->element.scalars().size();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_array_count(quiver_element_t* element, size_t* out_count) {
    QUIVER_REQUIRE(element, out_count);

    *out_count = element->element.arrays().size();
    return QUIVER_OK;
}

```

Afterwards, `quiver_element_set_array_string`'s closing `}` is followed by one blank line and then `QUIVER_C_API quiver_error_t quiver_element_to_string(...)`. Leave the includes alone.

### 5. `include/quiver/element.h`: delete `has_scalars` / `has_arrays`

Current (~L32-39):

```cpp
    // Accessors
    const std::map<std::string, Value>& scalars() const;
    const std::map<std::string, std::vector<Value>>& arrays() const;

    bool has_scalars() const;
    bool has_arrays() const;

    void clear();
```

New:

```cpp
    // Accessors
    const std::map<std::string, Value>& scalars() const;
    const std::map<std::string, std::vector<Value>>& arrays() const;

    void clear();
```

### 6. `src/element.cpp`: delete the two definitions

Delete (currently ~L98-105, between `Element::arrays()` and `Element::clear()`):

```cpp
bool Element::has_scalars() const {
    return !scalars_.empty();
}

bool Element::has_arrays() const {
    return !arrays_.empty();
}

```

`Element::to_string()` already tests `scalars_.empty()` / `arrays_.empty()` directly. Leave it as it is.

### 7. FFI declarations

**7a. Julia: `bindings/julia/src/c_api.jl` (generated).** Run from the repo root:

```
bindings/julia/generator/generator.bat
```

It runs `julia +1.12.5 --project=<generator dir> generator.jl`, which needs that juliaup channel. The expected diff is exactly the removal of these five wrappers, with their trailing blank lines, about 20 lines:

```julia
function quiver_clear_last_error()
    @ccall libquiver_c.quiver_clear_last_error()::Cvoid
end
```

(currently ~L87-89, right after `quiver_get_last_error`)

```julia
function quiver_element_has_scalars(element, out_result)
    @ccall libquiver_c.quiver_element_has_scalars(element::Ptr{quiver_element_t}, out_result::Ptr{Cint})::quiver_error_t
end

function quiver_element_has_arrays(element, out_result)
    @ccall libquiver_c.quiver_element_has_arrays(element::Ptr{quiver_element_t}, out_result::Ptr{Cint})::quiver_error_t
end

function quiver_element_scalar_count(element, out_count)
    @ccall libquiver_c.quiver_element_scalar_count(element::Ptr{quiver_element_t}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_element_array_count(element, out_count)
    @ccall libquiver_c.quiver_element_array_count(element::Ptr{quiver_element_t}, out_count::Ptr{Csize_t})::quiver_error_t
end
```

(currently ~L560-574, between `quiver_element_set_array_string` and `quiver_element_to_string`)

Do **not** run `scripts/generator.bat`. It also runs Dart's ffigen (see 7b).

**7b. Dart: `bindings/dart/lib/src/ffi/bindings.dart` (hand-edit, do NOT run ffigen).** Delete two regions.

(i) Currently ~L38-44, right after the `_quiver_get_last_error` lookup and before `quiver_database_options_default`. Delete these lines and the blank line after them:

```dart
  void quiver_clear_last_error() {
    return _quiver_clear_last_error();
  }

  late final _quiver_clear_last_errorPtr = _lookup<ffi.NativeFunction<ffi.Void Function()>>('quiver_clear_last_error');
  late final _quiver_clear_last_error = _quiver_clear_last_errorPtr.asFunction<void Function()>();

```

Afterwards `late final _quiver_get_last_error = ...asFunction<ffi.Pointer<ffi.Char> Function()>();` is followed by one blank line and then `  quiver_database_options_t quiver_database_options_default() {`.

(ii) Currently ~L3379-3446. Delete everything from `  int quiver_element_has_scalars(` down to and including the blank line after `_quiver_element_array_count`'s `.asFunction<...>();`. That is these four blocks:

```dart
  int quiver_element_has_scalars(
    ffi.Pointer<quiver_element_t1> element,
    ffi.Pointer<ffi.Int> out_result,
  ) {
    return _quiver_element_has_scalars(
      element,
      out_result,
    );
  }

  late final _quiver_element_has_scalarsPtr =
      _lookup<ffi.NativeFunction<ffi.Int32 Function(ffi.Pointer<quiver_element_t1>, ffi.Pointer<ffi.Int>)>>(
        'quiver_element_has_scalars',
      );
  late final _quiver_element_has_scalars = _quiver_element_has_scalarsPtr
      .asFunction<int Function(ffi.Pointer<quiver_element_t1>, ffi.Pointer<ffi.Int>)>();

  int quiver_element_has_arrays(
    ...same shape, 'quiver_element_has_arrays'...
      .asFunction<int Function(ffi.Pointer<quiver_element_t1>, ffi.Pointer<ffi.Int>)>();

  int quiver_element_scalar_count(
    ffi.Pointer<quiver_element_t1> element,
    ffi.Pointer<ffi.Size> out_count,
  ) {
    ...same shape, 'quiver_element_scalar_count'...
      .asFunction<int Function(ffi.Pointer<quiver_element_t1>, ffi.Pointer<ffi.Size>)>();

  int quiver_element_array_count(
    ...same shape, 'quiver_element_array_count'...
  late final _quiver_element_array_count = _quiver_element_array_countPtr
      .asFunction<int Function(ffi.Pointer<quiver_element_t1>, ffi.Pointer<ffi.Size>)>();

```

Afterwards, the `_quiver_element_set_array_string` lookup's closing `>();` is followed by one blank line and then `  int quiver_element_to_string(`.

Check that nothing else in `bindings/dart` references the deleted names:

```
git grep -n -E "quiver_clear_last_error|quiver_element_(has_scalars|has_arrays|scalar_count|array_count)" -- bindings/dart
```

This must print nothing. Today only `bindings.dart` matches.

**7c. Python: `bindings/python/src/quiverdb/_c_api.py` (hand-edit).** In the `ffi.cdef("""...""")` block:

- Under `// common.h` (currently ~L14-16):
  ```
      const char* quiver_version(void);
      const char* quiver_get_last_error(void);
      void quiver_clear_last_error(void);
  ```
  becomes:
  ```
      const char* quiver_version(void);
      const char* quiver_get_last_error(void);
  ```
- After `quiver_element_set_array_string` (currently ~L86-92):
  ```
                                                      const uint8_t* has_value);

      quiver_error_t quiver_element_has_scalars(quiver_element_t* element, int* out_result);
      quiver_error_t quiver_element_has_arrays(quiver_element_t* element, int* out_result);
      quiver_error_t quiver_element_scalar_count(quiver_element_t* element, size_t* out_count);
      quiver_error_t quiver_element_array_count(quiver_element_t* element, size_t* out_count);

      quiver_error_t quiver_element_to_string(quiver_element_t* element, char** out_string);
  ```
  becomes:
  ```
                                                      const uint8_t* has_value);

      quiver_error_t quiver_element_to_string(quiver_element_t* element, char** out_string);
  ```

**7d. JS: no change.** `bindings/js/src/loader.ts` declares none of the five symbols (verified: `git grep` over `bindings/js` finds nothing).

### 8. `bindings/python/src/quiverdb/_helpers.py`: `check()` docstring

Current (~L7-12):

```python
def check(err: int) -> None:
    """Raise QuiverError if err indicates a C API failure.

    Reads the thread-local error message via quiver_get_last_error().
    Does NOT call quiver_clear_last_error() (matches Julia/Dart behavior).
    """
```

New:

```python
def check(err: int) -> None:
    """Raise QuiverError if err indicates a C API failure.

    Reads the thread-local error message via quiver_get_last_error().
    """
```

The deleted line names a function that no longer exists. The body is unchanged.

### 9. `tests/test_element.cpp`: drop the `has_*` assertions

Each Set* test already reads the value back through `scalars()` / `arrays()` right after the `has_*` line. `.at()` throws, and gtest reports a failure, if the value is missing, so these lines are redundant. **Delete** these single lines:

- `DefaultEmpty` (~L6-7): `EXPECT_FALSE(element.has_scalars());` and `EXPECT_FALSE(element.has_arrays());`. The `EXPECT_TRUE(element.scalars().empty());` / `arrays().empty()` lines that follow stay.
- `SetInt` (~L16), `SetFloat` (~L25), `SetString` (~L33), `SetNull` (~L41): `EXPECT_TRUE(element.has_scalars());`
- `SetArrayInt` (~L49), `SetArrayFloat` (~L62), `SetArrayString` (~L74), `SetArrayValueWithNulls` (~L86): `EXPECT_TRUE(element.has_arrays());`

**Rewrite** `Clear` (currently ~L106-117). New full body:

```cpp
TEST(Element, Clear) {
    quiver::Element element;
    element.set("label", std::string{"test"}).set("data", std::vector<double>{1.0});

    EXPECT_EQ(element.scalars().size(), 1);
    EXPECT_EQ(element.arrays().size(), 1);

    element.clear();

    EXPECT_TRUE(element.scalars().empty());
    EXPECT_TRUE(element.arrays().empty());
}
```

**Rewrite** `ClearAndReuse` (currently ~L219-238). New full body:

```cpp
TEST(Element, ClearAndReuse) {
    quiver::Element element;
    element.set("label", std::string("Original")).set("data", std::vector<double>{1.0, 2.0});

    EXPECT_EQ(element.scalars().size(), 1);
    EXPECT_EQ(element.arrays().size(), 1);

    element.clear();

    EXPECT_TRUE(element.scalars().empty());
    EXPECT_TRUE(element.arrays().empty());

    // Reuse after clear: only the new entries are present
    element.set("new_label", std::string("Reused")).set("new_data", std::vector<int64_t>{3, 4, 5});

    EXPECT_EQ(element.scalars().size(), 1);
    EXPECT_EQ(element.arrays().size(), 1);
    EXPECT_EQ(std::get<std::string>(element.scalars().at("new_label")), "Reused");
    EXPECT_EQ(element.arrays().at("new_data").size(), 3);
}
```

`size() == 1` after reuse is stronger than the old `has_*() == true`. It also proves the pre-clear keys `label` / `data` are gone.

### 10. `tests/test_c_api_element.cpp`: observe elements through `quiver_element_to_string`

After the deletion, `quiver_element_to_string` is the only way to read an element back through the C API. Add this helper after the includes (currently after L5 `#include <string>`):

```cpp
namespace {

// quiver_element_to_string is the only way to read an element back through the C API.
std::string element_string(quiver_element_t* element) {
    char* out = nullptr;
    EXPECT_EQ(quiver_element_to_string(element, &out), QUIVER_OK);
    std::string result = out ? out : "";
    quiver_database_free_string(out);
    return result;
}

}  // namespace
```

`quiver_database_free_string` is declared in `quiver/c/database.h`, which is already included. It accepts `nullptr` (`StringFreeNull` pins that).

`Element::to_string()` (`src/element.cpp`) prints:
- `"Element {\n"`;
- if there are scalars, `"  scalars:\n"` then `"    <name>: <value>\n"` for each, in `std::map` (sorted) order;
- if there are arrays, `"  arrays:\n"` then `"    <name>: [v, v]\n"` for each;
- then `"}"`.

Values print like this: an int as `std::to_string` (`42`), a double as `std::to_string`, which is `%f` (`3.140000`), a string as `"..."`, and NULL as `null`.

Replace the accessor assertions in these tests. Keep each test's create, set and destroy lines as they are; only the lines shown change.

- **`EmptyElement`**: replace all four accessor blocks (`int has_scalars ...` through `EXPECT_EQ(array_count, 0);`) with:
  ```cpp
      EXPECT_EQ(element_string(element), "Element {\n}");
  ```
- **`SetInt`**: replace the `int has_scalars ...` through `EXPECT_EQ(scalar_count, 1);` lines, after `quiver_element_set_integer(element, "count", 42)`, with:
  ```cpp
      EXPECT_EQ(element_string(element),
                "Element {\n"
                "  scalars:\n"
                "    count: 42\n"
                "}");
  ```
- **`SetFloat`**: replace the `has_scalars` block with:
  ```cpp
      EXPECT_EQ(element_string(element),
                "Element {\n"
                "  scalars:\n"
                "    value: 3.140000\n"
                "}");
  ```
- **`SetString`**: replace the `has_scalars` block with:
  ```cpp
      EXPECT_EQ(element_string(element),
                "Element {\n"
                "  scalars:\n"
                "    label: \"Plant 1\"\n"
                "}");
  ```
- **`SetNull`**: replace the `has_scalars` block with:
  ```cpp
      EXPECT_EQ(element_string(element),
                "Element {\n"
                "  scalars:\n"
                "    empty: null\n"
                "}");
  ```
- **`SetArrayInt`**: replace the `has_arrays` / `array_count` lines with:
  ```cpp
      EXPECT_EQ(element_string(element),
                "Element {\n"
                "  arrays:\n"
                "    counts: [10, 20, 30]\n"
                "}");
  ```
- **`SetArrayFloat`**: replace them with:
  ```cpp
      EXPECT_EQ(element_string(element),
                "Element {\n"
                "  arrays:\n"
                "    costs: [1.500000, 2.500000, 3.500000]\n"
                "}");
  ```
- **`SetArrayString`**: replace them with:
  ```cpp
      EXPECT_EQ(element_string(element),
                "Element {\n"
                "  arrays:\n"
                "    tags: [\"important\", \"urgent\", \"review\"]\n"
                "}");
  ```
- **`Clear`**: new full body:
  ```cpp
  TEST(ElementCApi, Clear) {
      quiver_element_t* element = nullptr;
      ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
      ASSERT_NE(element, nullptr);

      quiver_element_set_integer(element, "id", 1);
      double values[] = {1.0, 2.0};
      quiver_element_set_array_float(element, "data", values, 2, nullptr);

      EXPECT_EQ(element_string(element),
                "Element {\n"
                "  scalars:\n"
                "    id: 1\n"
                "  arrays:\n"
                "    data: [1.000000, 2.000000]\n"
                "}");

      EXPECT_EQ(quiver_element_clear(element), QUIVER_OK);

      EXPECT_EQ(element_string(element), "Element {\n}");

      EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
  }
  ```
- **`NullAccessors`** (currently ~L216-225): **delete the whole test.** It only exercised the removed functions' null guards.
- **`MultipleScalars`**: replace `size_t scalar_count = 0;` through `EXPECT_EQ(scalar_count, 3);` with:
  ```cpp
      EXPECT_EQ(element_string(element),
                "Element {\n"
                "  scalars:\n"
                "    capacity: 50.000000\n"
                "    id: 1\n"
                "    label: \"Plant 1\"\n"
                "}");
  ```
  The keys come out sorted (`std::map`), not in insertion order.

Leave `CreateAndDestroy`, `DestroyNull`, `SetArrayWithNullMask`, `ClearNull`, `NullElementErrors`, `NullNameErrors`, `ToString`, `ToStringNull`, `StringFreeNull` and `ArrayNullErrors` unchanged. You may leave `SetArrayWithNullMask` and `ToString` calling `quiver_element_to_string` by hand; converting them to the helper is optional.

## Tests

- **C++ (`tests/test_element.cpp`)**, suite `Element`: 23 tests before, and still 23 after. No test is added or deleted. Nine tests lose a redundant line (step 9), and `Clear` / `ClearAndReuse` are rewritten on `scalars()` / `arrays()`. No fixture or schema is involved.
- **C API (`tests/test_c_api_element.cpp`)**, suite `ElementCApi`: 21 tests before, 20 after (`NullAccessors` is deleted).
  - `EmptyElement`, `SetInt`, `SetFloat`, `SetString`, `SetNull`, `SetArrayInt`, `SetArrayFloat`, `SetArrayString`, `Clear` and `MultipleScalars` now assert the exact `quiver_element_to_string` output (step 10).
  - These are strictly stronger than before. The old tests proved only that *something* was set, or a count. The new ones prove the name, the value and the type formatting reached the element.
  - No fixture or schema is involved.
- **Which test fails before the fix:** none can, because this is a deletion. The evidence has three parts:
  1. The rewritten tests use only surviving API, so they pass on the unchanged code. Do steps 9-10 first and run them.
  2. After steps 1-6 the whole tree still compiles, so nothing else depended on the deleted symbols.
  3. The leftover `git grep` in Verification step 6 prints nothing.
- **Lua, Julia, Dart, Python, JS:** no test changes. No binding test references these symbols (verified by grep). Their suites run in Verification only to prove nothing broke.
  - A stale FFI declaration would not fail them anyway. Julia `@ccall`, Dart `late final` lookups and CFFI ABI-mode attributes all resolve lazily, which is why step 6 of Verification greps for the deleted names.
- **No new schema files.**

## Docs and changelog

**`src/c/AGENTS.md`** (CRLF). There are two edits.

(a) `## Return Codes` (currently ~L49). Old:

```
Exceptions: `quiver_get_last_error`, `quiver_version`, `quiver_clear_last_error`, `quiver_database_options_default`, `quiver_csv_options_default` (utility functions with direct return).
```

New:

```
Exceptions: `quiver_get_last_error`, `quiver_version`, `quiver_database_options_default`, `quiver_csv_options_default` (utility functions with direct return).
```

(b) `## Error Handling`, the paragraph after the code block (currently ~L67-72). It ends with:

```
because its typed-dispatch deallocation can. All components (LuaRunner included) report through
the single `quiver_get_last_error` channel; there are no per-handle error channels.
```

Append one sentence to that paragraph, so it ends:

```
because its typed-dispatch deallocation can. All components (LuaRunner included) report through
the single `quiver_get_last_error` channel; there are no per-handle error channels. Nothing resets
that message: a successful call leaves the previous failure's text in place, so it is read only
after a call returns `QUIVER_ERROR` (every binding's `check` does exactly that).
```

Do not touch the `## Memory Management` block (~L112). Plan 12 owns it.

**`bindings/dart/AGENTS.md`** (CRLF). In the bullet "**The checked-in `bindings.dart` predates the pinned ffigen (20.1.1).**", the current text reads:

```
  `quiver_database_upsert_time_series_row` plus its `_by_label` form, and
  `quiver_database_update_relation` plus its `_by_label` form.
  Take the generator upgrade as its own deliberate change (regenerate, then fix the enum call
```

New:

```
  `quiver_database_upsert_time_series_row` plus its `_by_label` form, and
  `quiver_database_update_relation` plus its `_by_label` form. Removals are hand-deleted the same
  way (`quiver_clear_last_error` and the four `quiver_element_*` has/count accessors).
  Take the generator upgrade as its own deliberate change (regenerate, then fix the enum call
```

If an earlier plan (17, 18, 20, 22 or 23) already extended this list, add the "Removals are hand-deleted..." sentence at the end of whatever list is there.

**No other AGENTS.md changes.**
- Root `AGENTS.md`, `src/AGENTS.md`, `tests/AGENTS.md`, `bindings/julia/AGENTS.md`, `bindings/python/AGENTS.md` and `bindings/js/AGENTS.md` do not mention these symbols (verified by grep).
- The root "Element Class" section documents only the fluent `set` builder.

**No other docs.** `docs/*.md`, `README.md`, the binding READMEs and `bindings/js/src/lua-api.ts` do not mention them (verified by grep).

**`CHANGELOG.md`** (CRLF). Put the bullet under `## [0.11.0] — unreleased` → `### Removed`.
- If plan 12 already created `### Removed`, which sits after the last `### Changed` entry and before `### Fixed`, append this bullet after plan 12's bullet.
- Otherwise create `### Removed` in that position. Keep a Changelog order is Added, Changed, Removed, Fixed.

```markdown
- **BREAKING — `quiver_clear_last_error`, the C element accessors, and C++ `Element::has_scalars` /
  `has_arrays`.** `quiver_clear_last_error`, `quiver_element_has_scalars`,
  `quiver_element_has_arrays`, `quiver_element_scalar_count` and `quiver_element_array_count` are
  removed from the C API, along with the two C++ methods behind them. Nothing called them: no binding
  read an element back or cleared the error message. Julia's generated `Quiver.C` wrappers and the
  internal Dart and Python declarations for them are gone too. No binding's public API changes. The
  `quiver_get_last_error` header comment is corrected. It used to say the message is empty when no
  error occurred, but a successful call never reset it, so after a failure every later successful
  call still reported the old message.

  *Adapt:* in C, delete calls to `quiver_clear_last_error` and read `quiver_get_last_error` only
  after a call returns `QUIVER_ERROR`. To inspect an element, use `quiver_element_to_string`. In C++,
  replace `element.has_scalars()` / `element.has_arrays()` with `!element.scalars().empty()` /
  `!element.arrays().empty()`.
```

No manifest version bump. 0.11.0 is already the minor bump.

## Verification

Run from the repo root (`C:\Development\Quiver\quiver1`), in this order.

1. **Tests first, against the unchanged code.** Apply steps 9 and 10 only, then:
   ```
   cmake --build build --config Debug
   ./build/bin/quiver_tests.exe --gtest_filter='Element.*'
   ./build/bin/quiver_c_tests.exe --gtest_filter='ElementCApi.*'
   ```
   Expected results:
   - `Element.*`: 23 tests pass.
   - `ElementCApi.*`: 20 tests pass.
   - If an `element_string` expectation fails, fix the expected literal, not the product code. Compare it with the actual output gtest prints. The format is defined by `Element::to_string()`.
2. **Apply steps 1-8**, then build:
   ```
   cmake --build build --config Debug
   ```
   It must compile and link with no reference to a deleted symbol.
3. **Targeted suites again:**
   ```
   ./build/bin/quiver_tests.exe --gtest_filter='Element.*'
   ./build/bin/quiver_c_tests.exe --gtest_filter='ElementCApi.*'
   ```
   The results are the same as in step 1: 23 and 20 tests pass, and `ElementCApi.NullAccessors` no longer exists.
4. **Full native suites:**
   ```
   ./build/bin/quiver_tests.exe
   ./build/bin/quiver_c_tests.exe
   ```
   All pass.
5. **Regenerate Julia FFI:**
   ```
   bindings/julia/generator/generator.bat
   git diff --stat -- bindings/julia/src/c_api.jl
   git diff -- bindings/julia/src/c_api.jl
   ```
   The diff must be the deletion of the 5 wrapper functions (about 20 lines). See Pitfalls if other hunks appear.
6. **Leftover check**, which must print nothing:
   ```
   git grep -n -E "quiver_clear_last_error|quiver_element_(has_scalars|has_arrays|scalar_count|array_count)|has_scalars|has_arrays" -- . ':!CHANGELOG.md'
   ```
   The unrelated local `scalar_count` in `tests/test_c_api_database_create.cpp` does not match this pattern.
7. **Binding suites** (they exercise the rebuilt library; no test changed):
   ```
   bindings/julia/test/test.bat
   bindings/dart/test/test.bat
   bindings/python/tests/test.bat
   bindings/js/test/test.bat
   ```
   All pass.
8. **Format:**
   ```
   scripts/format.bat
   ```
   Then run `git status`. Only the files listed under Acceptance criteria may be modified. If the formatter touched any other file, revert it with `git checkout -- <file>`: pre-existing lint debt is a root "Do Not Fix" item. No `.bat` file may show as changed.
9. **Everything:**
   ```
   scripts/test-all.bat
   ```
   All steps pass (six suites plus the `quiver_cli` smoke test).
   - If the CLI smoke test fails because `example/example1.lua` is missing, that is the pre-existing breakage plan 65 fixes. It is unrelated to this change.
   - Confirm that the six suites pass.

## Acceptance criteria

- [ ] `include/quiver/c/common.h` declares no `quiver_clear_last_error`. The comment above `quiver_get_last_error` says the message is from the most recent failed call and is read only after `QUIVER_ERROR`.
- [ ] `src/c/common.cpp` defines no `quiver_clear_last_error`.
- [ ] `include/quiver/c/element.h` has no `// Accessors` block. `src/c/element.cpp` defines none of the four accessor functions.
- [ ] `include/quiver/element.h` / `src/element.cpp` have no `has_scalars` / `has_arrays`.
- [ ] `bindings/julia/src/c_api.jl` was regenerated with `bindings/julia/generator/generator.bat`, and the five wrappers are gone.
- [ ] `bindings/dart/lib/src/ffi/bindings.dart` was hand-edited, not regenerated: the five method/lookup blocks are gone, and nothing else in the file changed.
- [ ] `bindings/python/src/quiverdb/_c_api.py` has no cdef for the five functions. The `_helpers.py` `check()` docstring no longer names `quiver_clear_last_error`.
- [ ] `tests/test_element.cpp` has no `has_*` call. `Clear` / `ClearAndReuse` assert on `scalars()` / `arrays()`.
- [ ] `tests/test_c_api_element.cpp` has the `element_string` helper. The ten tests listed in step 10 assert exact `to_string` output, and `NullAccessors` is deleted.
- [ ] `src/c/AGENTS.md` (exception list and error-channel sentence) and `bindings/dart/AGENTS.md` (hand-removal note) are updated, with CRLF preserved.
- [ ] `CHANGELOG.md` 0.11.0 `### Removed` has the **BREAKING** bullet with an *Adapt:* line. No manifest version changed.
- [ ] The Verification step 6 `git grep` prints nothing. `Element.*` passes 23 tests, `ElementCApi.*` passes 20, and all native and binding suites pass.

## Pitfalls

- **Order matters for the test file.** `tests/test_c_api_element.cpp` calls the four accessors today. If you delete them from the header first, the C test target stops compiling. Rewrite the tests first (Verification step 1).
- **Do not run `scripts/generator.bat`,** and do not run `dart run ffigen`. Either one regenerates `bindings.dart` with the pinned ffigen 20.1.1, which turns `quiver_error_t` / `quiver_data_type_t` / `quiver_log_level_t` into Dart enums and breaks hub (`bindings/dart/AGENTS.md`). Run only `bindings/julia/generator/generator.bat`.
- **The Julia generator rewrites the whole file** from the current headers. If `git diff` of `c_api.jl` shows hunks beyond the five removals, an earlier plan changed a C header without regenerating.
  - Those hunks are correct, because they mirror the headers. Keep them and mention them in the commit message.
  - Do not hand-delete the wrappers as a shortcut. The generator needs `julia +1.12.5` (a juliaup channel).
- **Stale declarations do not fail binding tests.** Julia `@ccall`, Dart `late final _lookup` and CFFI ABI-mode functions all resolve on first use, and nothing calls these. A missed removal only shows up in the step 6 grep.
- **`EXPECT_*` inside the helper is fine. `ASSERT_*` is not:** gtest's fatal assertions need a `void` function, and `element_string` returns `std::string`.
- **Float formatting in the expected strings:** `Element::to_string` uses `std::to_string(double)`, which is `%f` in the "C" locale (`3.14` prints as `3.140000`). `quiver_c_tests` never changes the locale. The only `setlocale` in the tests is in `tests/test_database_csv_import.cpp`, which runs in the other executable and restores the locale with RAII.
- **Keep each literal on its own line, ending in `\n`.** clang-format keeps a line break after a string literal that ends in `\n`, so the multi-line `EXPECT_EQ` literals survive `scripts/format.bat`. A single-line form of `Clear`'s expectation is over the 120-column limit.
- **Line endings:** every `.md` edited here (`CHANGELOG.md`, `src/c/AGENTS.md`, `bindings/dart/AGENTS.md`) is CRLF in the working tree. The `.h` / `.cpp` / `.py` / `.dart` / `.jl` files are LF. Use the Edit tool, not `sed`, and touch no `.bat` file.
- **Stale native libraries in caches:** `bindings/dart/.dart_tool/...` and the Python venv may still hold a `libquiver_c` that exports the old symbols until they rebuild. That is harmless, because no binding declares or calls them any more.
- **Earlier plans shift lines** in `c_api.jl`, `bindings.dart`, `_c_api.py` and `CHANGELOG.md` (plans 12, 16, 17, 18, 20). Find every edit by symbol name or quoted text.

## Out of scope

- The seven `quiver_binary_metadata_*` builders and C++ `BinaryMetadata::add_dimension` / `add_time_dimension`: **plan 12**.
- Clearing the error message on success at every entry point: rejected above.
- Upgrading the Dart ffigen output (the enum churn): a separate, deliberate change per `bindings/dart/AGENTS.md`. No plan owns it.
- The `_c_api.py` header comment ("Phase 1 CFFI declarations...") and other stale Python docstrings: **plan 30**.
- Python's `Element.clear` / `_ensure_valid`: **plan 29**. That plan leaves the `quiver_element_clear` C function in place; this plan does not touch it either.
- Converting `SetArrayWithNullMask` / `ToString` in `tests/test_c_api_element.cpp` to the new helper: optional, and not required.
- Any include cleanup in `src/c/element.cpp`: not needed for this change.
