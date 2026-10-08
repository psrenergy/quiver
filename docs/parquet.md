# Parquet snapshots

Quiver's binary format remains the working format: write coordinates in any order, close the
writer, then explicitly export a snapshot. Re-export after changing the binary data.

```cpp
#include <quiver/binary/parquet.h>

quiver::bin_to_parquet("results");
```

This reads `results.qvr` and `results.toml` and writes `results.parquet`. Julia exposes
`Quiver.Binary.bin_to_parquet("results")`, C exposes `quiver_bin_to_parquet("results")`,
and Lua exposes `db:bin_to_parquet("results")`. Lua resolves paths against the database's
directory, checks the actual input/output files against its sandbox, and rejects in-memory
databases. Other bindings can use the Lua operation through `Sandbox`.

The writer must be closed. An existing snapshot is replaced only after the new file's footer
and output stream have been successfully closed. Failure leaves the previous snapshot intact;
this is not a guarantee of durability through a power failure. Binary inputs are never modified.
Do not modify the inputs or concurrently export to the same destination during conversion.
On Windows, close readers of an existing Parquet snapshot before replacing it.

Each valid binary coordinate becomes one row in the existing traversal order. Columns are:

1. Original dimension names, ordered as in metadata, containing 1-based non-null int64 values.
2. `datetime`, when time dimensions exist: the calendar cell's start, as a UTC microsecond timestamp.
3. Original agent labels, ordered as in metadata, containing nullable float64 values.

NaN is binary's missing-value marker and becomes Parquet NULL. Infinite values remain infinite.
Invalid calendar cells and coordinates preceding the initial coordinate are skipped by Quiver's usual
traversal. Conflicting column names, including a generated `datetime` collision, raise an error.
Names are never silently changed.

The footer's `quiver:metadata` value contains the complete canonical TOML metadata; the
`quiver:parquet_version` value is `1`. A Parquet reader needs no adjacent TOML file. For example:

```python
import pyarrow.parquet as parquet

table = parquet.read_table("results.parquet", columns=["datetime", "plant_1"])
```

Exports use Zstandard level 3. Batches contain at most 65,536 rows, reduced for wide schemas
to target approximately 32 MiB of uncompressed column data (at least one row). Encoders, schema
and row-group footer metadata add overhead, so this is not a strict process-memory limit.
Conversion requires a full traversal and storage for both formats. There is no Parquet importer
or automatic export on save/close.

## Local measurement

A Windows x64 Release run with 32 agents and 10% missing cells produced these results.
Peak memory covers the whole benchmark process, including binary-file creation; timings are
illustrative measurements under concurrent build load, not performance guarantees.

| Rows | Binary MB | Parquet MB | Export seconds | Peak working set MB |
| --- | ---: | ---: | ---: | ---: |
| 100,000 | 25.60 | 8.44 | 0.729 | 35.14 |
| 400,000 | 102.40 | 33.41 | 2.300 | 35.73 |

Run `quiver_benchmark --parquet <rows>` to repeat the workload. The native Release core DLL
grew from approximately 4.39 MB to 11.97 MB with the statically linked Parquet dependency.
Fresh builds also compile Arrow and its bundled dependencies; incremental builds reuse them.
