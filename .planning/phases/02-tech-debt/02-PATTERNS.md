# Phase 2: Phase 1 Tech Debt - Pattern Map

**Mapped:** 2026-05-06
**Files analyzed:** 9 (5 source/header edits + 1 collateral edit + 2 test additions; 1 header is test target only)
**Analogs found:** 9 / 9 (every modification has an in-tree pattern to copy)

---

## File Classification

| Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---------------|------|-----------|----------------|---------------|
| `src/expr/nodes.cpp` | service (AST validator + row computer) | transform / pure-function | self (existing patterns inside the file) | exact — the throw idioms, translation table construction, and helper `find_dim_index` are already present and re-used |
| `include/quiver/expr/node.h` | model (private members of `BinaryOpNode`) | declaration | self (existing `lhs_dim_translate_` / `rhs_dim_translate_` declarations at lines 110-111) | exact |
| `src/binary/binary_file.cpp` | service (file I/O + RAII registry) | file-I/O / request-response | self (existing `Impl::~Impl()` cleanup at lines 37-44; `BinaryFile::write_registry` lifecycle) | exact |
| `include/quiver/binary/binary_file.h` | model (public/protected member declarations) | declaration | self (deletions only — patterns are pre-existing) | exact |
| `src/binary/iteration.cpp` | service (stateless iterator step) | transform | self (existing `next_dimensions` body at 104-140) | exact |
| `src/binary/csv_converter.cpp` | service (format converter, derives from `BinaryFile`) | streaming / batch | `tests/test_expression.cpp:62-77` (write_qvr loop using free-function `quiver::binary::next_dimensions` with `std::optional` unwrap) | exact — the optional-unwrap shape is already canonicalized in the test fixture |
| `include/quiver/expr/expression.h` | header (test target only) | none | not modified | n/a |
| `tests/test_expression.cpp` | test (4 D-23 cases + 1 D-25 case) | unit | `TEST_F(ExpressionFixture, TimePropertiesMismatchThrows)` at lines 335-362 | exact for D-23; near-duplicate templates available |
| `tests/test_binary_file.cpp` | test (1 D-24 case) | unit | `MoveAssignWriterUnregistersOldPath` (lines 399-419) + `MovedWriterClearsRegistryOnDestruction` (lines 464-471) | exact — both demonstrate the registry-leak negative-path testing idiom |

---

## Pattern Assignments

### `src/expr/nodes.cpp` — CR-01 + WR-01 (compatibility loop restructure)

**Analog:** self — `BinaryOpNode` constructor at `src/expr/nodes.cpp:117-130` (existing same-name shape-rule loop, structurally identical to the new mirror loop)

**Existing single-side iteration with `find_dim_index` lookup** (lines 117-130):
```cpp
for (const auto& l_dim : lhs_meta.dimensions) {
    int r_idx = find_dim_index(rhs_meta.dimensions, l_dim.name);
    if (r_idx < 0)
        continue;  // dim only on lhs; D-02 keeps verbatim
    int64_t l_size = l_dim.size;
    int64_t r_size = rhs_meta.dimensions[r_idx].size;
    if (l_size == r_size)
        continue;
    if (l_size == 1 || r_size == 1)
        continue;
    throw std::runtime_error("Cannot apply binary operation: dimension '" + l_dim.name +
                             "' has incompatible sizes " + std::to_string(l_size) + " vs " +
                             std::to_string(r_size) + " (broadcasting requires n×n, 1×n, or n×1)");
}
```

**Existing time-dim compatibility loop to replace** (lines 132-151):
```cpp
for (const auto& l_dim : lhs_meta.dimensions) {
    if (!l_dim.is_time_dimension())
        continue;
    int r_idx = find_dim_index(rhs_meta.dimensions, l_dim.name);
    if (r_idx < 0)
        continue;
    const auto& r_dim = rhs_meta.dimensions[r_idx];
    if (!r_dim.is_time_dimension()) {
        throw std::runtime_error("Cannot apply binary operation: dimension '" + l_dim.name +
                                 "' is a time dimension on lhs but not on rhs");
    }
    const auto& lp = *l_dim.time;
    const auto& rp = *r_dim.time;
    if (lp.frequency != rp.frequency || lp.initial_value != rp.initial_value ||
        lp.parent_dimension_index != rp.parent_dimension_index) {
        throw std::runtime_error("Cannot apply binary operation: time dimension '" + l_dim.name +
                                 "' has incompatible TimeProperties");
    }
}
```

**Existing helper** (lines 64-70):
```cpp
int find_dim_index(const std::vector<Dimension>& dims, const std::string& name) {
    for (size_t i = 0; i < dims.size(); ++i) {
        if (dims[i].name == name)
            return static_cast<int>(i);
    }
    return -1;
}
```

