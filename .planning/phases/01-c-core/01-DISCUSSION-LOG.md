# Phase 1: C++ Core - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in `01-CONTEXT.md` — this log preserves the alternatives considered.

**Date:** 2026-05-06
**Phase:** 1-C++ Core
**Areas discussed:** Shape compatibility, Output metadata, FileNode caching, BinaryFile extensions

---

## Shape Compatibility

### Q1 — Compatibility model

| Option | Description | Selected |
|--------|-------------|----------|
| Strict equality (Recommended) | `lhs.metadata() == rhs.metadata()` in full — same dimensions (count, names, sizes), same labels, same unit, same time properties. Rejects everything else. Matches DESIGN.md note that 'strict is simpler'. | |
| Sizes + dim names match | Dimensions match by count, names, and sizes; labels and unit may differ. Lets users combine files with different value labels but identical shape. | |
| Sizes only (NumPy-flavored) | Just dim count + sizes match. Names/labels/units ignored. Most permissive, easy to misuse. | |
| **Other (free text)** | "It should be the union of the dimensions. and if dimensions match those size should: n×n, 1×n, n×1. if is n×m it should raise an error." | ✓ |

**User's choice:** broadcasting with named-dim union; same-name dims must satisfy n×n / 1×n / n×1; otherwise error.
**Notes:** Significant departure from DESIGN.md "strict" assumption. Drove subsequent questions on output metadata, broadcasting behavior of labels axis, and operand dim ordering.

### Q2 — Output dim order

| Option | Description | Selected |
|--------|-------------|----------|
| lhs order, then rhs-only appended | Output = lhs.dims with rhs-only dims appended in rhs order. Predictable, asymmetric. | ✓ |
| Reject mismatched ordering | If dims overlap, they must appear in the same relative order in both operands. | |
| Keep order from first appearance, lhs first | Walk lhs.dims, then walk rhs.dims and append any not seen yet. | |

**User's choice:** lhs order, then rhs-only appended.
**Notes:** Confirmed asymmetric ordering — `a + b` and `b + a` may produce different output dim orderings while values match.

### Q3 — Validation timing

| Option | Description | Selected |
|--------|-------------|----------|
| At BinaryOpNode construction (Recommended) | Check when the user writes a+b. Throws right at the operator call. | ✓ |
| At save() / first row | Tree builds without checking; mismatch surfaces only when save() iterates. | |
| Both — cheap check at construction, full check during save | Construction does cheap shape check. Save does full validation incl. time-dimension consistency. | |

**User's choice:** at BinaryOpNode construction.

### Q4 — Time-dim compatibility

| Option | Description | Selected |
|--------|-------------|----------|
| TimeProperties must match exactly | Same frequency, initial_value, parent_dimension_index. Strictest. | ✓ |
| Frequency must match; initial_value/parent can differ | Frequency is structural; initial offset is bookkeeping. | |
| Time-dim matches like any other dim (sizes only) | Treat time dims as plain dims for compat. | |

**User's choice:** TimeProperties must match exactly.

### Q5 — Operand dim ordering

| Option | Description | Selected |
|--------|-------------|----------|
| Order can differ — match by name only (Recommended) | a=[time, scenario] and b=[scenario, time] are compatible. compute_row translates indices per operand. | ✓ |
| Order must match for shared dims | If both have 'time' and 'scenario', they must appear in the same relative order in both operands. | |
| Output order = lhs order; rhs must align to lhs | rhs must literally have the shared dims in lhs's order. | |

**User's choice:** order can differ; match by name only.

### Q6 — Shape error message detail

| Option | Description | Selected |
|--------|-------------|----------|
| Pinpoint dim + sizes (Recommended) | `"Cannot apply binary operation: dimension 'scenario' has incompatible sizes 5 vs 7 (broadcasting requires n×n, 1×n, or n×1)"`. | ✓ |
| Full metadata diff | Print both lhs and rhs full metadata on failure. | |
| Short generic | `"Cannot apply binary operation: incompatible shapes"`. | |

**User's choice:** pinpoint dim + sizes.

---

## Output Metadata

### Q1 — `unit` derivation

| Option | Description | Selected |
|--------|-------------|----------|
| Inherit lhs.unit silently (Recommended) | Output unit = lhs.unit, no check. | |
| Error if units differ | Throws at construction if a.unit != b.unit. Dimensional safety. | ✓ |
| Empty if units differ; lhs if equal | If a.unit == b.unit, keep it. Otherwise empty string. | |

**User's choice:** error if units differ.
**Notes:** Strict dimensional safety, consistent with the strict-by-default tone of D-01.

### Q2 — `labels` derivation

| Option | Description | Selected |
|--------|-------------|----------|
| Error if labels differ (consistent with unit) | Element-wise equality required. | |
| Inherit lhs.labels silently | Output = lhs.labels, no content check. | |
| Match lengths; error if different non-empty values | Element-wise with empty strings as wildcards. | |
| **Other (free text)** | "if labels_a .== labels_b: ok; elseif size(labels_a) > 1 and size(labels_b) == 1: ok; elseif size(labels_a) == 1 and size(labels_b) > 1: ok; else error" | ✓ |

