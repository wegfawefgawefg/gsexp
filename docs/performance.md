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

Latest verified Plan 9 results on this machine:

| Case | Result |
| --- | ---: |
| assets_10k | 207.44 MiB/s |
| assets_50k | 172.33 MiB/s |
| asset_database_5k | 253.60 MiB/s |
| asset_database_5k_owned | 240.27 MiB/s |
| asset_database_5k_file_read | 365.26 MiB/s |
| asset_database_5k_file_owned | 150.52 MiB/s |
| small_files_1k | 166.40 MiB/s |
| strings_plain_5k | 840.54 MiB/s |
| strings_escaped_5k | 524.48 MiB/s |
| deep_1k | 174.29 MiB/s |
| wide_10k | 278.41 MiB/s |
| query_assets_10k | 14.66M queries/s |
| query_first_10k | 13.79M queries/s |
| query_last_10k | 7.88M queries/s |
| query_missing_10k | 10.59M queries/s |
| query_string_view_10k | 13.14M queries/s |
| query_text_only_10k | 41.07M queries/s |
| query_symbol_compare_10k | 36.15M queries/s |
| query_many_keys_last | 4.02M queries/s |
| query_find_many_keys_last | 3.63M queries/s |
| query_child_at_many_keys_last | 3.58M queries/s |

Latest yyjson comparison results:

| Equivalent case | gsexp | yyjson | yyjson/gsexp |
| --- | ---: | ---: | ---: |
| assets_10k parse | 207.44 MiB/s | 623.49 MiB/s | 3.01x |
| assets_50k parse | 172.33 MiB/s | 662.27 MiB/s | 3.84x |
| asset_database_5k parse | 253.60 MiB/s | 696.08 MiB/s | 2.74x |
| small_files_1k parse | 166.40 MiB/s | 546.02 MiB/s | 3.28x |
| strings_plain_5k parse | 840.54 MiB/s | 1225.40 MiB/s | 1.46x |
| strings_escaped_5k parse | 524.48 MiB/s | 1116.04 MiB/s | 2.13x |
| wide_10k parse | 278.41 MiB/s | 774.15 MiB/s | 2.78x |
| assets_10k lookup | 14.66M queries/s | 105.80M queries/s | 7.22x |
| many_keys_last lookup | 4.02M queries/s | 11.70M queries/s | 2.91x |

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

Important Plan 7 retained changes:

1. Parser diagnostics now keep byte offsets during successful parsing and
   compute exact line/column only when an error is emitted.
2. Exact parser diagnostic positions are covered by tests.
3. Query extraction helpers use an internal value-child fast path while keeping
   the public API unchanged.
4. Atom scanning and plain-string scanning use an internal SSE2 path on x86
   builds with scalar fallback.
5. Wide child indexes are sorted and searched with `lower_bound`.
6. Escaped-string decoded storage starts with a smaller conservative reserve.
7. Tokenization and string quoting moved to `src/tokenize.cpp` to keep parser
   source size under the project guideline.

Plan 7 optimization attempt results:

| Attempt | Result |
| --- | --- |
| Lazy diagnostic line/column computation | Kept. Exact diagnostics still pass tests. Parse throughput improved across the main parse cases, including `assets_10k`, `asset_database_5k`, `small_files_1k`, and `wide_10k`. |
| Query helper value-child fast path | Kept. Public helpers remain unchanged. `query_first_10k`, `query_string_view_10k`, and common asset lookup improved versus the Plan 6 baseline. |
| Integrated SSE2 atom/plain-string scanning | Kept. `strings_plain_5k` reached 914.77 MiB/s and `wide_10k` reached 289.02 MiB/s on the latest run. Scalar fallback remains available. |
| Sorted lazy child indexes | Kept. Wide-key lookup improved versus the Plan 6 baseline, though results are still noisy and yyjson remains faster. |
| Retained `last_child` in `NodeData` | Rejected. It increased retained node storage substantially and hurt lookup throughput, so the parse-only `last_children` side vector was restored. |
| Smaller decoded string reserve | Kept. `strings_escaped_5k` retained storage dropped from about 4.5 MB to 4.2 MB with similar throughput. |
| yyjson source review | Completed. Useful takeaways: yyjson keeps compact 16-byte values, separates trivia skipping from hot value reads, and `yyjson_obj_getn` is still a linear object scan. The local query gap is therefore not because yyjson uses a magic hash table in the compared API. |