**Pattern to copy:** preserve the `for (const auto& l_dim : lhs_meta.dimensions)` outer iteration; preserve the `find_dim_index(rhs_meta.dimensions, l_dim.name)` lookup; preserve the `continue` early-exit for dims unique to one side. The new loop replaces the lhs-only `if (!l_dim.is_time_dimension())` guard with a symmetric `bool l_time = ...; bool r_time = ...; if (l_time != r_time) throw` check. Per RESEARCH.md "Pattern 1 — Compatibility Loop Restructure" Pass A is sufficient (the symmetric check fires regardless of which side has the time dim).

**Parent-by-name resolution pattern** — already established at `nodes.cpp:214-235` for the broadcast_meta_ recompute loop:
```cpp
// Determine source: prefer lhs (Pass 1) if present; else rhs.
int src_idx = find_dim_index(lhs_meta.dimensions, out_d.name);
const auto* src_meta = &lhs_meta;
if (src_idx < 0) {
    src_idx = find_dim_index(rhs_meta.dimensions, out_d.name);
    src_meta = &rhs_meta;
}
// src_idx is non-negative because out_d came from one of the operands.
int64_t src_parent_idx = src_meta->dimensions[src_idx].time->parent_dimension_index;
if (src_parent_idx < 0) {
    out_d.time->parent_dimension_index = -1;
    continue;
}
const std::string& parent_name = src_meta->dimensions[src_parent_idx].name;
```

**Apply this:** the WR-01 fix uses the exact same name-lookup idiom — `(parent_idx >= 0) ? meta.dimensions[parent_idx].name : std::string{}` — to resolve each side's parent and compare names instead of indices.

---

### `src/expr/nodes.cpp` — CR-03 (error verb conformance, all 7 throws in `BinaryOpNode` ctor)

**Analog:** self — D-11 introduced `"Cannot save: ..."` (`src/expr/expression.cpp:67`) and existing reads use `"Cannot read: ..."` (`src/binary/binary_file.cpp:126, 165, 196`). Both follow CLAUDE.md three-pattern rule.

**Existing `"Cannot read:"` pattern** (`src/binary/binary_file.cpp:126`):
```cpp
throw std::runtime_error("Cannot read: data at {" + dim_str + "} contains null values");
```

**Existing `"Cannot open_file:"` pattern** (`src/binary/binary_file.cpp:60`):
```cpp
throw std::runtime_error("Cannot open_file: file is already open for writing: " + canonical);
```

**Pattern to copy:** verb without object — single short word matching the operation. For `BinaryOpNode` ctor, D-18 specifies `"Cannot apply: {reason}"`. Mechanical find/replace of `"Cannot apply binary operation:"` → `"Cannot apply:"` across the 7 throw sites at `nodes.cpp:110, 127, 141, 148, 157, 168, 177`. The new CR-01 mirror-reject message authored in this same commit also uses `"Cannot apply: ..."`.

---

### `src/expr/nodes.cpp` — WR-06 (reverse translation table)

**Analog:** self — D-05 forward-table construction at `nodes.cpp:246-259`

**Existing forward table construction** (lines 246-259):
```cpp
const auto& out_dims = broadcast_meta_.dimensions;
lhs_dim_translate_.resize(out_dims.size());
rhs_dim_translate_.resize(out_dims.size());
lhs_dim_sizes_.assign(out_dims.size(), 0);
rhs_dim_sizes_.assign(out_dims.size(), 0);
for (size_t i = 0; i < out_dims.size(); ++i) {
    int li = find_dim_index(lhs_meta.dimensions, out_dims[i].name);
    int ri = find_dim_index(rhs_meta.dimensions, out_dims[i].name);
    lhs_dim_translate_[i] = li;
    rhs_dim_translate_[i] = ri;
    lhs_dim_sizes_[i] = (li >= 0) ? lhs_meta.dimensions[li].size : 0;
    rhs_dim_sizes_[i] = (ri >= 0) ? rhs_meta.dimensions[ri].size : 0;
}
```

**Existing O(n²) inner-search loops to replace** (`nodes.cpp:283-310`):
```cpp
for (size_t li = 0; li < lhs_dims_buf_.size(); ++li) {
    int out_i = -1;
    for (size_t k = 0; k < lhs_dim_translate_.size(); ++k) {
        if (lhs_dim_translate_[k] == static_cast<int>(li)) {
            out_i = static_cast<int>(k);
            break;
        }
    }
    int64_t coord = dims[out_i];
    if (lhs_dim_sizes_[out_i] == 1)
        coord = 1;
    lhs_dims_buf_[li] = coord;
}
// Identical block for rhs at lines 298-310.
```

**Pattern to copy:** append a second loop right after the existing forward-table loop (after line 259) that walks `out_dims` once more and writes the inverse:
```cpp
// Sizes already captured above; now build the inverse map operand→output.
lhs_to_out_.assign(lhs_meta.dimensions.size(), -1);
rhs_to_out_.assign(rhs_meta.dimensions.size(), -1);
for (size_t out_i = 0; out_i < out_dims.size(); ++out_i) {
    if (lhs_dim_translate_[out_i] >= 0)
        lhs_to_out_[lhs_dim_translate_[out_i]] = static_cast<int>(out_i);
    if (rhs_dim_translate_[out_i] >= 0)
        rhs_to_out_[rhs_dim_translate_[out_i]] = static_cast<int>(out_i);
}
```

