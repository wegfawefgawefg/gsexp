# gsexp Performance Notes

Performance work should be measured before it is kept.

Process:

1. Run `./scripts/bench.sh` before the change.
2. Make one optimization attempt.
3. Run `./scripts/build.sh`.
4. Run `./scripts/bench.sh` after the change.
5. Keep parser-local changes only if they improve the benchmark without making
   the implementation harder to audit.
6. Public model changes are allowed only when they are deliberately chosen for
   usability or correctness, then benchmarked and documented.
7. Record the result here.

The benchmark generates S-expression and JSON records in memory and parses them
repeatedly. It reports the best of three runs for each case. It is intended to
catch parser-level improvements, not full application startup behavior.

Older optimization plans and detailed results are archived in
[performance-history.md](performance-history.md).

## Current Results

Latest verified Plan 6 baseline results on this machine:

| Case | Result |
| --- | ---: |
| assets_10k | 196.44 MiB/s |
| assets_50k | 164.50 MiB/s |
| asset_database_5k | 226.71 MiB/s |
| small_files_1k | 169.64 MiB/s |
| strings_plain_5k | 434.98 MiB/s |
| strings_escaped_5k | 272.33 MiB/s |
| deep_1k | 177.27 MiB/s |
| wide_10k | 251.18 MiB/s |
| query_assets_10k | 14.05M queries/s |
| query_first_10k | 11.69M queries/s |
| query_last_10k | 7.55M queries/s |
| query_missing_10k | 9.68M queries/s |
| query_string_view_10k | 13.25M queries/s |
| query_many_keys_last | 3.77M queries/s |

Plan 6 yyjson comparison results:

| Equivalent case | gsexp | yyjson | yyjson/gsexp |
| --- | ---: | ---: | ---: |
| assets_10k parse | 196.44 MiB/s | 642.10 MiB/s | 3.27x |
| assets_50k parse | 164.50 MiB/s | 647.15 MiB/s | 3.93x |
| asset_database_5k parse | 226.71 MiB/s | 759.81 MiB/s | 3.35x |
| small_files_1k parse | 169.64 MiB/s | 567.68 MiB/s | 3.35x |
| strings_plain_5k parse | 434.98 MiB/s | 1350.91 MiB/s | 3.11x |
| strings_escaped_5k parse | 272.33 MiB/s | 1141.83 MiB/s | 4.19x |
| wide_10k parse | 251.18 MiB/s | 800.63 MiB/s | 3.19x |
| assets_10k lookup | 14.05M queries/s | 61.12M queries/s | 4.35x |
| many_keys_last lookup | 3.77M queries/s | 7.75M queries/s | 2.05x |

These are equivalent data shapes, not byte-identical files. The JSON fixtures
are generated beside the S-expression fixtures and measured by each format's
own source byte size. `asset_database_5k` is the first real-ish generated asset
database case: mixed record types, nested fields, optional fields, repeated
keys, and comments on the S-expression side. yyjson is fetched only for
benchmark builds.

Important Plan 5 retained changes:

1. Flat node storage remains the public parser representation.
2. `parse_owned(std::string)` moves caller-owned source into the parse result.
3. `extract_string_view` avoids string copies when callers respect
   `ParseResult` lifetime.
4. `storage_stats()` reports retained parser storage for benchmarks and
   diagnostics.
5. Node storage reserves `source.size() / 4` to avoid growth churn.
6. `find_child` uses direct node storage scans and lazily indexes wide lists.
7. Retained node fields are packed; `parent` and retained `last_child` were
   removed from `NodeData`.
8. Decoded escaped strings live in one parse-owned byte arena.

## Optimization Plan 6

Goal: compare `gsexp` honestly against optimized JSON parsers and investigate
SIMD/scanner work without making the normal API harder to use. This machine is
currently an Intel Core i5-3320M with SSE4.2, AVX, POPCNT, and no AVX2, so Plan
6 should target portable scalar improvements first and SSE4.2/AVX-era SIMD only
when the fallback path stays simple.

Measurement work:

