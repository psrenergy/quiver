# Phase 4: Performance Benchmark - Context

**Gathered:** 2026-02-21
**Status:** Ready for planning

<domain>
## Phase Boundary

C++ benchmark proving that explicit transactions improve write throughput. Creates multiple elements with scalars, vectors, and time series data, comparing all-individual vs all-batched-in-one-transaction performance on a file-based database. Results printed to stdout.

</domain>

<decisions>
## Implementation Decisions

### Benchmark workload
- 5000 elements per run
- Each element has label + 2-3 scalar attributes + vector data + time series data
- 10 time series rows per element with realistic-looking timestamps (plausible date_time values, e.g., hourly intervals)
- Two variants only: all-individual (no explicit transaction) vs all-batched (single wrapping transaction)
- Reuse an existing test schema from tests/schemas/valid/
- Temporary file-based database (created and deleted per benchmark run)

### Output presentation
- Clean ASCII table with columns: variant name, total time (ms), per-element time, ops/sec, speedup ratio
- Header banner before results showing benchmark parameters (element count, time series rows per element, schema used, iteration count)
- All four metrics shown: total time (ms), per-element time, ops/sec, speedup ratio

### Statistical rigor
- 3-5 iterations per variant
- Report statistics across iterations (not just single run)

### Test integration
- Separate executable (e.g., quiver_benchmark.exe) — not part of the normal test suite
- Source lives in tests/benchmark/
- build-all.bat builds the benchmark but test-all.bat does NOT run it — manual invocation only

### Claude's Discretion
- Baseline variant structure (how individual operations are grouped without explicit transaction)
- Primary statistic (median vs mean) — pick most appropriate for benchmark reporting
- Whether to include a warm-up run discarded from results
- Whether each iteration uses a fresh DB or reuses with cleanup
- Whether to use Catch2 BENCHMARK or plain main() — pick simplest approach
- Progress output during execution (e.g., "Running: individual..." before results)
- Exact number of iterations within the 3-5 range

</decisions>

<specifics>
## Specific Ideas

- Time series timestamps should look realistic (hourly intervals like "2024-01-01T00:00:00", "2024-01-01T01:00:00", etc.), not abstract sequential values
- Include vector attribute writes per element in addition to scalars + time series to show broader transaction benefit
- 5000 elements chosen for dramatic, definitive speedup demonstration

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-performance-benchmark*
*Context gathered: 2026-02-21*