The Pass-1/Pass-2 invariant of `broadcast_meta_` construction (`nodes.cpp:195-210`) guarantees every operand dim appears in `out_dims`, so no `-1` entries survive at the end of population. Per RESEARCH.md "Anti-Patterns" — do NOT add defensive `if (out_i < 0)` checks in `compute_row` after the lookup.

---

### `include/quiver/expr/node.h` — WR-06 (reverse translation table member declarations)

**Analog:** self — existing private declarations at lines 110-111

**Existing forward-table member declarations** (lines 108-116):
```cpp
// For each output-dim index: the corresponding index in lhs/rhs metadata.dimensions,
// or -1 if absent. Built once at construction; used by compute_row (D-05 translation).
std::vector<int> lhs_dim_translate_;
std::vector<int> rhs_dim_translate_;

// For each output-dim index: lhs/rhs's size for that dim, or 0 if absent. Used to
// detect size-1 broadcast at compute_row time (clamp output coord to 1 on size-1 dims).
std::vector<int64_t> lhs_dim_sizes_;
std::vector<int64_t> rhs_dim_sizes_;
```

**Pattern to copy:** add two new `std::vector<int>` private members after the forward-table block, with a comment explaining their relation to the forward tables. Per RESEARCH.md Pattern 3 they are declared as:
```cpp
// For each operand-dim index (lhs/rhs operand metadata.dimensions): the corresponding
// index in the output (broadcast_meta_) dimensions. Always >= 0 (no -1) because every
// operand dim appears in the output (D-02 Pass 1 / Pass 2 invariant). Built once at
// construction alongside the forward translation tables; used by compute_row (WR-06).
std::vector<int> lhs_to_out_;
std::vector<int> rhs_to_out_;
```

---

### `src/expr/nodes.cpp` — WR-07 (apply_op exhaustive-switch fall-through)

**Analog:** D-18 verb pattern from `"Cannot apply: {reason}"`

**Existing `apply_op` fall-through to replace** (`nodes.cpp:82-94`):
```cpp
double apply_op(BinaryOp op, double a, double b) {
    switch (op) {
    case BinaryOp::Add:
        return a + b;
    case BinaryOp::Subtract:
        return a - b;
    case BinaryOp::Multiply:
        return a * b;
    case BinaryOp::Divide:
        return a / b;
    }
    return 0.0;  // unreachable; silences MSVC missing-return warning
}
```

**Pattern to copy:** the throw uses the same `"Cannot apply:"` verb established by D-18 for consistency:
```cpp
throw std::runtime_error("Cannot apply: unhandled BinaryOp variant");
```

This sits in an anonymous namespace helper, so no header change. The post-switch path becomes a runtime canary if a new variant lands in `BinaryOp` enum without an `apply_op` update.

---

### `src/binary/binary_file.cpp` — CR-02 (write-registry late-insert)

**Analog:** self — existing `Impl::~Impl()` registry-erase mechanism (lines 37-44) which already correctly handles partial-construction cleanup IF `registered_path` is non-empty.

**Existing destructor that consumes the late-insert state** (lines 37-44):
```cpp
~Impl() {
    if (!registered_path.empty()) {
        write_registry.erase(registered_path);
    }
    if (io) {
        io->flush();
    }
}
```

**Existing `case 'w':` body to restructure** (lines 81-100):
```cpp
case 'w': {
    // Validate metadata provided
    if (!metadata.has_value()) {
        throw std::invalid_argument("Metadata must be provided when opening a file in write mode.");
    }

    write_registry.insert(canonical);   // ← MOVE this to end

    // Write metadata to TOML file
    std::ofstream toml_file(file_path + std::string(TOML_EXTENSION));
    toml_file << metadata->to_toml();   // ← can throw via validate()

    // Open binary data file
    auto io =
        std::make_unique<std::fstream>(file_path + std::string(QVR_EXTENSION), std::ios::out | std::ios::binary);
    BinaryFile binary_file(file_path, metadata.value(), std::move(io));
    binary_file.impl_->registered_path = canonical;   // ← MOVE this to end
    binary_file.fill_file_with_nulls();   // ← can throw on disk-full
    return binary_file;
}
```

**Existing prologue check that must NOT be touched** (line 59):
```cpp
if (write_registry.count(canonical)) {
    throw std::runtime_error("Cannot open_file: file is already open for writing: " + canonical);
}
```

**Pattern to copy:** follow RESEARCH.md "Pattern 2 — Late-Insert Write Registry" verbatim. Move both lines to the bottom of the case body (after `fill_file_with_nulls()`). Do NOT add try/catch (CLAUDE.md "Clean code over defensive code"). The destructor pattern is unchanged — `registered_path` only becomes non-empty when the registry has been populated, so the `Impl::~Impl()` `erase` always matches the `insert`.