1. Add local JSON comparison benchmarks.
   Generate equivalent asset data as JSON and parse it with at least one
   optimized C/C++ JSON parser. Prefer `yyjson` first because it has a simple C
   DOM API and can be vendored for benchmarks only. Add `simdjson` only if it
   builds cleanly on this old CPU and benchmark setup.

2. Keep comparison modes honest.
   Compare like with like:
   - parse and materialize a navigable DOM/tree
   - parse and run equivalent key lookups
   - parse in-memory source, not disk I/O, unless the case is explicitly a file
     loading benchmark

3. Add generated JSON equivalents for current S-expression cases.
   Cover asset records, string-heavy data, escaped string data, wide records,
   and small files. Keep the data generators next to the existing benchmark
   generators so shapes stay comparable.

4. Record CPU feature context.
   Benchmark output should include or document the CPU family/features used for
   the run. SIMD results are not portable without this context.

5. Add an optional real-ish asset database benchmark.
   Synthetic records are useful, but future optimization should be guided by a
   closer approximation of the intended asset database: nested records, comments,
   path-like strings, repeated keys, missing optional fields, and mixed record
   widths.

Candidate options:

1. SIMD delimiter scan prototype.
   Try scanning for `(`, `)`, `"`, whitespace, `;`, and `#` in wider chunks.
   Keep a scalar fallback. Do not wire SIMD throughout the parser until a
   benchmark-only prototype shows a meaningful win on this CPU.

2. SSE4.2 string scanning.
   The CPU supports SSE4.2 but not AVX2. Try `_mm_cmpestri` or a simple SSE byte
   mask path for finding quote/backslash/newline in unescaped strings. Keep only
   if `strings_plain_5k`, `strings_escaped_5k`, and asset cases improve.

3. Faster atom scan.
   Atoms are delimiter-terminated. Try word-at-a-time or SIMD delimiter search
   for atom ends. Keep only if asset and wide cases improve without hurting
   diagnostics.

4. Safer decoded arena sizing.
   The Plan 5 decoded arena reserves `source.size()` on first escaped string.
   Try block-based decoded storage or a smaller conservative reserve strategy.
   Keep only if escaped-heavy speed remains good and memory drops materially.

5. Real file-load `parse_owned` benchmark.
   Add a benchmark that reads generated data into a `std::string` and calls
   `parse_owned(std::move(text))`. This should measure real ownership flow
   better than copying the same benchmark string every iteration.

6. Density-aware node reserve heuristic.
   `source.size() / 4` is fast but over-reserves long string-heavy data. Try a
   cheap sampler that estimates node density from the first N KiB rather than a
   full pre-scan. Keep only if memory improves without losing the Plan 5 parse
   wins.

7. Lazy index threshold tuning on realistic data.
   Threshold 16 is good for current synthetic data. Retune only after JSON and
   real-ish asset benchmarks exist.

8. Explicit benchmark-only parser probes.
   It is acceptable to add small benchmark-only prototypes for SIMD scans or
   reserve heuristics. Do not expose them as public parser APIs and remove or
   integrate them after measurement.

Rejected-by-default unless evidence changes:

1. A second public parser such as `parse_simd`.
   SIMD should be an internal implementation detail selected at compile time or
   runtime, not a user-facing split.

2. Required SIMD baseline.
   The library should still build and run on machines without SSE4.2/AVX. Any
   SIMD path must have a scalar fallback.

3. Chasing simdjson raw validation numbers.
   `gsexp` builds retained node storage and diagnostics. A fair comparison is
   DOM/tree materialization and useful lookups, not JSON validation only.

4. Broad parser rewrite before benchmark evidence.
   SIMD scanning can easily make the code harder to debug. Prototype first,
   integrate second.

Acceptance rules:

1. `gsexp` build/tests pass.
2. `glayout` build/tests pass after any public or vendoring change.
3. Existing Plan 5 benchmark cases still run.
4. JSON comparison benchmarks run locally or are explicitly documented as not
   available because a dependency could not be built.
5. Every kept optimization has before/after results and a documented rejection
   record for failed attempts.
6. Public API remains the same unless a change is deliberately documented in the
   README.