**User's choice:** broadcast labels like a trailing axis with the same n×n / 1×n / n×1 rule.
**Notes:** Significant — labels.size() becomes effectively a hidden broadcastable trailing axis. The save engine must broadcast the size-1 side's value across the n-side's row when computing each row.

### Q3 — `initial_datetime` derivation

| Option | Description | Selected |
|--------|-------------|----------|
| Must match if both have time dims; otherwise inherit the time-bearing side | Strict when both time-bearing; inherit when one-sided; unused when neither. | ✓ |
| Always inherit lhs.initial_datetime silently | No check. | |
| Inherit from time-bearing side; if both, take the earlier | Permissive. | |

**User's choice:** must match if both have time dims; otherwise inherit the time-bearing side.

---

## FileNode Caching

### Q1 — Multiple FileNodes referring to the same file

| Option | Description | Selected |
|--------|-------------|----------|
| No caching — each FileNode opens independently (Recommended) | FileNode owns its own unique_ptr<BinaryFile>, lazily opened. a+a opens twice. Simple ownership. | ✓ |
| Cache by canonical path within a single Expression tree | Dedupe FileNodes via shared_ptr<BinaryFile>. | |
| Cache by canonical path globally (per-process) | Static map of canonical path → weak_ptr<BinaryFile>. | |

**User's choice:** no caching, each FileNode opens independently.

### Q2 — save() to a path matching an input FileNode

| Option | Description | Selected |
|--------|-------------|----------|
| Eager check at save() entry (Recommended) | Walk tree, collect FileNode paths, throw early with dedicated message before opening writer. | ✓ |
| Let the write registry block it lazily | save() opens writer first, FileNode read open then bubbles registry's error mid-iteration. | |
| Allow it via temp-file swap | Detect collision, write to temp file, swap into place after writer closes. | |

**User's choice:** eager check at save() entry.

---

## BinaryFile Extensions

### Q1 — Where row iteration lives

| Option | Description | Selected |
|--------|-------------|----------|
| Free functions on BinaryMetadata (Recommended) | quiver::binary::first_dimensions(metadata), quiver::binary::next_dimensions(metadata, current). Pure metadata-driven. | ✓ |
| Promote BinaryFile::next_dimensions to public; add public BinaryFile::first_dimensions | Engine needs a BinaryFile instance. | |
| Member methods on BinaryMetadata | Adds behavior to a Rule-of-Zero value type. | |

**User's choice:** free functions in `quiver::binary` namespace.

### Q2 — Fast read overloads

| Option | Description | Selected |
|--------|-------------|----------|
| Add both fast overloads now (Recommended) | BinaryFile::read(vector<int64_t>) returning vector<double> AND read_into(vector<int64_t>, vector<double>&) for buffer reuse. | ✓ |
| Add only the vector overload, no read_into | ~40% win, but per-row allocation cost (~3%) remains. | |
| Use the existing unordered_map API; defer perf to a later phase | Phase 1 stays narrow; engine builds map per call. | |

**User's choice:** add both fast overloads (read + read_into).

### Q3 — Validation in the new fast read overloads

| Option | Description | Selected |
|--------|-------------|----------|
| read(vector) validates; read_into skips validation (Recommended) | Vector-returning overload is general-purpose (validates). read_into is the trusted-caller fast path (no validation). Engine uses read_into. | ✓ |
| Both new overloads validate | Same validation as existing path. Engine pays the ~19% cost. | |
| Both new overloads skip validation | Maximum throughput; misuse from outside the engine can hit out-of-bounds reads silently. | |

**User's choice:** read(vector) validates; read_into skips validation.

### Q4 — Fast write overload

| Option | Description | Selected |
|--------|-------------|----------|
| Add fast write(data, vector<int64_t> dims) too (Recommended) | Symmetric with read; validates dims. | ✓ |
| Add fast write AND a no-validate write_unchecked variant | Mirrors read/read_into; engine uses write_unchecked. | |
| Skip — leave write on the unordered_map API | Asymmetric but tighter Phase 1. | |

**User's choice:** add fast write(data, vector<int64_t>) overload.

---

## Claude's Discretion

Items the user did not explicitly decide; planning/execution may resolve:

- Public/internal split for `Node` and its subclasses (DESIGN.md sketches public; will follow that unless planning surfaces a reason to keep them internal).
- Test fixture strategy (programmatic `.qvr` generation in `SetUp()` vs committed fixtures under `tests/fixtures/expr/`).
- Exact wording of error messages beyond the structures decided in D-06 / D-07 / D-09 / D-11.
- Coexistence vs deprecation of the existing `unordered_map`-based `read`/`write` overloads on `BinaryFile`.
- Whether the protected `BinaryFile::next_dimensions` member is removed in favor of the new free function or kept as a thin delegate.
- Header organization for the new free functions (co-locate in `binary_metadata.h` vs new `iteration.h`).

## Deferred Ideas

None — discussion stayed within phase scope. v1 / v2 feature scoping is already locked in `REQUIREMENTS.md` and `PROJECT.md`'s "Out of Scope" section.