---

### `src/binary/binary_file.cpp` and `include/quiver/binary/binary_file.h` — WR-02 (delete `validate_file_is_open`)

**Analog:** none needed — pure deletion. RESEARCH.md grep confirms zero callers across `src/`, `tests/`, `include/`, `bindings/`, `src/c/`.

**Existing dead function to delete** (`binary_file.cpp:263-267`):
```cpp
void BinaryFile::validate_file_is_open() const {
    if (!impl_->io || !impl_->io->good()) {
        throw std::runtime_error("File is not open: " + impl_->file_path);
    }
}
```

**Existing dead declaration to delete** (`binary_file.h:67`):
```cpp
void validate_file_is_open() const;
```

**Pattern to copy:** verify with one final grep before the delete (per D-20). No code copies; only structural removal.

---

### `src/binary/binary_file.cpp` and `include/quiver/binary/binary_file.h` — WR-03 + WR-04 (delete duplicate `dimension_sizes_at_values` + protected `next_dimensions`)

**Analog:** the canonical implementation in `src/binary/iteration.cpp:21-91` (anonymous-namespace `dimension_sizes_at_values`) — already imported via `quiver::binary::next_dimensions`.

**Existing duplicate to delete in `binary_file.cpp:394-464`:**
```cpp
std::vector<int64_t> BinaryFile::dimension_sizes_at_values(const std::vector<int64_t>& dimension_values) const {
    using namespace quiver::time;
    const auto& metadata = impl_->metadata;
    const auto& dimensions = metadata.dimensions;
    // ... 70 lines, byte-for-byte identical to iteration.cpp:21-91
}
```

**Existing thin delegate to delete in `binary_file.cpp:381-392`:**
```cpp
std::vector<int64_t> BinaryFile::next_dimensions(const std::vector<int64_t>& current_dimensions) {
    auto next = quiver::binary::next_dimensions(impl_->metadata, current_dimensions);
    if (!next) {
        return quiver::binary::first_dimensions(impl_->metadata);
    }
    return *next;
}
```

**Existing protected declarations to delete** (`binary_file.h:75-77`):
```cpp
// Dimension iteration
std::vector<int64_t> next_dimensions(const std::vector<int64_t>& current_dimensions);
std::vector<int64_t> dimension_sizes_at_values(const std::vector<int64_t>& dimension_values) const;
```

**Pattern to copy:** structural deletion only. The free-function `quiver::binary::next_dimensions` (`include/quiver/binary/iteration.h:24`) is the survivor. Per Pitfall 7 in RESEARCH.md, recommended commit ordering is WR-03 first (delete duplicate `dimension_sizes_at_values` body in `binary_file.cpp`, leaving the thin `next_dimensions` delegate temporarily), then WR-04 (delete the delegate + headers + update `csv_converter.cpp`).

---

### `src/binary/csv_converter.cpp` — WR-04 collateral (re-point `next_dimensions` calls)

**Analog:** `tests/test_expression.cpp:62-77` (the `write_qvr` fixture helper) — the canonical established shape for using the optional return type from `quiver::binary::next_dimensions`.

**Established optional-unwrap pattern** (`tests/test_expression.cpp:65-76`):
```cpp
auto writer = BinaryFile::open_file(path, 'w', meta);
std::vector<int64_t> dims = quiver::binary::first_dimensions(meta);
std::vector<double> row(meta.labels.size());
for (;;) {
    for (size_t k = 0; k < row.size(); ++k)
        row[k] = fill(dims, k);
    writer.write(row, dims);
    auto nxt = quiver::binary::next_dimensions(meta, dims);
    if (!nxt)
        break;
    dims = std::move(*nxt);
}
```

Same pattern at `tests/test_expression.cpp:85-93` (read_all_cells helper) and at `src/expr/expression.cpp:77-86` (Expression::save row loop, per CONTEXT integration point).

**Existing csv_converter call sites to update** (`src/binary/csv_converter.cpp:85-89` and `:130-134`):
```cpp
// csv_to_bin loop, line 85:
current_dimensions = csv_reader.next_dimensions(current_dimensions);
if (current_dimensions == initial_dimensions) {
    break;
}

// bin_to_csv loop, line 130:
current_dimensions = csv_writer.next_dimensions(current_dimensions);
if (current_dimensions == initial_dimensions) {
    break;
}
```

**Pattern to copy:** replace each pair (assign + equality compare) with a single optional-unwrap, mirroring the test-fixture shape:
```cpp
auto nxt = quiver::binary::next_dimensions(csv_reader.get_metadata(), current_dimensions);
if (!nxt)
    break;
current_dimensions = std::move(*nxt);
```

The metadata accessor `BinaryFile::get_metadata()` is already public (`binary_file.h:53`). `CSVConverter` inherits publicly from `BinaryFile` (`include/quiver/binary/csv_converter.h:17`), so `csv_reader.get_metadata()` works without any new exposed surface.

