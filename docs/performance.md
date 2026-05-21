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

Latest verified Plan 6 results on this machine:

| Case | Result |
| --- | ---: |
| assets_10k | 190.74 MiB/s |
| assets_50k | 167.35 MiB/s |
| asset_database_5k | 225.87 MiB/s |
| small_files_1k | 173.54 MiB/s |
| strings_plain_5k | 446.10 MiB/s |
| strings_escaped_5k | 274.29 MiB/s |
| deep_1k | 176.89 MiB/s |
| wide_10k | 240.74 MiB/s |
| query_assets_10k | 14.92M queries/s |
| query_first_10k | 11.45M queries/s |
| query_last_10k | 7.47M queries/s |
| query_missing_10k | 9.65M queries/s |
| query_string_view_10k | 13.25M queries/s |
| query_many_keys_last | 3.47M queries/s |

Plan 6 yyjson comparison results:

| Equivalent case | gsexp | yyjson | yyjson/gsexp |
| --- | ---: | ---: | ---: |
| assets_10k parse | 190.74 MiB/s | 659.84 MiB/s | 3.46x |
| assets_50k parse | 167.35 MiB/s | 632.48 MiB/s | 3.78x |
| asset_database_5k parse | 225.87 MiB/s | 743.61 MiB/s | 3.29x |
| small_files_1k parse | 173.54 MiB/s | 553.14 MiB/s | 3.19x |
| strings_plain_5k parse | 446.10 MiB/s | 1447.86 MiB/s | 3.25x |
| strings_escaped_5k parse | 274.29 MiB/s | 1294.42 MiB/s | 4.72x |
| wide_10k parse | 240.74 MiB/s | 836.61 MiB/s | 3.48x |
| assets_10k lookup | 14.92M queries/s | 63.50M queries/s | 4.26x |
| many_keys_last lookup | 3.47M queries/s | 7.70M queries/s | 2.22x |

These are equivalent data shapes, not byte-identical files. The JSON fixtures
are generated beside the S-expression fixtures and measured by each format's
own source byte size. `asset_database_5k` is the first real-ish generated asset
database case: mixed record types, nested fields, optional fields, repeated
keys, and comments on the S-expression side. yyjson is fetched only for
benchmark builds.

Benchmark context from the latest run:

```text
benchmark_context cpu_model="Intel(R) Core(TM) i5-3320M CPU @ 2.60GHz" sse4_2=not_compiled avx2=not_compiled yyjson=enabled
```

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

Important Plan 6 retained changes:

1. Benchmark output now records CPU model, compiled SIMD context, and yyjson
   availability.
2. yyjson comparison fixtures are generated beside the S-expression fixtures.
3. A real-ish mixed asset database fixture is included for both formats.
4. Node reservation now uses a bounded density sample only when it is clearly
   lower than the old `source.size() / 4` reserve. Dense and small inputs keep
   the old reserve.
5. A benchmark-only delimiter scan probe compares scalar scanning against an
   SSE2 chunked scan. It is not part of the parser API or parser code path.

Plan 6 optimization attempt results:

| Attempt | Result |
| --- | --- |
| Density-aware node reserve, first version | Rejected. It reduced memory on string-heavy files, but changed dense/small/deep reserves too aggressively and hurt `small_files_1k` and `deep_1k`. |
| Density-aware node reserve, gated version | Kept. `strings_plain_5k` retained storage dropped from about 10.1 MiB to 3.0 MiB and `strings_escaped_5k` dropped from about 12.3 MiB to 4.3 MiB. Dense and small inputs keep the old reserve. |
| Benchmark-only SSE2 delimiter scan probe | Measured, not integrated. On `asset_database_5k`, scalar delimiter counting reached 863.18 MiB/s and SSE2 reached 1696.90 MiB/s, a 1.97x raw scan speedup. Parser integration still needs separate proof because parsing also maintains structure, diagnostics, and line/column state. |

## Optimization Plan 7

Goal: close the measured parse/query gap against yyjson without adding a
second public parser API and without making the implementation hard to audit.
Plan 7 should focus on the parser success path and query helpers. Comparison
against yyjson remains useful, but new work should primarily improve `gsexp`
itself.

Baseline gaps from Plan 6:

1. Parse throughput is usually 3.2x to 4.7x behind yyjson on equivalent DOM/tree
   workloads.
2. Lookup throughput is 2.2x to 4.3x behind yyjson.
3. A raw SSE2 delimiter scan is about 2x faster than scalar delimiter counting,
   but it is not yet integrated into parsing.

Work order:

1. Success-path position tracking.
   Today the parser updates line/column during ordinary successful parsing.
   Try storing only byte offsets during parse and computing line/column only
   when an error diagnostic is emitted. Keep exact diagnostics in tests. This
   should be the first attempt because it may reduce branchy per-character work
   without SIMD or API changes.

2. Faster atom and string scanning.
   Integrate word-at-a-time or SSE2 delimiter search into atom scanning first.
   Then try string scanning for quote/backslash/newline. Keep scalar fallback.
   Do not introduce a public `parse_simd` or required SIMD baseline.

3. Query helper fast paths.
   Add internal helpers that return the value child directly for `(key value)`
   pairs so `extract_int`, `extract_float`, and string helpers do less repeated
   `Node` construction and `child_count` work. Preserve the public helper API.

4. Child index representation.
   The current lazy index stores a vector of key/child entries inside an
   unordered_map keyed by parent node. Measure whether a simpler per-parse side
   array, sorted vector, or compact open-addressed table helps the wide-key
   lookup cases without increasing parse cost.

5. Node append hot path.
   Measure alternatives to pushing both `NodeData` and `last_children` for every
   node. Do not reintroduce retained parent pointers unless benchmarks prove the
   memory/speed tradeoff is worth it.

6. Decode arena sizing.
   Revisit escaped string storage after parser scanning changes. Try block-based
   decoded storage only if escaped-heavy benchmarks show either speed or memory
   pressure that justifies it.

External code review:

1. Inspect yyjson selectively for allocation, scanner, and lookup techniques.
   Do not copy large code or change license surface.
2. Optionally inspect one small S-expression/data parser for representation
   ideas, but do not optimize for Lisp evaluation or cons-cell semantics.
3. Avoid cloning many libraries until a local bottleneck is unclear. Current
   bottlenecks are clear enough to try local changes first.

Acceptance rules:

1. Each attempt gets before/after benchmark output and a keep/reject note.
2. `gsexp ./scripts/build.sh` passes.
3. `gsexp ./scripts/bench.sh` passes with yyjson enabled.
4. Benchmark build with `GSEXP_BENCHMARK_YYJSON=OFF` still works.
5. `glayout ./scripts/build.sh` passes after any public API or vendoring change.
6. Public API stays stable unless the README and glayout are updated in the same
   change.
7. SIMD changes must compile out cleanly on non-x86 or non-SIMD builds.
8. Keep files roughly within the existing size discipline; split benchmark or
   parser helpers by responsibility if a file grows clearly past the guideline.

Rejected-by-default unless evidence changes:

1. A separate fast parser API.
   Users should still call `parse` or `parse_owned`.
2. Removing useful diagnostics for speed.
   Exact diagnostics can become lazy, but error quality should not regress.
3. Rewriting the parser around a complex generator or table-driven framework.
   The project goal is still low-abstraction, easy-to-debug C++.
4. Chasing yyjson validation-only numbers.
   The target remains retained tree construction and practical lookup speed.

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
