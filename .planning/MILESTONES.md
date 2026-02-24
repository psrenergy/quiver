# Milestones

## v0.3 (Pre-GSD)

Established before GSD initialization. Core library, C API, Julia/Dart/Lua bindings, CSV export, transactions, benchmarking.

**Phases:** Pre-GSD (no phase tracking)

## v0.4 Python Bindings & Test Parity (Shipped: 2026-02-24)

**Delivered:** Full Python CFFI binding package (quiverdb) wrapping the entire C API, plus cross-layer test parity across all 6 language layers.

**Phases:** 1-7 (15 plans, 62 commits, 79 min execution)
**Files:** 126 modified (+16,451 / -1,502)
**Python LOC:** 5,098
**Tests:** 1,849 across C++, C API, Julia, Dart, Python, Lua (all pass)
**Timeline:** 2 days (2026-02-23 to 2026-02-24)
**Git range:** bc3558f → 1a87d11

**Key accomplishments:**
1. Full Python CFFI binding package (quiverdb) wrapping the entire C API surface (49 requirement areas)
2. Type-safe read/write/query/time-series/CSV operations across the Python FFI boundary
3. Transaction context manager with BaseException-safe commit/rollback
4. Multi-column void** time series marshaling with columnar type dispatch
5. Cross-layer test parity audit: 1,849 tests across 6 language layers
6. Shared all_types.sql schema enabling consistent testing across all layers

**Known tech debt (from audit):**
- Generator script references non-existent header paths (INT-01)
- Package name documentation mismatch: docs say "quiver", actual is "quiverdb" (INT-02)
- import_csv signature missing group parameter for forward-compatibility (INT-03)
- read_all_vectors/sets_by_id known limitation with multi-column group names

---