---

### `src/binary/binary_file.cpp` — WR-05 (`validate_dimension_values` map-to-indexed dedup)

**Analog:** self — the existing `validate_dimension_values_indexed` body at `binary_file.cpp:323-372` (the survivor).

**Existing duplicate to dedup** (`binary_file.cpp:269-321`):
```cpp
void BinaryFile::validate_dimension_values(const std::unordered_map<std::string, int64_t>& dims) {
    const auto& metadata = impl_->metadata;
    const auto& dimensions = metadata.dimensions;
    if (dims.size() != dimensions.size()) {
        throw std::invalid_argument("Expected " + std::to_string(dimensions.size()) + " dimensions, got " +
                                    std::to_string(dims.size()));
    }
    for (size_t i = 0; i < dimensions.size(); ++i) {
        const auto& dim = dimensions[i];
        auto it = dims.find(dim.name);
        if (it == dims.end()) {
            throw std::invalid_argument("Missing required dimension: '" + dim.name + "'");
        }
        // ... 40 lines of bounds check + time-dim consistency, identical to indexed version
    }
}
```

**Pattern to copy:** RESEARCH.md "Pattern 4 — Map-to-Indexed Dedup" exactly. The map version retains:
- The count check (preserves the "Expected N dimensions, got M" error verbatim).
- The map-to-vector conversion loop (preserves the "Missing required dimension: 'X'" error verbatim).

After conversion, delegate to the existing `validate_dimension_values_indexed(indexed_dims)`. Do not modify the indexed version. Per CONTEXT.md D-21, the conversion overhead is irrelevant on the slow path.

---

### `src/binary/iteration.cpp` — WR-09 (next_dimensions `bool incremented` flag)

**Analog:** self — the existing implementation at lines 104-140.

**Existing function to modify** (`src/binary/iteration.cpp:104-140`):
```cpp
std::optional<std::vector<int64_t>> next_dimensions(const BinaryMetadata& meta,
                                                    const std::vector<int64_t>& current) {
    const auto& dimensions = meta.dimensions;
    const auto current_sizes = dimension_sizes_at_values(meta, current);

    std::vector<int64_t> next = current;

    for (int i = static_cast<int>(next.size()) - 1; i >= 0; --i) {
        if (next[i] < current_sizes[i]) {
            next[i] += 1;
            break;
        } else {
            next[i] = 1;
        }
    }

    // Adjust time dimensions which were reset to 1 before their parent dimension is incremented.
    for (size_t i = 0; i < next.size(); ++i) { /* ... unchanged ... */ }

    // End-of-iteration: if we wrapped back to the initial position, signal nullopt.
    if (next == first_dimensions(meta)) {   // ← per-call rebuild — REPLACE
        return std::nullopt;
    }
    return next;
}
```

**Pattern to copy:** RESEARCH.md "Pattern 5 — bool incremented Flag" verbatim. Add `bool incremented = false;` before the increment loop, set `incremented = true;` adjacent to the existing `break;` statement, and replace the post-loop equality check:
```cpp
if (!incremented)
    return std::nullopt;
```

The time-dim adjustment loop and the function signature stay unchanged. RESEARCH.md verifies the existing 5 `IterationTest` cases cover both the wrap and no-wrap paths.

---

### `tests/test_expression.cpp` — D-23 (4 mirror/parent tests)

**Analog:** `TEST_F(ExpressionFixture, TimePropertiesMismatchThrows)` at `tests/test_expression.cpp:335-362`.

**Existing template** (lines 335-362):
```cpp
TEST_F(ExpressionFixture, TimePropertiesMismatchThrows) {
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "block"})
                                                 .set("dimension_sizes", {3, 12})
                                                 .set("time_dimensions", {"block"})
                                                 .set("frequencies", {"monthly"})
                                                 .set("labels", {"v1", "v2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "block"})  // no time_dimensions
                                                 .set("dimension_sizes", {3, 12})
                                                 .set("labels", {"v1", "v2"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
}
```

**Existing positive-case template** for accept-path tests (lines 206-225, `AddTwoFiles`):
```cpp
TEST_F(ExpressionFixture, AddTwoFiles) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) { /* fill */ });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) { /* fill */ });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) + Expression(b);
    e.save(path_out);
    // ... assertions on round-trip values
}
```

**Pattern to copy for D-23 #1 (Mirror A — lhs non-time + rhs time):** clone the `TimePropertiesMismatchThrows` body and swap which side declares `block` as a time dim. md_a omits `time_dimensions`/`frequencies`; md_b sets `time_dimensions: {"block"}` + `frequencies: {"monthly"}`. Expected: `EXPECT_THROW(..., std::runtime_error)`.

**Pattern to copy for D-23 #2 (Mirror B — lhs time + rhs non-time):** the existing `TimePropertiesMismatchThrows` body — but make it explicit by renaming and asserting the new error message wording. The current body covers this implicitly; D-23 #2 makes it a permanent regression backstop.