Important Plan 8 retained changes:

1. Benchmark storage stats now report `sizeof(NodeData)`, total retained node
   bytes, and node bytes per source byte.
2. `NodeData` stores text as offset/length plus a source/decoded tag instead of
   retaining `std::string_view`.
3. `ValueType` and `TokenType` use byte-sized enum storage.
4. Escaped strings decode directly into the parse-owned decoded text arena.
5. Query microbenchmarks isolate find-only and child-at lookup on wide records.
6. The benchmark includes an owned file-load path for the generated asset
   database.
7. Lazy child indexes use a sorted vector cache instead of an `unordered_map`.
8. yyjson benchmark helpers moved to `benchmarks/yyjson_bench.hpp` to keep the
   main benchmark file focused.

Plan 8 optimization attempt results:

| Attempt | Result |
| --- | --- |
| Node layout instrumentation | Kept. Stats now show node layout directly. Current `NodeData` is 24 bytes and dense generated data retains about 6 node bytes per source byte. |
| Offset/length node text storage | Kept. Node size dropped from 28 bytes after the first prototype to 24 bytes with enum packing, and retained memory dropped across all parse cases while preserving `Node::text()`. |
| Direct decoded string arena writes | Kept. `strings_escaped_5k` improved to 349.42 MiB/s on the latest run and retained decoded storage stayed lower than Plan 7. |
| Query-only microbenchmarks | Kept. `query_find_many_keys_last` and `query_child_at_many_keys_last` show lookup/traversal cost separately from extraction. |
| Sorted-vector child index cache | Kept. `query_many_keys_last` improved to 4.64M queries/s on the latest run and child-index storage is included in `approximate_bytes`. |
| Owned file-load benchmark | Kept. `asset_database_5k_file_owned` measured 162.86 MiB/s on the latest run, which includes cached file read plus `parse_owned`. |

Important Plan 9 retained changes:

1. Text-access query microbenchmarks now measure value `Node::text()` access and
   repeated symbol comparisons separately from normal extraction.
2. The asset database benchmark now reports in-memory parse, owned parse,
   cached file-read-only, and combined file-read-plus-parse paths.
3. Escaped string parsing copies contiguous plain chunks into the decoded arena
   instead of decoding every post-escape byte one character at a time.
4. Failed escaped strings are covered by tests that verify decoded arena
   rollback leaves no public roots and no decoded string storage.
5. Numeric extraction now accepts leading `+` consistently with
   `looks_like_integer` and `looks_like_float`, while keeping invalid numeric
   rejection tests.

Plan 9 optimization attempt results:

| Attempt | Result |
| --- | --- |
| Text-access microbenchmarks | Kept. Current run reports `query_text_only_10k` at 41.07M accesses/s and `query_symbol_compare_10k` at 36.15M comparisons/s. These are measurement tools, not public APIs. |
| Remove numeric pre-scan before `from_chars` | Rejected. It did not improve the numeric query cases on the measured run. The pre-scan stays, with a small fix for leading `+` consistency. |
| Fast key comparison helper | Rejected. Direct text equality helpers did not produce a convincing query-suite win and added internal surface area, so the code returned to the simpler `node_text(...) == symbol` path. |
| Escaped string chunk copying | Kept. `strings_escaped_5k` improved from the Plan 8 baseline of 349.42 MiB/s to 524.48 MiB/s on the latest run. |
| Decoded arena rollback test | Kept. Unterminated escaped strings are tested for no public roots, zero decoded string count, and zero decoded bytes. |
| Query cache locality follow-up | Measured, no further structure change. The wide-key query cases remain noisy and did not justify replacing the sorted lazy child-index cache in this pass. |
| Owned file-load benchmark split | Kept. Latest run: in-memory parse 253.60 MiB/s, owned parse 240.27 MiB/s, cached file read 365.26 MiB/s, combined file-read-plus-parse 150.52 MiB/s. |