**Pattern to copy for D-23 #3 (WR-01 positive — parent-name match at different operand indices):** clone `AddTwoFiles` body but use a 3-dim metadata where the parent dim sits at different positions:
- md_a: `dimensions: {"month", "extra", "day"}`, `time_dimensions: {"month", "day"}`, `frequencies: {"monthly", "daily"}`. Parent of `day` is `month` at index 0.
- md_b: `dimensions: {"extra", "month", "day"}`, `time_dimensions: {"month", "day"}`, `frequencies: {"monthly", "daily"}`. Parent of `day` is `month` at index 1.

Both files' `day` dim has the SAME parent NAME (`month`), at different operand indices (0 vs 1). Expected: NO throw — `Expression e = Expression(a) + Expression(b)` succeeds; `e.save(path_out)` produces a readable file.

**Pattern to copy for D-23 #4 (WR-01 negative — parent-name mismatch):** clone `AddTwoFiles` body where:
- md_a: `dimensions: {"month", "day"}`, parent of `day` is `month`.
- md_b: `dimensions: {"week", "day"}`, parent of `day` is `week`.

Same dim name `day` but different parent NAMES. Expected: `EXPECT_THROW(..., std::runtime_error)`.

Per RESEARCH.md Pitfall 3, both metadatas must individually pass `BinaryMetadata::validate()` — the conflict only emerges when paired in `BinaryOpNode`. The `from_element` factory enforces per-side validity. The existing `TimePropertiesMismatchThrows` test confirms this construction shape works.

---

### `tests/test_expression.cpp` — D-25 (implicit conversion in operator argument)

**Analog:** `TEST_F(ExpressionFixture, IdentityFile)` at `tests/test_expression.cpp:102-117` (existing implicit-conversion-on-assignment test) + `TEST_F(ExpressionFixture, AddTwoFiles)` at lines 206-225 (existing two-file add round-trip).

**Existing `IdentityFile` test** demonstrating the assignment-form implicit conversion (lines 102-117):
```cpp
TEST_F(ExpressionFixture, IdentityFile) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));
    });

    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = a;  // implicit conversion (CORE-01) — STAND-ALONE assignment
    e.save(path_out);

    auto orig = read_all_cells(path_a);
    auto copy = read_all_cells(path_out);
    ASSERT_EQ(orig.size(), copy.size());
    for (size_t i = 0; i < orig.size(); ++i)
        EXPECT_DOUBLE_EQ(orig[i], copy[i]);
}
```

**Existing `Expression(a) + Expression(b)` call shape** (line 216) — this is the EXPLICIT version which D-25 must replace with implicit conversion:
```cpp
Expression e = Expression(a) + Expression(b);  // explicit
```

**Pattern to copy:** combine both existing patterns. Build two simple files via `write_qvr`, then write `Expression e = a + b;` with NO explicit `Expression(...)` wrapper around either operand. The compiler must implicitly convert both `BinaryFile&` operands inside the `operator+` call argument. Per RESEARCH.md "Test Pattern 8" — the load-bearing line is `Expression e = a + b;`. If a future reviewer adds `explicit` to `Expression(const BinaryFile&)`, this line fails to compile.

Verify the round-trip values matches `va[i] + vb[i]` per cell (mirrors the `AddTwoFiles` assertion shape).

---

### `tests/test_binary_file.cpp` — D-24 (CR-02 registry-leak regression)

**Analog:** `TEST_F(BinaryTempFileFixture, MoveAssignWriterUnregistersOldPath)` at lines 399-419 (the canonical "writer state then verify registry is clean" idiom) + `MovedWriterClearsRegistryOnDestruction` at lines 464-471 (minimal close-then-reopen verification).

**Existing template — `MovedWriterClearsRegistryOnDestruction`** (lines 464-471):
```cpp
TEST_F(BinaryTempFileFixture, MovedWriterClearsRegistryOnDestruction) {
    auto md = make_simple_metadata();
    {
        auto writer1 = BinaryFile::open_file(path, 'w', md);
        auto writer2 = std::move(writer1);
    }
    EXPECT_NO_THROW(BinaryFile::open_file(path, 'r'));
}
```

**Existing template — `MoveAssignWriterUnregistersOldPath`** (lines 399-419, fuller idiom):
```cpp
TEST_F(BinaryTempFileFixture, MoveAssignWriterUnregistersOldPath) {
    auto md = make_simple_metadata();
    auto path2 = path + "_2";
    {
        auto writer1 = BinaryFile::open_file(path, 'w', md);
        auto writer2 = BinaryFile::open_file(path2, 'w', md);
        writer2 = std::move(writer1);
        EXPECT_NO_THROW(BinaryFile::open_file(path2, 'r'));
        EXPECT_THROW(BinaryFile::open_file(path, 'r'), std::runtime_error);
    }
    EXPECT_NO_THROW(BinaryFile::open_file(path, 'r'));

    for (auto ext : {".qvr", ".toml"}) {
        auto full = path2 + ext;
        if (fs::exists(full))
            fs::remove(full);
    }
}
```

**Existing template — `WriterBlocksSecondWriter`** (lines 450-454, demonstrates the assertion shape for "second open succeeds because registry is clean"):
```cpp
TEST_F(BinaryTempFileFixture, WriterBlocksSecondWriter) {
    auto md = make_simple_metadata();
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(BinaryFile::open_file(path, 'w', md), std::runtime_error);
}
```

**Failure-trigger metadata** — the test must use a metadata that makes `to_toml()` throw via `validate()`. Per RESEARCH.md Pitfall 4, two viable triggers:
1. `bad_md.version = "999"` — fails the version check at `binary_metadata.cpp:370`.
2. `bad_md.labels.clear()` — fails the labels check at `binary_metadata.cpp:381`.

Existing relevant validate throw at `src/binary/binary_metadata.cpp:367-372`:
```cpp
void BinaryMetadata::validate() const {
    if (version != QUIVER_FILE_VERSION) {
        throw std::runtime_error("Incompatible file version: expected " + std::string(QUIVER_FILE_VERSION) + ", got " +
                                 version);
    }
    // ...
```

**Pattern to copy:** RESEARCH.md "Test Pattern 7 — D-24 CR-02 Regression" verbatim. Three-step shape:
1. Build `bad_md` from `make_simple_metadata()`, then mutate `version = "999"`.
2. `EXPECT_THROW(BinaryFile::open_file(path, 'w', bad_md), std::runtime_error)` — fails Phase 1 code path leaks the registry; passes after D-19.
3. `EXPECT_NO_THROW(...)` for a SECOND `open_file('w', good_md)` on the SAME path — only succeeds if the registry was correctly cleaned up.

Optional 4th step: actually use the writer + reopen as reader to confirm the file is fully functional.

---

## Shared Patterns

### Three-Pattern Error Messages
**Source:** `CLAUDE.md` "## Error Handling" section + Phase 1 D-11 (`expression.cpp:67`) + read paths in `binary_file.cpp:126, 165, 196`
**Apply to:** all 7 `BinaryOpNode` ctor throws in `nodes.cpp` (CR-03 + CR-01 mirror reject) and the WR-07 `apply_op` throw

```cpp
// Pattern 1 — Precondition failure ("Cannot {operation}: {reason}")
throw std::runtime_error("Cannot apply: dimension '" + l_dim.name + "' has incompatible sizes ...");
throw std::runtime_error("Cannot apply: time dimension '" + l_dim.name + "' has incompatible TimeProperties");
throw std::runtime_error("Cannot apply: unhandled BinaryOp variant");
```

The verb is short and operator-agnostic (matches `"Cannot save:"`, `"Cannot read:"`, `"Cannot open_file:"`).

### RAII Cleanup via `Impl::~Impl()` — Late-Insert Compatibility
**Source:** `src/binary/binary_file.cpp:37-44` (existing destructor)
**Apply to:** CR-02 fix — relies on this pattern, no new cleanup logic needed

The destructor erases `registered_path` from `write_registry` only when `registered_path` is non-empty. The CR-02 late-insert fix preserves this contract — `registered_path` becomes non-empty ONLY after all throwable operations succeed. Throw paths leave `registered_path` empty and the destructor becomes a no-op for the registry. No try/catch, no new state machine.

### Optional-Unwrap Iteration Loop
**Source:** `tests/test_expression.cpp:62-77` (`write_qvr` fixture) + `src/expr/expression.cpp:77-86` (Expression::save loop)
**Apply to:** `csv_converter.cpp:73-89` and `:120-135` (WR-04 collateral edits)

```cpp
auto nxt = quiver::binary::next_dimensions(meta, dims);
if (!nxt)
    break;
dims = std::move(*nxt);
```

Replaces the legacy "assign + equality compare to initial_dimensions" pattern. The free-function returns `std::optional` directly (no need to wrap in a delegate that converts `nullopt` back to `first_dimensions(meta)`).

### `find_dim_index(dims, name)` Lookup Helper
**Source:** `src/expr/nodes.cpp:64-70` (anonymous namespace helper)
**Apply to:** all CR-01 / WR-01 dimension matching in the new compatibility loop

Returns `int` index or `-1` if absent. Used consistently throughout `BinaryOpNode` ctor for cross-operand dim resolution. The CR-01 + WR-01 fix relies on this helper for both the same-name-pair iteration AND for the parent-by-name resolution.

### `from_element` Metadata Construction in Tests
**Source:** `tests/test_expression.cpp:49-57` (`make_simple_metadata`) and `tests/test_binary_file.cpp:31-51` (both `make_simple_metadata` and `make_time_metadata`)
**Apply to:** all D-23, D-24, D-25 new tests