## Optimization Plan 9

Goal: improve query/text access and escaped-string handling after the Plan 8
storage layout changes. Plan 8 made nodes smaller, but text access now rebuilds
views from offset/length and the remaining yyjson gap is mostly lookup and
escaped data.

Baseline gaps after Plan 8:

1. Plain long strings are close enough for now: 917.71 MiB/s vs yyjson
   1339.24 MiB/s.
2. Escaped strings are still slow: 349.42 MiB/s vs yyjson 1123.13 MiB/s.
3. Common asset lookup is still far behind yyjson on the latest run.
4. Wide-key lookup improved, but is still slower than yyjson.
5. `asset_database_5k_file_owned` is much slower than in-memory parse, so real
   file loading needs clearer measurement before optimizing it.

Work order:

1. Text access microbenchmarks.
   Add benchmarks for `Node::text()` only, value-child text access through
   extraction helpers, and repeated symbol comparisons. Determine whether
   offset/length reconstruction is a meaningful query cost.

2. Numeric helper scan removal.
   `extract_int` and `extract_float` still pre-scan with `looks_like_*` before
   `from_chars`. Try removing the pre-scan and relying on `from_chars` plus the
   end pointer. Keep only if numeric query benchmarks improve without accepting
   invalid forms.

3. Fast key comparison.
   For source-backed atom keys, compare key length before building a
   `string_view` and avoid reconstructing views inside tight child scans where
   possible. Keep the public `Node::text()` API unchanged.

4. Escaped string chunk copying.
   After the first escape, the parser currently decodes character by character.
   Copy contiguous non-escape spans into the decoded arena and handle only
   escape points individually. This targets `strings_escaped_5k`.

5. Decoded arena rollback guard.
   Make direct decode rollback explicit and cheap for unterminated strings. Add
   tests that ensure failed escaped strings do not leave user-visible decoded
   nodes.

6. Query cache locality follow-up.
   Use the query microbenchmarks to decide whether the sorted child-index cache
   should become a flatter arena-backed structure. Reject any version that only
   helps one query while hurting common lookup or increasing parse cost.

7. Owned file-load benchmark split.
   Split `asset_database_5k_file_owned` into file read time and parse-owned time
   so we know whether the current 162.86 MiB/s is parser cost or filesystem
   overhead. Optimize only the parser side.

8. Ownership/API audit.
   Revisit `shared_ptr<ParseStorage>` only after query/text microbenchmarks
   prove ownership overhead matters. Default remains no public API churn.

Acceptance rules:

1. Each attempt gets before/after benchmark output and a keep/reject note.
2. `gsexp ./scripts/build.sh` passes.
3. `gsexp ./scripts/bench.sh` passes with yyjson enabled.
4. Benchmark build with `GSEXP_BENCHMARK_YYJSON=OFF` still works.
5. `glayout ./scripts/build.sh` passes after any public API, build, or vendoring
   change.
6. Public API stays stable unless README and glayout are updated in the same
   change.
7. Extraction helpers must keep rejecting invalid numeric inputs already covered
   by tests, and new numeric edge cases should be added before changing numeric
   parsing.
8. Keep files within the project size guideline by splitting cohesive benchmark
   or parser helpers as needed.

Rejected-by-default unless evidence changes:

1. Reintroducing retained `std::string_view` in every node.
   Plan 8 showed the smaller node layout is valuable.
2. Eagerly hashing or indexing every key during parse.
   Keep indexes lazy unless query benchmarks prove otherwise.
3. Adding a second public lookup API before optimizing existing helpers.
4. Optimizing file I/O before separating it from parser-owned parse cost.

## Optimization Plan 8

Goal: reduce the remaining gap against yyjson by improving storage layout,
retained node size, and lookup locality. Plan 7 already improved scanning and
success-path parsing; Plan 8 should avoid more broad scanner work until the
retained representation has been measured.

Baseline gaps after Plan 7:

1. `strings_plain_5k` is now relatively close to yyjson at 1.41x behind.
2. `asset_database_5k`, `assets_*`, and `wide_10k` are still roughly 3x behind.
3. Escaped strings are still about 4x behind.
4. Query helpers improved, but common asset lookup is still roughly 3.6x behind
   yyjson and wide-key lookup is roughly 1.9x behind.

Work order:

1. Measure node layout directly.
   Add benchmark output for `sizeof(NodeData)`, node bytes per source byte, and
   text-storage overhead. Use this before changing representation so every
   layout attempt has a memory-locality baseline.

2. Offset/length text representation.
   Prototype replacing retained `std::string_view` in `NodeData` with source
   offset and length fields. `Node::text()` can still return
   `std::string_view`, so the public API can remain stable. This should improve
   node density and cache behavior if the packed fields are materially smaller
   than the current view-based layout.

3. Pack node metadata.
   Measure narrower fields for type, child count, and links. Keep `uint32_t`
   indexes unless there is a proven need for larger files. Avoid clever bit
   packing unless it is clearly faster or smaller without making debugging bad.

4. Direct decoded string arena writes.
   Escaped string parsing still builds a temporary `std::string`, then copies to
   decoded storage. Decode directly into the parse-owned arena and roll back on
   unterminated strings if needed. This targets the biggest remaining string
   gap.

5. Query-only microbenchmarks.
   Add benchmarks that isolate `find_child`, value-child lookup, numeric
   parsing, and optional/string construction separately. Do this before more
   lookup rewrites so the next change attacks the actual slow component.

6. Child index locality.
   After query microbenchmarks exist, try a compact per-parse side array for
   child indexes instead of an `unordered_map<uint32_t, vector<Entry>>`. Keep
   lazy construction. Reject if parse memory or simple lookup cases regress.

7. Owned file-load hot path.
   Add a benchmark that reads generated content into a `std::string` and calls
   `parse_owned(std::move(text))`. If this becomes the intended asset database
   path, optimize that path first. Do not add an unsafe in-situ parser unless
   the owned path is clearly insufficient.

8. Optional public API review.
   Only after storage experiments, decide whether any API addition is warranted.
   Default answer remains no: callers should use `parse` and `parse_owned`.

Acceptance rules:

1. Each attempt gets before/after benchmark output and a keep/reject note.
2. `gsexp ./scripts/build.sh` passes.
3. `gsexp ./scripts/bench.sh` passes with yyjson enabled.
4. Benchmark build with `GSEXP_BENCHMARK_YYJSON=OFF` still works.
5. `glayout ./scripts/build.sh` passes after any public API, build, or vendoring
   change.
6. Public API stays stable unless README and glayout are updated in the same
   change.
7. `Node::text()` and existing extraction helpers keep their behavior unless a
   deliberate API change is documented.
8. Keep files within the project size guideline by splitting cohesive storage,
   tokenizer, benchmark, or lookup helpers as needed.

Rejected-by-default unless evidence changes:

1. In-situ parsing that leaves callers responsible for source lifetime.
   `parse_owned` already gives the parser ownership safely.
2. Complex compressed node formats that make debugger inspection painful.
3. Hashing every key during parse.
   Most data does not need indexes. Keep indexes lazy unless benchmarks prove
   eager indexing is worth it.
4. Copying yyjson internals wholesale.
   The useful lesson is compact layout and hot-loop discipline, not a wholesale
   JSON parser architecture transplant.

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