```cpp
BinaryMetadata::from_element(Element()
                                 .set("version", "1")
                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                 .set("unit", "MW")
                                 .set("dimensions", {"name1", "name2"})
                                 .set("dimension_sizes", {N1, N2})
                                 .set("time_dimensions", {"name2"})       // optional
                                 .set("frequencies", {"monthly"})         // matches time_dimensions length
                                 .set("labels", {"v1", "v2"}));
```

Each new test in D-23/D-24/D-25 follows this construction. D-23 #3 (WR-01 positive parent-name match at different indices) requires a 3-dim variant; the others fit the 2-dim shape.

### Programmatic .qvr Population in Tests
**Source:** `tests/test_expression.cpp:62-77` (`write_qvr` fixture helper)
**Apply to:** all D-23, D-25 tests requiring readable `.qvr` files. (D-24 does NOT call `write_qvr` because the bad_md path does not survive past the `to_toml()` failure point.)

```cpp
write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
```

Reuses the fixture's `write_qvr` lambda. Tests asserting THROW need only constant fill; tests asserting round-trip values (D-23 #3, D-25) use `[](dims, k) { return ...; }` per the `AddTwoFiles` template.

### `EXPECT_THROW` for Validation Failures
**Source:** `tests/test_expression.cpp:332, 361, 387` and `tests/test_binary_file.cpp:74, 78, 83`
**Apply to:** D-23 #1, #2, #4 (negative cases) and D-24 first stage

```cpp
EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
EXPECT_THROW(BinaryFile::open_file(path, 'w', bad_md), std::runtime_error);
```

Project preference: `std::runtime_error` for all D-18 `"Cannot apply: ..."` throws and for the CR-02 metadata-validation throws (which originate from `BinaryMetadata::validate() → throw std::runtime_error("Incompatible file version: ...")`).

---

## No Analog Found

(none — every modification has a clear in-tree pattern to copy)

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| — | — | — | All 9 modifications either operate inside files with their own established patterns, or duplicate test idioms already present in the same fixture. The phase is pure refactoring + test additions against existing, working machinery. |

---

## Cross-File Coupling (planner must coordinate)

### WR-04 mandates simultaneous edits to `binary_file.h` + `binary_file.cpp` + `csv_converter.cpp`

Per RESEARCH.md Pitfall 1, a planner reading just CONTEXT.md could miss the csv_converter dependency. The WR-04 commit MUST include all three edits:

1. `include/quiver/binary/binary_file.h` — delete protected declarations at lines 76-77
2. `src/binary/binary_file.cpp` — delete the `BinaryFile::next_dimensions` thin delegate (381-392) and `BinaryFile::dimension_sizes_at_values` (394-464)
3. `src/binary/csv_converter.cpp` — re-point lines 85 and 130 to call `quiver::binary::next_dimensions` directly with optional unwrap

Compile failure in csv_converter.cpp is the canary if step 3 is missed. Verify with `quiver_tests --gtest_filter="CSVConverter*"` (37 tests).

### WR-03 + WR-04 ordering choice

Per RESEARCH.md Pitfall 7, two valid commit splits:
- **Strategy 1 (recommended):** WR-03 deletes the duplicate `dimension_sizes_at_values` body in `binary_file.cpp` only; WR-04 then deletes the thin `next_dimensions` delegate, header declarations, and updates `csv_converter.cpp`. Two meaningful commits.
- **Strategy 2:** Merge into a single commit titled "WR-04 (subsumes WR-03)". Reviewer-friendly but loses per-audit-ID granularity.

CONTEXT.md D-26 implies audit-order commits (Strategy 1). The planner should make this choice explicit in the plan's commit table.

### CR-01 + WR-01 land as a SINGLE commit (per D-26)

Both touch the same compatibility loop in `BinaryOpNode` ctor (`nodes.cpp:132-151`). D-16 says "rewrite the time-dim compatibility loop ... iterate the union of same-name dims across both operands." Splitting into two commits would either leave the code in a half-fixed state mid-commit or duplicate the loop edit. Single commit is the only sensible choice.

---

## Metadata

**Analog search scope:** `src/expr/`, `src/binary/`, `include/quiver/expr/`, `include/quiver/binary/`, `tests/test_expression.cpp`, `tests/test_binary_file.cpp`
**Files scanned (full read):** 8 (`src/expr/nodes.cpp`, `include/quiver/expr/node.h`, `src/binary/binary_file.cpp`, `include/quiver/binary/binary_file.h`, `src/binary/iteration.cpp`, `include/quiver/binary/iteration.h`, `include/quiver/expr/expression.h`, `tests/test_expression.cpp` head + targeted, `tests/test_binary_file.cpp` head + targeted)
**Files scanned (targeted ranges):** 3 (`src/binary/csv_converter.cpp:1-160`, `src/binary/binary_metadata.cpp:325-380`, `include/quiver/binary/csv_converter.h` confirmation grep)
**Pattern extraction date:** 2026-05-06

---

*Phase: 2-Tech Debt*
*Mapper: Claude (gsd-pattern-mapper)*
