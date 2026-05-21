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

Latest verified Plan 11 results on this machine:

| Case | Result |
| --- | ---: |
| assets_10k | 254.56 MiB/s |
| assets_50k | 216.99 MiB/s |
| asset_database_5k | 305.07 MiB/s |
| asset_database_5k_owned | 320.04 MiB/s |
| asset_database_20k | 311.49 MiB/s |
| asset_database_20k_owned | 296.37 MiB/s |
| asset_database_5k_file_read | 376.44 MiB/s |
| asset_database_5k_file_owned | 168.97 MiB/s |
| small_files_1k | 215.53 MiB/s |
| strings_plain_5k | 1073.81 MiB/s |
| strings_escaped_5k | 714.02 MiB/s |
| deep_1k | 232.47 MiB/s |
| code_forms_2k | 270.15 MiB/s |
| wide_10k | 368.09 MiB/s |
| query_assets_10k | 24.41M queries/s |
| query_assets_10k_cold_once | 16.70M queries/s |
| query_assets_10k_warm_repeated | 20.78M queries/s |
| query_internal_assets_10k | 36.90M queries/s |
| query_internal_ordered_assets_10k | 49.05M queries/s |
| query_first_10k | 15.73M queries/s |
| query_last_10k | 10.67M queries/s |
| query_missing_10k | 13.35M queries/s |
| query_string_view_10k | 17.01M queries/s |
| query_text_only_10k | 46.92M queries/s |
| query_symbol_compare_10k | 43.44M queries/s |
| query_asset_database_5k | 23.55M queries/s |
| query_internal_asset_database_5k | 36.92M queries/s |
| query_asset_database_20k | 21.91M queries/s |
| query_internal_asset_database_20k | 34.98M queries/s |
| query_many_keys_8_last | 10.73M queries/s |
| query_many_keys_16_last | 9.63M queries/s |
| query_many_keys_16_cold_once | 1.39M queries/s |
| query_many_keys_16_warm_repeated | 8.63M queries/s |
| query_many_keys_24_last | 7.39M queries/s |
| query_many_keys_48_last | 4.21M queries/s |
| query_many_keys_last | 7.58M queries/s |
| query_find_many_keys_last | 7.68M queries/s |
| query_child_at_many_keys_last | 7.41M queries/s |
| query_find_arg_many_keys_last | 7.65M queries/s |
| query_nested_find_arg_5k | 24.47M queries/s |
| query_internal_nested_find_arg_5k | 75.01M queries/s |
| iterate_code_forms_2k | 38.01M visits/s |
| iterate_internal_code_forms_2k | 231.52M visits/s |
| iterate_child_span_code_forms_2k | 205.80M visits/s |

Latest yyjson comparison results:

| Equivalent case | gsexp | yyjson | yyjson/gsexp |
| --- | ---: | ---: | ---: |
| assets_10k parse | 254.56 MiB/s | 675.40 MiB/s | 2.65x |
| assets_50k parse | 216.99 MiB/s | 645.01 MiB/s | 2.97x |
| asset_database_5k parse | 305.07 MiB/s | 798.06 MiB/s | 2.62x |
| asset_database_20k parse | 311.49 MiB/s | 767.05 MiB/s | 2.46x |
| small_files_1k parse | 215.53 MiB/s | 577.29 MiB/s | 2.68x |
| strings_plain_5k parse | 1073.81 MiB/s | 1318.38 MiB/s | 1.23x |
| strings_escaped_5k parse | 714.02 MiB/s | 1168.21 MiB/s | 1.64x |
| code_forms_2k parse | 270.15 MiB/s | 640.65 MiB/s | 2.37x |
| wide_10k parse | 368.09 MiB/s | 831.31 MiB/s | 2.26x |
| assets_10k lookup | 24.41M queries/s | 53.03M queries/s | 2.17x |
| asset_database_5k lookup | 23.55M queries/s | 55.05M queries/s | 2.34x |
| asset_database_20k lookup | 21.91M queries/s | 50.18M queries/s | 2.29x |
| many_keys_last lookup | 7.58M queries/s | 11.61M queries/s | 1.53x |

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

Important Plan 10 retained changes:

1. `FormView` is now the normal public query API for headed forms.
2. Public free helpers `find_child`, `extract_int`, `extract_float`,
   `extract_string`, and `extract_string_view` were removed instead of kept as
   compatibility wrappers.
3. Tests, examples, README, and benchmarks use `FormView`.
4. `glayout` was updated to use `FormView` and builds against the new API.
5. `FormView::get_*` uses internal value-child lookup helpers to avoid extra
   temporary `Node` and `FormView` construction on hot query paths.
6. Wide forms still use the parse-storage lazy child index. Small forms use
   direct scans because per-view heap indexing was slower.

Plan 10 optimization attempt results:

| Attempt | Result |
| --- | --- |
| Public `FormView` API migration | Kept. It removes parallel public extraction styles and gives the query code a single documented shape. |
| Per-view vector cache built on first lookup | Rejected. It dropped common asset lookup to 5.38M queries/s and single-field lookups to about 1.9M queries/s because every view paid heap allocation and sort cost. |
| Direct small-form scan plus existing wide-form storage cache | Kept. It recovered wide-key lookup while avoiding the per-view allocation cost for small forms. |
| Internal `FormView::get_*` value-child fast path | Kept. Current run reached 16.31M queries/s for common asset lookup and 4.94M queries/s for many-key lookup, both above the Plan 9 recorded results. |
| Make `get_string_view` the benchmark default | Kept. The asset lookup benchmark now measures borrowed string access through `FormView`, which is the intended asset-loading style when `ParseResult` remains alive. |

## Optimization Plan 11

Goal: stop treating syntax simplicity as enough. `gsexp` has simpler syntax
than JSON, but its retained tree is currently heavier than yyjson's DOM layout.
Plan 11 is allowed to heavily change internals while keeping one normal public
API: `parse`/`parse_owned`, then `Node` and `FormView`. The target is a
representation that is faster to walk, faster to query, and closer to
contiguous memory without asking users to switch to a second "fast" reader.

Current gap during Plan 11, refreshed on the clean `5fd4fa7` plus later
documentation commits baseline:

1. `assets_10k` parse is about 3.00x behind yyjson: 239.21 MiB/s versus
   718.27 MiB/s on the latest clean run.
2. `asset_database_5k` parse is about 2.72x behind yyjson: 303.10 MiB/s
   versus 825.69 MiB/s.
3. `code_forms_2k` parse is about 2.49x behind yyjson: 266.07 MiB/s versus
   661.96 MiB/s.
4. `assets_10k` lookup is about 5.04x behind yyjson: 25.51M q/s versus
   128.64M q/s.
5. `asset_database_5k` lookup is about 3.00x behind yyjson: 22.61M q/s versus
   67.75M q/s.
6. `many_keys_last` lookup is about 1.85x behind yyjson: 7.81M q/s versus
   14.41M q/s.
7. The public API is now clean enough that internal representation churn should
   not force another user-facing rewrite.

Public API direction:

1. Keep one normal consumption path: `parse`/`parse_owned`, then `Node` and
   `FormView`.
2. Do not add a second public batch/cursor API just to win benchmarks. yyjson's
   compared path uses one loaded document and normal repeated object lookup;
   `gsexp` should make normal `FormView::get_*` calls competitive.
3. `FormView` should no longer be treated as necessarily stateless. It may hold
   tiny mutable state, a storage-cache handle, or a pointer/id into
   `ParseStorage` if that makes the same public API faster.
4. Storage-owned caches are preferred over user-visible helper objects. A normal
   caller should not need to choose between a "simple" API and a "fast" API.
   The fast path should be hidden behind `FormView`.
5. Benchmark-only scan-once or cursor experiments are allowed only to identify
   the ceiling and bottleneck. If they win, the production fix should be
   internal state, storage-owned caches, or representation changes behind the
   existing `FormView` API unless there is strong evidence users need a new
   public shape.

Attempt results so far:

| Attempt | Result |
| --- | --- |
| Contiguous child-span arena with parser-only sibling finalization | Rejected. `NodeData` dropped from 24 to 20 bytes, but `assets_10k` parse fell from the Plan 10 result of 223.80 MiB/s to 125.01 MiB/s, and common asset lookup fell from 16.31M queries/s to 13.14M queries/s. The finalization pass and extra child-index indirection outweighed the smaller node. |
| Atom text hash stored in every node | Rejected. `NodeData` grew from 24 to 28 bytes. `assets_10k` parse fell to 159.94 MiB/s and common asset lookup fell to 14.68M queries/s. Head lookup needs a side-table or head-only strategy, not a wider node for every atom. |
| Length-gated `FormView` direct lookup | Rejected. It did not improve the main lookup benchmarks and hurt wide-key lookup in the measured run. |
| Custom checked integer parser | Rejected. It preserved tests but did not beat `looks_like_integer` plus `from_chars` on the integer lookup benchmarks. |
| Length-gated `Node::is_atom` | Rejected. It was too noisy and did not produce a stable improvement in the isolated final run. |
| Head metadata in list text fields | Rejected. It kept `NodeData` at 24 bytes, but it slowed parse by mutating parent list text fields on first-child append and did not improve lookup. |
| Remove `FormView` last-lookup cache | Kept. The cache was not used by the hot `get_*` path and only helped repeated `find()` of the same key on the same view. Removing it simplifies `FormView` and removes one string comparison from `find()`. |
| Flat child-index lookup table | Kept. Wide-form lookup now uses a direct `list_index -> cache_index` table after the first wide lookup. It improved `query_many_keys_last` from 4.94M to 5.65M queries/s and `query_find_many_keys_last` from 4.58M to 6.33M queries/s. The table is allocated lazily only when wide-form indexes are used. |
| Flat index-entry arena | Rejected. Replacing one vector per lazy wide-form index with one shared `child_index_entries` arena regressed the target wide lookup cases in the measured run: `query_many_keys_last` fell to 5.20M queries/s and `query_find_many_keys_last` fell to 4.88M queries/s. The retained memory shape was simpler, but the lookup path lost enough locality or iterator simplicity to reject it. |
| Compiler CTZ for SSE2 movemask result | Kept. A same-session A/B run against the manual first-set-bit loop improved important parse cases: `assets_10k` went from 216.00 to 233.34 MiB/s, `asset_database_5k` from 260.47 to 296.71 MiB/s, and `strings_escaped_5k` from 588.81 to 645.53 MiB/s. The fallback loop remains for non-GCC/non-Clang compilers. |
| Code-like form fixture | Kept. `code_forms_2k` adds a non-record-shaped workload with nested calls, blocks, strings, arithmetic forms, and conditionals. The first measured result is 699,796 bytes, 162,016 nodes, and 242.29 MiB/s. This is benchmark coverage, not an optimization by itself. |
| Head-only side table indexed by node | Rejected. Storing a `list_heads` side vector outside `NodeData` added 4 bytes of retained capacity per node and slowed the measured hot cases: `assets_10k` fell to 220.44 MiB/s, `code_forms_2k` fell to 214.12 MiB/s, and `query_assets_10k` fell to 14.13M queries/s. The existing `first_child` field is the better head source until there is a compact list-only table or symbol metadata with a larger payoff. |
| Direct `Node::is_atom()` text comparison | Kept. Comparing text size and bytes directly avoids constructing a `string_view` on this symbol hot path. `query_symbol_compare_10k` improved from 30.37M to 40.88M queries/s, while `query_first_10k` improved to 13.69M queries/s and `query_string_view_10k` improved to 14.36M queries/s in the latest full run. The broader FormView direct-compare variant was rejected during the same attempt because it was mixed and duplicated too much local text logic. |
| Direct index traversal in `Node::child_at()` and `second()` | Rejected. Replacing temporary `Node` traversal with direct sibling-index walking looked simpler, but target query cases regressed in the measured run: `query_child_at_many_keys_last` fell to 3.36M queries/s and `query_many_keys_last` fell to 3.97M queries/s. The previous implementation was restored. |
| Lower small-form index threshold from 16 to 8 | Rejected. The asset record shape has enough fields to trigger indexing at threshold 8, but the measured result was worse: `query_assets_10k` fell to 11.47M queries/s and `query_string_view_10k` fell to 9.49M queries/s. Retained bytes for the query case also grew from about 9.6 MB to 13.4 MB because 10,000 small child indexes were built. |
| Use `std::sort` instead of `std::stable_sort` for child indexes | Rejected. Stable ordering is not required for correctness, but the measured run regressed relevant cases: `query_many_keys_last` fell to 3.70M queries/s, `query_find_many_keys_last` fell to 4.72M queries/s, and `query_assets_10k` fell to 10.57M queries/s. The stable sort path was restored. |
| Resize plus `memcpy` for decoded string chunks | Rejected. Replacing `decoded_text.insert()` with a manual resize plus `memcpy` helper regressed the escaped string benchmark to 437.81 MiB/s and also hurt unrelated parse cases in the measured run. The original `vector::insert` path was restored. |
| ASCII fast path in whitespace/comment skipper | Rejected. Checking `byte > ' '` before the full whitespace predicate improved some asset parse cases, but it was not broad enough: `code_forms_2k` fell to 230.94 MiB/s, `strings_plain_5k` fell to 974.72 MiB/s, and `wide_10k` fell to 306.75 MiB/s in the measured run. The simpler original skipper was restored. |
| Combined integer validation and conversion | Rejected. Boundary tests for `INT_MIN`, `INT_MAX`, and overflow were added and passed, but the custom conversion did not produce a clear query-suite win. `query_first_10k` improved to 14.17M queries/s, but `query_many_keys_last` fell to 4.61M queries/s and `query_find_many_keys_last` fell to 4.65M queries/s, so the `looks_like_integer` plus `from_chars` path was restored. |
| JSON equivalent for code-shaped forms | Kept. `yyjson_code_forms_2k` adds an AST-shaped JSON equivalent for `code_forms_2k`, so code-like input now appears in the yyjson comparison table. First measured comparison: `gsexp` 226.57 MiB/s on 699,796 bytes versus yyjson 599.42 MiB/s on 899,797 bytes, a 2.65x gap by each format's own byte size. |
| Start child scans at the second child | Rejected. Removing the per-loop `first` branch from direct lookup and index building improved `query_first_10k`, but it regressed wide-key lookup badly in the measured run: `query_many_keys_last` fell to 3.26M queries/s, `query_find_many_keys_last` fell to 3.78M queries/s, and `query_child_at_many_keys_last` fell to 3.03M queries/s. The original branchy scan was restored. |
| Lower node reserve slack from 1.08 to 1.02 | Kept. This is a modest allocation-discipline win rather than a major speed win. `strings_plain_5k` node capacity dropped from 61,602 to 58,180 and retained approximate bytes dropped from 2,655,129 to 2,573,001. `strings_escaped_5k` node capacity dropped from 61,064 to 57,673 and retained approximate bytes dropped from 3,725,978 to 3,644,594. The full measured run stayed within normal throughput noise or better: `assets_10k` 202.09 MiB/s, `asset_database_5k` 290.47 MiB/s, and `strings_escaped_5k` 643.41 MiB/s. |
| Narrow retained child count with overflow side table | Kept. `NodeData` dropped from 24 to 20 bytes while preserving exact `child_count()` through a rare overflow side table covered by a 70,001-child regression test. Retained node bytes dropped from about 8.26 MB to 6.88 MB on `assets_10k`, 4.41 MB to 3.68 MB on `asset_database_5k`, and 4.20 MB to 3.50 MB on `code_forms_2k`. Query throughput improved on the measured run (`query_assets_10k` 16.27M queries/s, `query_many_keys_last` 5.76M queries/s), while parse throughput was mixed enough to treat this primarily as a memory/cache-layout win. |
| Compact wide-index entries | Kept. Lazy wide-form index entries now store the atom-head node index instead of a retained `string_view`, reducing `KeyIndexEntry` storage while keeping index sorting and lookup private. On `query_many_keys_last`, approximate retained bytes dropped from about 14.58 MB to 12.66 MB and throughput improved to 6.11M queries/s. `query_find_many_keys_last` and `query_child_at_many_keys_last` dipped slightly, so this is kept for memory plus acceptable mixed lookup performance rather than as a pure speed win. |
| Compact sorted child-index lookup table | Rejected. Replacing the dense `list_index -> cache_index` lookup table with sorted lookup entries reduced `query_many_keys_last` retained approximate bytes further, from about 12.66 MB to 11.24 MB, but repeated wide lookup regressed too much: `query_many_keys_last` fell from 6.11M to 4.52M queries/s and `query_find_many_keys_last` fell from 5.59M to 4.51M queries/s. The dense table was restored. |
| Direct small-form head text comparison | Kept. Direct lookup now compares candidate head size and bytes without first constructing a `string_view`. The measured common small-form path improved: `query_assets_10k` rose from 17.84M to 18.23M queries/s, `query_first_10k` from 14.54M to 15.55M queries/s, and `query_string_view_10k` from 13.79M to 15.69M queries/s. Wide-index results were mixed on the same run, but this change does not alter the indexed lookup path. |
| Source-atom indexed comparison helpers | Rejected. Specializing indexed child-entry sort and search for source-backed atom text added local comparison helpers but did not improve the indexed lookup cases. The measured run had `query_many_keys_last` at 4.75M queries/s and `query_find_many_keys_last` at 5.61M queries/s, below the previous documented mixed indexed results, so the normal `node_text()` indexed path was restored. |
| First-argument-only `get_*` helper | Rejected. Splitting `get_*()` onto a specialized first-argument helper removed the general arg-index loop from the hot path, but the measured query suite regressed: `query_assets_10k` fell to 17.04M queries/s and `query_string_view_10k` fell to 14.08M queries/s. The simpler shared `find_arg_data(..., 0)` helper was restored. |
| Head-only symbol interning with unordered map | Rejected. Interning only first-child atom heads gave lookup a possible symbol-ID path, but parse cost and retained storage dominated. `assets_10k` parse fell to 158.18 MiB/s, `asset_database_5k` fell to 138.80 MiB/s, `code_forms_2k` fell to 123.59 MiB/s, and `assets_10k` retained approximate bytes rose from about 8.26 MB to 9.31 MB because every node carried a side-vector symbol slot. The implementation was removed. |
| 16-bit dense child-index lookup table | Kept. The dense `list_index -> cache_index` table now stores 16-bit cache ids with a direct-scan fallback if more than 65k wide indexes are built. This keeps the O(1) lookup shape that beat the compact sorted table while reducing `query_many_keys_last` retained approximate bytes from about 12.66 MB to 11.92 MB. The measured wide lookup cases stayed in the same range: `query_many_keys_last` 5.51M queries/s, `query_find_many_keys_last` 5.49M queries/s, and `query_child_at_many_keys_last` 5.03M queries/s. |
| Child-index lookup byte stats | Kept. Benchmark stats now print `child_index_lookup_bytes` separately from capacity so memory changes to the dense lookup table are visible without inferring element size from the implementation. This is measurement support, not a parser behavior change. |
| Materialized wide-index key offsets | Kept. Wide child-index entries now store source offset and size for the key instead of the head node index. This avoids chasing the head node and using the general `node_text()` path during indexed binary search. It increases `query_many_keys_last` retained approximate bytes from about 11.92 MB to 12.40 MB, but the measured wide lookup cases improved enough to keep it: `query_many_keys_last` 6.33M queries/s, `query_find_many_keys_last` 5.66M queries/s, and `query_child_at_many_keys_last` 6.29M queries/s. |
| Direct byte comparison for materialized wide-index keys | Kept. Indexed sort and lookup now compare materialized key offsets with `memcmp` and explicit length checks instead of relying on `string_view` relational operators. The retained memory shape is unchanged, and the measured indexed lookup cases improved: `query_many_keys_last` 6.20M queries/s, `query_find_many_keys_last` 7.93M queries/s, and `query_child_at_many_keys_last` 6.57M queries/s. |
| Raw offset/size wide-index comparison | Rejected. Removing `string_view` construction inside indexed comparisons made `query_many_keys_last` faster in the measured run at 7.94M queries/s, but it regressed two adjacent indexed cases: `query_find_many_keys_last` fell to 6.18M queries/s and `query_child_at_many_keys_last` fell to 5.75M queries/s. The previous comparison helper was restored. |
| ParseResult root reserve | Rejected. Reserving one root did not improve the expected one-root parse cases in the measured run: `small_files_1k` fell to 175.82 MiB/s, `assets_10k` fell to 218.48 MiB/s, and `strings_plain_5k` fell to 909.76 MiB/s. Some unrelated query cases moved with normal benchmark noise, so the reserve was removed. |
| Source-atom direct lookup helper | Kept. Direct small-form lookup now uses a source-backed atom comparison helper instead of the generic text-storage comparison path. The second measured run showed the target small-form lookup cases in a good range: `query_assets_10k` 17.73M queries/s, `query_first_10k` 16.88M queries/s, and `query_string_view_10k` 16.68M queries/s. Wide lookup also stayed healthy in that run: `query_many_keys_last` 6.74M queries/s, `query_find_many_keys_last` 6.84M queries/s, and `query_child_at_many_keys_last` 6.85M queries/s. |
| Parser-local list tail storage | Kept. The parser no longer keeps a separate `last_children` vector for every node. List nodes have no public text, so parsing uses their text-offset field as the current child tail while sibling links are built. Two measured runs showed better parse throughput; the second run had `assets_10k` 254.70 MiB/s, `asset_database_5k` 300.09 MiB/s, `small_files_1k` 214.56 MiB/s, and `code_forms_2k` 258.48 MiB/s. Lookup stayed in range, with `query_assets_10k` 17.72M queries/s and `query_many_keys_last` 7.36M queries/s. |
| Closed-list head metadata in text offset | Rejected. After parser-local tail storage, list text offsets were changed to hold the list head once a list closed, and `FormView` lookup used that field. The second measured run regressed common lookup too much: `query_assets_10k` fell to 15.93M queries/s, `query_first_10k` to 15.54M queries/s, and `query_string_view_10k` to 16.13M queries/s. Wide lookup also failed to hold the first-pass win, so the lookup path went back to `first_child`. |
| Packed 16-byte `NodeData` with side child counts | Rejected. Packing value type and text storage into the text-size word and moving child counts to a narrow side vector reduced `NodeData` from 20 to 16 bytes, and approximate retained bytes dropped on `assets_10k` from about 8.26 MB to 7.57 MB. The throughput tradeoff was not good enough: after inlining the metadata accessors, the second measured run still had `assets_10k` parse at 226.54 MiB/s, `small_files_1k` at 194.83 MiB/s, and `query_string_view_10k` at 15.37M queries/s. The simpler 20-byte node layout was restored. |
| Direct node-vector construction | Rejected. Changing `Parser::add_node()` to `emplace_back()` and fill the appended node in place did not produce a clear parse win and moved unrelated query cases down in the measured run. `assets_10k` parse was 243.01 MiB/s, `asset_database_5k` was 271.94 MiB/s, `code_forms_2k` was 232.32 MiB/s, and `query_string_view_10k` fell to 15.03M queries/s. The temporary `NodeData` plus `push_back()` path was restored. |
| Child iteration benchmark | Kept. `iterate_assets_10k` now measures direct child traversal separately from `FormView` lookup by walking asset children, field children, and field heads. First baseline on the current sibling-chain representation is 18.0M visits in 0.403769 seconds, or 44.58M visits/s. This is measurement support for future child-span or traversal changes, not a parser behavior change. |
| Remove child-iterator bounds check | Rejected. `ChildIterator::operator++()` dropped its `index >= nodes.size()` guard for iterators produced from valid parsed nodes. The measured run regressed the new traversal benchmark instead of improving it: `iterate_assets_10k` fell from the 44.58M visits/s baseline to 41.02M visits/s. Adjacent traversal-heavy query cases also moved down, so the safer checked increment was restored. |
| Inline `Node::child_count()` common path | Rejected. Moving the non-overflow child-count branch directly into `Node::child_count()` only produced a very small traversal movement, from the 44.58M visits/s baseline to 44.99M visits/s on the second measured run. Adjacent lookup results were mixed, including `query_string_view_10k` at 15.50M queries/s, so the helper-based implementation was restored rather than keeping a noise-level change. |
| Scalar prefix before atom SIMD scan | Rejected. Checking up to eight atom bytes scalarly before the SSE2 delimiter scan was intended to help short atoms, but it regressed the main parse fixtures badly in the measured run: `assets_10k` fell to 188.70 MiB/s, `asset_database_5k` to 241.33 MiB/s, `small_files_1k` to 148.94 MiB/s, and `code_forms_2k` to 219.46 MiB/s. The immediate SSE2 atom scan was restored. |
| Public scan-once asset query probe | Rejected. A benchmark-only path walked each asset form once and extracted `id`, `x`, `y`, and `path` through the public `Node` traversal API. It did not expose a useful ceiling: the refined probe reached only 8.32M queries/s while the normal `FormView::get_*` path reached 18.13M queries/s in the same run. Public nested child traversal is too expensive for this ceiling test, so the probe was removed instead of kept as benchmark noise. |
| `FormView` direct-scan resume hint | Rejected. A single mutable child-node hint let repeated small-form lookups resume after the last successful lookup without adding a public API or heap allocation. The extra state and wraparound logic regressed the measured query suite: `query_assets_10k` fell to 16.47M queries/s, `query_first_10k` to 15.15M queries/s, `query_last_10k` to 8.70M queries/s, and `query_string_view_10k` to 16.95M queries/s. The stateless direct scan was restored. |
| Full int/float numeric conversion cache | Rejected. Caching both integer and float conversions by node improved common lookup slightly in one run, but it allocated two node-sized side vectors and regressed integer-only lookup: `query_first_10k` fell to 12.96M queries/s. Integer conversion is not expensive enough to justify that cache shape. |
| Lazy float conversion cache | Kept. `get_float()` now caches finite parsed float values in a storage-owned side vector, using `NaN` as unknown and `inf` as invalid so the cache costs 4 bytes per node only after the first float lookup. A same-session A/B run had baseline `query_assets_10k` at 16.84M queries/s and `query_last_10k` at 8.85M queries/s. The final kept run measured `query_assets_10k` at 23.98M queries/s and `query_last_10k` at 10.42M queries/s, with `float_cache_bytes=1,360,008` on `assets_10k` float-query cases. |
| Fixed small-form value cache | Rejected. A storage-owned fixed cache for up to eight small-form fields improved the float-heavy `query_last_10k` case, but it added about 3.06 MB of side-cache memory on `assets_10k` and failed to beat the simpler lazy float cache on the main common lookup. The all-`get_*` version also regressed `query_first_10k` to 10.91M queries/s, and the float-only version measured `query_assets_10k` at 21.87M queries/s versus the kept float-cache run at 23.98M. |
| SSE2 whitespace-run skipper | Rejected. Replacing the scalar one-byte whitespace skip with an SSE2 run skipper regressed parse throughput across the suite. The measured run had `assets_10k` at 195.72 MiB/s, `asset_database_5k` at 253.35 MiB/s, `strings_plain_5k` at 848.65 MiB/s, `deep_1k` at 129.59 MiB/s, and `code_forms_2k` at 205.78 MiB/s. The short whitespace gaps in normal files do not amortize the SIMD setup cost. |
| Legacy node reserve without sampling | Rejected. Removing the 16 KiB node-count sample and always reserving `source.size() / 4` did not produce a broad parse win, and it over-reserved badly on string-heavy fixtures. `strings_plain_5k` retained node capacity jumped from about 58k to 294,170 for 55,002 nodes, raising approximate retained bytes from about 2.34 MB to 7.06 MB. `strings_escaped_5k` similarly rose to 322,920 node capacity and about 8.72 MB retained. |
| 4 KiB node-reserve sample | Kept. Reducing the reserve estimator sample from 16 KiB to 4 KiB cuts pre-scan work while preserving the memory savings from sampled reserve. In the same-session A/B run, 4 KiB improved `assets_10k` from 243.58 to 260.59 MiB/s, `asset_database_5k` from 310.03 to 322.67 MiB/s, `strings_plain_5k` from 1039.37 to 1125.90 MiB/s, and `code_forms_2k` from 269.73 to 272.05 MiB/s. String-heavy node capacity rose modestly (`strings_plain_5k` from 58,180 to 58,620; `strings_escaped_5k` from 57,673 to 60,809), which is acceptable compared with the rejected legacy reserve. |
| 1 KiB and 2 KiB node-reserve samples | Rejected. Smaller reserve samples were too mixed after the 4 KiB keep. The 1 KiB run improved some parse cases (`assets_10k` 264.91 MiB/s, `strings_plain_5k` 1157.64 MiB/s, `code_forms_2k` 280.73 MiB/s), but it regressed `asset_database_5k` versus the same-session 4 KiB baseline and increased string-heavy node capacity (`strings_plain_5k` 66,824; `strings_escaped_5k` 65,634). The 2 KiB run was worse on the main parse cases (`assets_10k` 237.82 MiB/s, `asset_database_5k` 301.37 MiB/s, `strings_plain_5k` 920.84 MiB/s), so 4 KiB remains the best documented balance. |
| 50% decoded string reserve | Rejected. Reducing the first escaped-string decoded-text reserve from 75% of source to 50% looked like a retained-memory win, but vector growth made it slower and larger on the escaped-string fixture. The measured run had `strings_escaped_5k` at 662.00 MiB/s with retained approximate bytes at 3,799,541, versus the previous 4 KiB reserve-sample run at 754.38 MiB/s and about 3,476,622 retained bytes. The 75% reserve was restored. |
| Inline `FormView` small-form value cache | Rejected. A mutable inline cache tried to build a small head/value table on the second `get_*` call for the same form, keeping the public API unchanged. The eight-entry version improved `query_assets_10k` to 24.86M q/s but hurt one-off lookups (`query_first_10k` 13.29M q/s, `query_last_10k` 9.17M q/s). Removing array zero-initialization and shrinking to five entries still left mixed results (`query_assets_10k` 24.92M q/s, `query_last_10k` 9.53M q/s, `query_many_keys_last` 5.92M q/s) and made `FormView` substantially larger, so the cache was removed. |
| Storage-owned hot-form value cache | Rejected. Moving the same small-form cache into `ParseStorage` kept `FormView` small and avoided a second public API, but the build and miss/fallback path cost more than repeated direct scans. The measured run regressed the main target to `query_assets_10k` 21.67M q/s, with `query_last_10k` 9.57M q/s and `iterate_assets_10k` 42.82M visits/s. The stateless direct lookup path was restored. |
| Unchecked source-atom comparison | Rejected. Removing internal bounds checks from the source-backed atom comparison was safe for parser-owned nodes in theory, but it did not improve the overall lookup suite. The measured run improved `query_first_10k` to 16.17M q/s but regressed the main common lookup to `query_assets_10k` 21.95M q/s and traversal to `iterate_assets_10k` 39.39M visits/s, so the checked helper was restored. |
| Integer-only lazy conversion cache | Rejected. Caching only integer conversions avoided the earlier full numeric cache's float side cost, but still did not pay for itself. The measured run had `query_first_10k` at 15.42M q/s while adding about 1.70 MB of int-cache side storage on `assets_10k`; it also regressed `query_assets_10k` to 22.58M q/s and `query_many_keys_last` to 4.62M q/s. The direct `parse_int()` path was restored. |
| Scalar byte-class table | Rejected. A constexpr 256-entry character-class table replaced the branchy scalar `is_space`, `is_digit`, delimiter, and string-special predicates in `sexp.cpp`. It did not produce the intended parse win: `assets_10k` measured 246.94 MiB/s, `strings_escaped_5k` 721.02 MiB/s, `deep_1k` 201.88 MiB/s, and `code_forms_2k` 263.14 MiB/s, with mixed query noise. The direct branch predicates were restored. |
| Larger mixed asset database fixture | Kept. The benchmark suite now includes `asset_database_20k` and `yyjson_asset_database_20k` so parser changes are measured against a larger generated asset database, not only 5k mixed records and the simpler asset list. First measured result: `gsexp` parsed 2,968,115 bytes at 333.87 MiB/s with about 17.81 MB retained, while yyjson parsed its 2,990,492-byte equivalent at 783.95 MiB/s. |
| Aggressive sampled node reserve fallback | Rejected. Accepting any sampled node-reserve estimate below the legacy `source.size() / 4` reserve reduced retained memory on mixed asset databases, but the growth churn cost was too high. `asset_database_20k` retained approximate bytes dropped from about 17.81 MB to 14.93 MB, but parse throughput fell from 333.87 to 313.80 MiB/s. `asset_database_5k` fell to 286.83 MiB/s, so the stricter half-legacy fallback was restored. |
| Simple decimal float parser | Rejected. A short custom decimal parser for common float atoms tried to avoid `std::from_chars()` after the existing shape checks, with fallback for uncommon forms. It did not improve the float-heavy lookup path: `query_last_10k` measured 9.88M q/s and `query_assets_10k` 22.82M q/s, both below the kept lazy-float-cache runs. The direct `from_chars()` conversion was restored. |
| Mixed asset database query fixture | Kept. The benchmark suite now queries heterogeneous `texture`, `sound`, and `prefab` records from `asset_database_5k`, extracting head/kind, `id`, `path`, and two kind-specific fields. This gives lookup changes a non-uniform asset workload alongside the simple `(asset ...)` query. First measured comparison: `gsexp` reached 21.88M q/s with about 4.98 MB retained after the float cache, while yyjson reached 79.71M q/s on the equivalent JSON records. |
| Monotonic `FormView` scan hint | Rejected. A tiny mutable hint let repeated small-form lookups resume after the previously found child while keeping the same public API and avoiding heap allocation. A same-session A/B run showed only a small common-lookup win (`query_assets_10k` 24.10M q/s versus 23.47M q/s) and broader regressions: `query_last_10k` fell from 11.29M to 9.95M q/s, `query_missing_10k` from 14.40M to 12.91M q/s, `query_string_view_10k` from 16.39M to 15.79M q/s, `query_asset_database_5k` from 23.88M to 22.57M q/s, and wide indexed lookup cases also fell. The stateless direct scan was restored. |
| Direct-scan head pre-skip only | Rejected. A narrower retry of the earlier start-at-second-child idea removed the `first` branch only from small-form direct lookup and left wide-index construction alone. It still regressed the lookup suite versus the latest clean baseline: `query_assets_10k` fell from 23.47M to 21.67M q/s, `query_missing_10k` from 14.40M to 13.78M q/s, `query_string_view_10k` from 16.39M to 14.63M q/s, and `query_asset_database_5k` from 23.88M to 23.75M q/s. The branchy direct scan was restored. |
| Short source-atom byte switch | Rejected. Direct source atom comparison tried explicit byte checks for keys up to four bytes (`x`, `y`, `id`, `path`) before falling back to `memcmp`. It did not improve the main lookup paths: `query_assets_10k` measured 23.03M q/s versus the 23.47M baseline, `query_first_10k` fell to 14.83M q/s, `query_string_view_10k` to 14.08M q/s, and `query_asset_database_5k` to 21.79M q/s. The plain checked `memcmp` helper was restored. |
| Whitespace-run inner loop | Rejected. `skip_space_and_comments()` tried consuming whitespace runs in an inner loop before checking comments. It made the hot parser path worse on the measured run: `assets_10k` fell to 228.98 MiB/s, `asset_database_5k` to 291.99 MiB/s, `small_files_1k` to 188.72 MiB/s, `strings_plain_5k` to 1023.94 MiB/s, and `strings_escaped_5k` to 691.04 MiB/s. The simpler one-byte branch loop was restored. |
| Insertion sort for wide child indexes | Rejected. Lazy wide child-index construction tried replacing `std::stable_sort` with a local insertion sort for the common 24-entry `many_keys` indexes. It regressed the target wide lookup benchmarks: `query_many_keys_last` fell from the 7.33M baseline to 6.40M q/s, `query_find_many_keys_last` from 8.14M to 6.74M q/s, and `query_child_at_many_keys_last` from 7.42M to 6.34M q/s. The `stable_sort` path was restored. |
| Node-count-based child-index cache reserve | Rejected. The wide-index path reserved `child_indexes` capacity from `nodes.size() / 64` when the dense lookup table was first created. It reduced `query_many_keys_last` retained approximate bytes from about 12.40 MB to 12.32 MB by lowering cache capacity from 8192 to 5781, but the speed tradeoff was not good enough: `query_find_many_keys_last` fell from the 8.14M baseline to 6.73M q/s and `query_child_at_many_keys_last` from 7.42M to 6.13M q/s. The normal vector growth path was restored. |
| Raise lazy-index threshold to 32 | Rejected. Keeping 24-field forms on direct scan removed wide-index side storage and lowered `query_many_keys_last` retained approximate bytes from about 12.40 MB to 9.95 MB, but lookup throughput fell too far: `query_many_keys_last` dropped from the 7.33M baseline to 4.41M q/s, `query_find_many_keys_last` to 4.47M q/s, and `query_child_at_many_keys_last` to 4.81M q/s. The threshold returned to 16. |
| Manual indexed binary search | Rejected. Replacing `std::lower_bound` with an explicit index-based binary search improved one repeated `get_int()` wide case (`query_many_keys_last` 7.66M q/s versus the 7.33M baseline), but it regressed adjacent indexed lookup cases too much: `query_find_many_keys_last` fell to 6.79M q/s and `query_child_at_many_keys_last` to 5.68M q/s. The `lower_bound` path was restored. |
| Remove unused child-index list field | Kept. `ChildIndexCache::list` was written but never read because `child_index_lookup` already maps a list node to its cache slot. Removing it drops `sizeof(ChildIndexCache)` by 8 bytes on this build. The wide-index benchmark retained approximate bytes fell from about 12.40 MB to 12.33 MB with the same 8192 cache capacity. The second measured run kept indexed lookup in range: `query_many_keys_last` 7.72M q/s, `query_find_many_keys_last` 8.03M q/s, and `query_child_at_many_keys_last` 6.80M q/s. |
| Child-index cache byte stats | Kept. Benchmark stats now print `child_index_cache_bytes` separately from cache capacity. This makes retained memory changes to `ChildIndexCache` visible directly after the unused-list-field removal, without inferring the platform `sizeof(ChildIndexCache)`. This is measurement support, not a parser behavior change. |
| Side child-span arena with sibling links retained | Rejected. A prototype finalized each list into a flat `child_indices` arena after parse while keeping existing `next_sibling` links. It made the representation strictly larger and slower before any node repack: `assets_10k` parse fell from about 267 to 183 MiB/s, retained bytes rose by about 1.36 MB, `iterate_assets_10k` fell from about 43.7M to 37.3M visits/s, and wide lookup regressed (`query_many_keys_last` 7.70M to 6.09M q/s). A child-span design only makes sense if it replaces sibling links during construction rather than layering a second child representation on top. |
| Flat arena-backed child-index entries | Kept. Lazy wide-form indexes now retain key entries in one storage-owned arena instead of one `std::vector` per cached form. A one-time `nodes.size() / 3` reserve keeps the arena from over-growing on the wide benchmark. The second measured run lowered `query_many_keys_last` retained approximate bytes from about 12.33 MB to 12.24 MB, with `child_index_cache_bytes=65,536` and `child_index_entry_capacity=123,334`. Wide lookup improved in that run: `query_many_keys_last` 8.50M q/s, `query_find_many_keys_last` 8.57M q/s, and `query_child_at_many_keys_last` 7.18M q/s. |
| Direct build into flat child-index arena | Rejected. Building each lazy wide index directly in the retained entry arena removed the temporary vector and copy, but it regressed the main wide lookup case after the flat-arena keep. The measured run had `query_many_keys_last` at 7.38M q/s versus the kept 8.50M q/s run, while `query_find_many_keys_last` was essentially flat at 8.59M q/s and `query_child_at_many_keys_last` improved to 7.82M q/s. The local temporary vector keeps the sort/build path faster enough to retain. |
| `std::sort` retry after flat child-index arena | Rejected. Retesting unstable sort after the child-index entry arena change did not beat the kept stable-sort path. The measured run had `query_many_keys_last` at 8.20M q/s, but `query_find_many_keys_last` fell to 7.45M q/s and `query_child_at_many_keys_last` fell to 6.32M q/s versus the kept flat-arena run at 8.57M and 7.18M q/s. `std::stable_sort` remains the better measured path. |
| Child-index entry byte stats | Kept. Benchmark stats now print `child_index_entry_bytes` separately from entry capacity. This makes retained memory changes to `KeyIndexEntry` and the flat entry arena visible directly, rather than requiring readers to multiply capacity by the platform entry size. This is measurement support, not a parser behavior change. |
| First-argument helper retest after flat index arena | Rejected. Retesting specialized first-argument helpers behind `get_*()` did not beat the shared `find_arg_*` helpers after the latest representation changes. Two measured runs showed the same shape: `query_asset_database_5k` improved to about 24.3-24.9M q/s, but common asset lookup fell to 22.9-25.0M q/s and wide indexed lookup fell too far, with the second run at `query_many_keys_last` 6.58M q/s, `query_find_many_keys_last` 6.34M q/s, and `query_child_at_many_keys_last` 6.65M q/s. The shared helper path was restored. |
| Public scan-once common asset probe retest | Rejected. A benchmark-only probe walked each asset form once through public `Node` children and extracted `id`, `x`, `y`, and `path`. It was much slower than normal `FormView` lookup on the current representation: `query_scan_once_assets_10k` measured 9.12M q/s while `query_assets_10k` measured 24.49M q/s in the same run. Public nested traversal is not the ceiling to optimize toward; any production fix needs lower-level internal state or representation changes behind `FormView`. |
| `get_string_view()` first-argument-only helper | Rejected. Isolating the first-argument specialization to `get_string_view()` still did not improve the target string-view path. The measured run had `query_string_view_10k` at 15.41M q/s versus the clean baseline around 16.55M q/s, while common asset lookup fell to 23.13M q/s and wide indexed cases also stayed below the kept baseline range. The shared `find_arg_data(..., 0)` path was restored. |
| Switch-based whitespace/comment skipper | Kept. `skip_space_and_comments()` now switches directly on the exact whitespace and comment marker bytes instead of calling `is_space()` before checking comments. Two measured runs improved the parse-oriented cases versus the refreshed clean baseline. The second run had `assets_10k` at 259.24 MiB/s, `asset_database_5k` at 310.10 MiB/s, `asset_database_20k` at 327.71 MiB/s, `strings_plain_5k` at 1113.66 MiB/s, `strings_escaped_5k` at 752.65 MiB/s, and `code_forms_2k` at 273.70 MiB/s. Query results moved within normal noise and are not the reason for keeping this parser-path change. |
| Direct atom delimiter switch helper | Rejected. Replacing the scalar atom tail's `is_delimiter()` call with a direct `switch` helper did not improve the parser hot path. The measured run regressed the main parse cases versus the switch-skipper baseline: `assets_10k` measured 225.85 MiB/s, `asset_database_5k` 287.86 MiB/s, `asset_database_20k` 273.92 MiB/s, `strings_plain_5k` 914.05 MiB/s, and `strings_escaped_5k` 689.98 MiB/s. The original helper call was restored. |
| `memchr` comment skip | Rejected. Using `std::memchr()` to skip comment bodies inside `skip_space_and_comments()` did not beat the simple byte loop. The measured run was mixed but below the kept switch-skipper parser baseline: `assets_10k` measured 250.68 MiB/s, `asset_database_5k` 295.68 MiB/s, `asset_database_20k` 301.83 MiB/s, `strings_plain_5k` 991.82 MiB/s, and `strings_escaped_5k` 729.73 MiB/s. The direct comment loop was restored. |
| Remove redundant value-form checks | Rejected. `find_child_index()` already returns child forms with valid atom heads, so `find_arg_data()` and `find_arg_index()` tried dropping the repeated child type/head check. The measured result was mixed and hurt adjacent lookup workloads: `query_assets_10k` reached 25.36M q/s, but `query_asset_database_5k` fell to 22.22M q/s, `query_many_keys_last` to 7.05M q/s, `query_find_many_keys_last` to 6.70M q/s, and `query_child_at_many_keys_last` to 6.84M q/s. The defensive checks were restored. |
| Internal storage-walk asset query probe | Kept. This is benchmark-only ceiling measurement, not a public API. The probe walks `ParseStorage` and `NodeData` directly, scans each asset form once, and extracts `id`, `x`, `y`, and `path`. The first run measured `query_internal_assets_10k` at 30.48M q/s versus normal `FormView` `query_assets_10k` at 23.53M q/s and yyjson at 127.37M q/s in the same run. After the probe was corrected to use the same lazy float cache behavior as `FormView`, it reached 38.96M q/s versus normal `FormView` at 24.34M q/s and yyjson at 97.64M q/s. The result shows repeated public lookup overhead is real but not the whole gap; retained representation, child traversal, text comparison, and conversion costs still dominate enough that a public batch/cursor API would be the wrong next step. |
| Private hot-key classifier | Rejected. Direct small-form lookup tried classifying caller keys such as `id`, `path`, `x`, `y`, and `h` once per lookup, then using fixed byte checks instead of `memcmp`. It did not improve the target path: `query_assets_10k` measured 24.10M q/s versus the corrected-probe baseline at 24.34M q/s, and `query_asset_database_5k` fell to 21.85M q/s. Wide lookup movement was mixed, so the generic checked `memcmp` path was restored. |
| Internal ordered asset query probe | Kept. This is another benchmark-only ceiling probe, not a public API. It uses the fixed generated asset fixture field order to extract `id`, `path`, `x`, and `y` by sibling position without key matching. The first measured run had normal `query_assets_10k` at 24.11M q/s, internal scan-by-key at 35.72M q/s, internal ordered lookup at 55.77M q/s, and yyjson at 112.68M q/s. This confirms key matching and repeated scans are a large part of the common asset gap, but the ordered path is too record-specific to expose. The production direction remains internal form-state, key metadata, or representation changes behind normal `FormView`. |
| Positional small-form key hint | Rejected. Direct small-form lookup tried checking likely generated-asset field positions for `id`, `path`, `x`, `y`, and `h`, verifying the hinted child head, then falling back to the normal scan on miss. The measured run did not justify the schema-shaped heuristic: `query_assets_10k` was only 24.33M q/s, `query_first_10k` fell to 15.35M q/s, `query_missing_10k` fell to 13.03M q/s, and `query_asset_database_5k` fell to 21.98M q/s. The plain direct scan was restored. |
| Direct `FormView::find_arg()` implementation | Kept. `find_arg()` now uses the existing internal `find_arg_index()` helper instead of constructing a temporary `FormView(find(...))` and then calling `arg()`. This preserves the public API and avoids extra `Node`/`FormView` traversal on nested form lookups. The measured mixed asset database query improved to 25.29M q/s on the first run, while common asset lookup stayed in range at 24.30M q/s. |
| Unified value-index helper for `get_*()` | Rejected. `get_int()`, `get_string()`, and `get_string_view()` tried using the index-returning `find_arg_index()` path so all `get_*()` methods shared one value lookup helper. It removed the pointer-returning helper but regressed the measured lookup suite: `query_assets_10k` fell to 23.98M q/s and `query_asset_database_5k` fell to 23.33M q/s. The pointer-returning helper was restored for string and int access. |
| Wide `find_arg()` query benchmark | Kept. The benchmark suite now reports `query_find_arg_many_keys_last`, measuring `FormView::find_arg("key_23", 0)` on the same wide records used by `query_find_many_keys_last` and `query_child_at_many_keys_last`. First measured run: `find_arg` reached 7.86M q/s, compared with `find()` at 7.29M q/s and `find()+child_at(1)` at 7.62M q/s. This is measurement support for the direct `find_arg()` implementation, not a new parser behavior. |
| Lazy atom-head hash side metadata | Rejected. Direct small-form lookup tried adding a storage-owned `atom_hash_cache` and comparing cached FNV-1a hashes before text comparison. This kept the public API unchanged but made the main targets worse: `query_assets_10k` fell to 17.77M q/s and `query_asset_database_5k` fell to 20.82M q/s, while `assets_10k` query stats gained about 1.36 MB of hash-cache capacity. The normal size check plus direct `memcmp` path was restored. |
| Manual checked integer parser | Kept, with mixed results. `get_int()` now uses a small decimal parser instead of `looks_like_integer()` plus `std::from_chars`, while preserving plus/minus, suffix rejection, sign-only rejection, and overflow behavior covered by tests. Two measured runs improved the common asset query to 23.94M and 24.34M q/s versus the previous documented 23.47M q/s, and `query_first_10k` reached 15.87M and 15.53M q/s versus 15.05M. The mixed asset database query was lower at 21.41M and 22.57M q/s versus the previous 23.88M, so this remains a watch item for later A/B runs. |
| Storage-owned integer conversion cache | Rejected. A lazy `int_cache` mirroring the float cache avoided repeated parsing for `get_int()`, but the memory and lookup cost were not acceptable. `query_assets_10k` improved to 25.61M q/s, but `query_first_10k` fell to 15.18M q/s, `query_asset_database_5k` fell to 19.45M q/s, `query_many_keys_last` fell to 6.83M q/s, and `assets_10k` query stats gained 2.72 MB of integer-cache capacity. The cache was removed and the manual parser path restored. |
| Table-driven scalar character classification | Rejected. Parser hot loops tried replacing explicit whitespace, delimiter, and string-special checks with a 256-entry constexpr classification table. The measured parse cases regressed enough to reject it: `assets_10k` was 237.12 MiB/s versus the current documented 250.17 MiB/s, `asset_database_5k` was 293.19 MiB/s versus 319.38 MiB/s, `strings_plain_5k` was 1045.16 MiB/s versus 1112.12 MiB/s, and `wide_10k` was 376.20 MiB/s versus 384.71 MiB/s. The explicit branch checks were restored. |
| Nested `find_arg()` benchmark fixture | Kept. The benchmark suite now includes `query_nested_find_arg_5k`, a layout-like fixture with child forms such as `(title label x y w h)` and `(play label x y w h)`. The query uses normal `FormView::find_arg()` plus one `get_int()` and measured 24.19M q/s on the first run. This is measurement support for Plan 11 nested lookup work, not a new public API. |
| Unrolled common `find_arg()` argument indexes | Rejected. `find_arg()` and `get_*()` tried replacing the small argument-walk loop with explicit fast paths for argument indexes 0 through 4, falling back to the loop above that. The nested layout benchmark barely moved, from the recorded 24.19M q/s to 24.31M q/s, while `query_find_arg_many_keys_last` measured only 7.09M q/s and the helper added a large branchy block. The simple loop was restored. |
| Lower lazy child-index threshold to 8 | Rejected. Lowering `indexed_child_threshold` from 16 to 8 forced asset-sized forms onto the existing lazy index path. It was much worse for the target workload: `query_assets_10k` fell to 14.13M q/s, `query_first_10k` fell to 9.52M q/s, and the `query_assets_10k` stats built 10k child indexes with retained approximate bytes rising from about 9.62 MB to 11.79 MB. The threshold was restored to 16. |
| Internal nested layout query probe | Kept. This is benchmark-only ceiling measurement for the new layout-like nested `find_arg()` fixture. It walks the known generated layout field order directly through `ParseStorage`/`NodeData`, then reads the same values as `query_nested_find_arg_5k`. First measured run: public nested `find_arg()` reached 23.84M q/s while the internal ordered probe reached 88.30M q/s. This shows the layout-like workload still has a large lookup/traversal gap available behind the normal `FormView` API. |
| Query benchmark helper split | Kept. `benchmarks/parse_bench.cpp` had grown to 795 lines, making Plan 11 benchmark edits harder to audit. Query and traversal benchmark helpers were moved to `benchmarks/query_bench.hpp`, leaving `parse_bench.cpp` at 358 lines and the new query helper at 496 lines. This is a maintainability change only; it does not change benchmark behavior. |
| First/last byte guard before atom `memcmp` | Rejected. Direct atom comparison tried checking the first and last byte after size/bounds checks before calling `memcmp`. The extra branches did not help target workloads: `query_assets_10k` fell to 22.49M q/s, `query_nested_find_arg_5k` fell to 22.44M q/s, and `query_internal_nested_find_arg_5k` fell to 73.71M q/s versus the recorded 88.30M q/s. The plain checked `memcmp` path was restored. |
| Cached `FormView` internals | Rejected. `FormView` stored the parse storage pointer, node data pointer, and node index at construction time to avoid repeated `Node::data()` and storage extraction in lookup methods. It built in `gsexp` and `glayout`, but the measured run did not justify the larger `FormView`: `query_find_arg_many_keys_last` improved to 8.24M q/s, while `query_first_10k` fell to 12.89M q/s, `query_last_10k` fell to 9.29M q/s, and `query_nested_find_arg_5k` stayed below the recorded baseline at 23.44M q/s. The single-`Node` `FormView` representation was restored. |
| Contiguous child-index arena beside sibling links | Rejected. The first child-span attempt added a storage-owned `child_indices` arena and changed `Node`, `FormView`, and internal probes to read direct children from contiguous slots while keeping `next_sibling` for the existing public sibling API. It built and passed tests, but it was too expensive because it layered a second child representation onto the current one: `assets_10k` fell to 107.75 MiB/s, `asset_database_5k` fell to 132.12 MiB/s, `query_assets_10k` fell to 19.96M q/s, `query_asset_database_5k` fell to 18.18M q/s, and `query_nested_find_arg_5k` fell to 21.13M q/s. `assets_10k` also gained about 1.44 MB of child-index arena capacity. The arena was removed; a future child-span attempt must replace sibling storage during construction rather than sit beside it. |
| Generic small-atom compare switch | Rejected. `source_atom_text_equals()` tried direct character comparisons for atom heads of length 1 through 4 before falling back to `memcmp`, targeting common keys such as `x`, `y`, `id`, and `path` without adding a caller-key API or schema hint. The measured result was mixed and not worth the extra branches: `query_assets_10k` was in range at 24.35M q/s and `query_string_view_10k` reached 16.44M q/s, but `query_nested_find_arg_5k` fell to 22.15M q/s, `query_asset_database_5k` was only 22.22M q/s, and wide indexed lookups stayed around 6.4-6.5M q/s. The plain checked `memcmp` path was restored. |
| Direct append while building sorted child indexes | Rejected. The lazy wide-form index builder tried appending `KeyIndexEntry` items directly into `ParseStorage::child_index_entries` and sorting that new slice, avoiding the temporary vector and final insert copy. It improved one wide indexed case, with `query_find_many_keys_last` at 7.62M q/s, but hurt the main workloads: `query_assets_10k` fell to 22.92M q/s, `query_asset_database_5k` was 23.61M q/s, and `query_nested_find_arg_5k` fell to 21.40M q/s. The temporary-vector builder was restored because the direct arena mutation is not a clear win. |
| Repeated-lookup small-form cache | Rejected. A stricter storage-owned small-form cache built only for forms below the wide-index threshold after the second lookup on that form. It remained hidden behind the normal `FormView` API and reported cache memory in `StorageStats`, but the cost was too high: `query_assets_10k` built 10,000 caches and fell to 17.81M q/s while retained approximate bytes grew from about 9.62 MB to 11.79 MB, `query_string_view_10k` fell to 14.46M q/s, `query_asset_database_5k` reached only 21.45M q/s, and `query_nested_find_arg_5k` fell to 20.64M q/s. The cache and stats fields were removed. |
| Scalar prefix before string SIMD scan | Rejected. `find_string_special()` tried checking the first eight bytes scalarly before entering the SSE2 quote/backslash/newline scan, separate from the already rejected atom scalar-prefix attempt. The measured result was worse on the target string cases and some general parse cases: `strings_plain_5k` fell to 977.69 MiB/s, `strings_escaped_5k` fell to 557.29 MiB/s, `asset_database_5k` fell to 266.58 MiB/s, and `code_forms_2k` measured 252.66 MiB/s. The direct SSE2 loop was restored. |
| `parse_value()` switch dispatch | Rejected. Replacing the ordered `if` chain in `parse_value()` with a `switch` on the current byte did not improve the parser hot path. The measured run regressed important parse fixtures: `strings_plain_5k` fell to 899.04 MiB/s, `code_forms_2k` fell to 227.63 MiB/s, `wide_10k` fell to 326.95 MiB/s, `asset_database_20k` fell to 278.34 MiB/s, and `assets_10k` was only 239.49 MiB/s. The original branch order was restored. |
| Source-only `Node::is_atom()` helper | Rejected. `Node::is_atom()` tried using a source-backed atom comparison helper with bounds checks, matching the fact that parsed atoms are source-backed and avoiding the generic decoded-text branch. The measured query result was mixed and not worth specializing this public traversal path: `query_symbol_compare_10k` improved to 45.17M q/s and `query_asset_database_5k` reached 23.67M q/s, but `query_string_view_10k` fell to 13.49M q/s, `query_nested_find_arg_5k` fell to 21.31M q/s, and traversal fell to 42.29M visits/s. The generic checked text helper was restored. |
| Larger mixed asset database query fixture | Kept. The benchmark suite now runs `query_asset_database_20k` and `yyjson_query_asset_database_20k`, using the existing 20k generated mixed asset database and the same total 5M query count as the 5k query case. First measured run: `gsexp` reached 21.17M q/s with about 20.07 MB retained after float cache allocation, while yyjson reached 77.52M q/s on the equivalent JSON fixture. This is measurement support for Plan 11 lookup work, not a parser behavior change. |
| Internal mixed asset database query probe | Kept. The benchmark suite now includes `query_internal_asset_database_5k` and `query_internal_asset_database_20k`, benchmark-only probes that scan each mixed asset record once through `ParseStorage`/`NodeData` and extract the same fields as the public `FormView` query. First measured run: public `query_asset_database_5k` reached 21.47M q/s while the internal scan reached 44.78M q/s; public `query_asset_database_20k` reached 20.77M q/s while the internal scan reached 33.77M q/s. This confirms repeated `FormView` lookup remains a major mixed-database bottleneck, but the 20k ceiling also shows retained traversal and conversion costs still matter. |
| Inline small-form value cache in `FormView` | Rejected. `FormView` tried a fixed inline cache of up to 15 small-form head/value pairs, populated on first lookup and used by normal `find_arg()`/`get_*()` calls without changing the public API or allocating heap memory. It did not justify the larger view or upfront cache fill: `query_assets_10k` measured 23.27M q/s and `query_asset_database_5k` measured 21.46M q/s, while single-key paths regressed badly with `query_first_10k` at 9.61M q/s, `query_last_10k` at 8.45M q/s, and `query_string_view_10k` at 11.10M q/s. The single-`Node` `FormView` representation was restored. |
| `FormView` resume-child hint | Rejected. `FormView` tried storing the last matched child node and starting the next small-form lookup after that child, wrapping once to preserve normal lookup semantics. This helped ordered repeated access, with `query_nested_find_arg_5k` improving to 27.03M q/s and `query_asset_database_5k` to 23.16M q/s, but it regressed broad single-key lookups and common lookup enough to reject: `query_first_10k` fell to 13.64M q/s, `query_last_10k` to 9.36M q/s, and `query_assets_10k` to 23.41M q/s versus current results of 15.53M, 11.35M, and 24.34M. The stateless `FormView` lookup path was restored. |
| Prepared direct atom comparison state | Rejected. Direct small-form lookup tried precomputing the caller key pointer/size and source size once per scan, then comparing candidate atom heads through that prepared state. It improved some mixed lookup noise, with `query_asset_database_5k` at 23.37M q/s, but did not beat the broader current lookup baseline: `query_assets_10k` was 24.32M q/s versus 24.34M, `query_string_view_10k` fell to 15.96M q/s versus 16.77M, and `query_nested_find_arg_5k` fell to 23.02M q/s versus 24.19M. The previous direct comparison helper was restored. |
| List head metadata in `text_size` | Rejected. List nodes tried storing their first child/head index in the otherwise-unused public text-size field while keeping `text_offset` as the parse-time tail. This avoided reading `first_child` in some `FormView` paths without widening `NodeData`, but it regressed the broader lookup suite: `query_assets_10k` fell to 21.54M q/s, `query_string_view_10k` to 13.85M q/s, and `query_nested_find_arg_5k` to 19.26M q/s. The mixed asset database query improved to 24.53M q/s, but the tradeoff was too narrow, so the normal `first_child` head source was restored. |
| Reuse initial escaped-string scan | Kept. `parse_string()` already scanned the first plain chunk to decide whether a string could stay source-backed; escaped strings then scanned that same chunk again inside the decode loop. The parser now copies the first scanned chunk into `decoded_text` before entering the loop, avoiding that duplicate scan without changing representation or API. The measured parser cases stayed healthy: `strings_escaped_5k` improved from 738.99 to 747.18 MiB/s, `strings_plain_5k` measured 1132.27 MiB/s, `assets_10k` 257.58 MiB/s, and `code_forms_2k` 261.77 MiB/s. |
| Skip string scan on immediate special byte | Rejected. After reusing the first escaped-string scan, the decode loop tried checking `is_string_special(text[index])` before calling `find_string_special()` so it could avoid rediscovering a known quote/backslash/newline. The extra scalar branch hurt the parser broadly: `strings_escaped_5k` fell to 642.98 MiB/s, `strings_plain_5k` to 938.77 MiB/s, `assets_10k` to 234.82 MiB/s, and `code_forms_2k` to 229.73 MiB/s. The direct scanner call was restored. |
| Direct switch for atom scalar tail delimiters | Rejected. The scalar tail after the SSE2 atom scan tried replacing `is_delimiter()`/`is_space()` with one direct `switch` over whitespace and parentheses. This isolated tail-only variant still regressed atom-heavy parse cases badly: `assets_10k` fell to 223.80 MiB/s, `asset_database_5k` to 274.80 MiB/s, `asset_database_20k` to 270.73 MiB/s, and `code_forms_2k` to 232.52 MiB/s. The original helper path was restored. |
| Insertion sort for lazy wide child indexes | Rejected. Wide-form index construction tried replacing `std::stable_sort()` with a simple insertion sort over the temporary `KeyIndexEntry` vector, targeting the small fixed-ish index sizes in `many_keys_last`. The measured result was too mixed: `query_many_keys_last` reached 7.48M q/s and `query_find_arg_many_keys_last` 8.15M q/s, but `query_find_many_keys_last` fell to 6.45M q/s and `query_child_at_many_keys_last` to 5.62M q/s versus current results of 8.12M and 7.54M. The stable sort builder was restored. |
| Variable-width many-key lookup benchmarks | Kept. The benchmark suite now measures `get_int()` of the last key for 8, 16, 24, and 48-key records, making lazy-index threshold changes easier to evaluate. First measured run: `query_many_keys_8_last` reached 10.91M q/s with no child indexes, `query_many_keys_16_last` reached 9.28M q/s while building 5k indexes and 80k entries, `query_many_keys_24_last` reached 7.02M q/s with 120k entries, and `query_many_keys_48_last` reached 4.14M q/s with 240k entries. This is measurement support, not a parser behavior change. |
| Raise lazy child-index threshold to 24 | Rejected. The variable-width benchmark suggested checking whether 16-key forms should stay direct to avoid index memory. Raising `indexed_child_threshold` from 16 to 24 did remove the 16-key child indexes and lowered that case's retained memory from about 8.21 MB to 6.65 MB, but lookup throughput fell too much: `query_many_keys_16_last` dropped from 9.28M to 7.20M q/s, `query_many_keys_24_last` stayed below the recorded 7.02M at 6.86M q/s, and `query_many_keys_48_last` fell from 4.14M to 3.80M q/s. The threshold was restored to 16. |
| Hybrid linear lookup for small child indexes | Kept. Indexed forms with 16 or fewer indexed entries now scan the sorted cache linearly and larger indexed forms keep the binary-search path. First measured run: `query_many_keys_16_last` improved from 9.28M to 9.71M q/s, `query_find_many_keys_last` from 8.12M to 8.80M q/s, `query_child_at_many_keys_last` from 7.54M to 7.86M q/s, and `query_find_arg_many_keys_last` from 7.35M to 7.70M q/s. `query_many_keys_48_last` fell from 4.14M to 3.56M q/s even though that case still uses binary search, so large indexed forms remain a watch item for repeat runs. |
| First-vs-repeated lookup transition benchmarks | Kept. The benchmark suite now reports cold first-pass and warmed repeated lookup rows for common asset queries and 16-key indexed forms. First measured run: `query_assets_10k_cold_once` reached 16.70M q/s and `query_assets_10k_warm_repeated` reached 20.78M q/s, while the normal blended `query_assets_10k` row reached 24.41M q/s. The wide-index warmup cost is much clearer: `query_many_keys_16_cold_once` reached 1.39M q/s while `query_many_keys_16_warm_repeated` reached 8.63M q/s. This is measurement support for future stateful lookup work, not a public API change. |
| Adaptive build-on-second wide index | Rejected. Wide forms tried direct-scanning the first lookup and building the lazy index only on a later lookup, tracked by a storage-owned byte vector. It helped the target cold case: `query_many_keys_16_cold_once` rose from 1.39M to 6.88M q/s, and `query_many_keys_last` rose from 7.58M to 8.28M q/s. The broader indexed suite did not justify the extra memory or policy: `query_many_keys_48_last` fell from 4.21M to 3.45M q/s, `query_find_arg_many_keys_last` fell from 7.65M to 6.90M q/s, and each indexed document gained a dense `child_index_seen` byte vector. The eager first-lookup index build was restored. |
| Node-index integer extraction path | Rejected. `get_int()` tried using the same `find_arg_index()` node-index path as `get_float()` so future numeric metadata could key consistently by atom node. The measured run did not justify the extra index handoff: `query_assets_10k` rose from 24.41M to 25.50M q/s, but `query_first_10k` fell from 15.73M to 13.58M q/s, `query_many_keys_16_last` fell from 9.63M to 9.11M q/s, `query_many_keys_last` fell from 7.58M to 6.81M q/s, and `query_nested_find_arg_5k` fell from 24.47M to 21.32M q/s. The direct `find_arg_data()` pointer path was restored. |
| Ordered code-form traversal benchmark | Kept. The benchmark suite now reports `iterate_code_forms_2k`, a non-record ordered traversal probe over the generated code-shaped form fixture. It walks `first_child()` and `next_sibling()` in source order without named lookup. First measured run: 32.40M visits in 0.852397 seconds, or 38.01M visits/s, with 162,016 nodes and about 4.20 MB retained. This is measurement support for future child-span work on code-like and tuple-like data, not a parser behavior change. |
| Internal ordered code traversal probe | Kept. The benchmark suite now reports `iterate_internal_code_forms_2k`, which walks the same code-shaped fixture through `ParseStorage`/`NodeData` indices instead of public `Node` wrappers. First measured run: public ordered traversal reached 38.21M visits/s, while the internal probe reached 231.52M visits/s over the same 32.40M visits. This is ceiling evidence for child-span/public traversal cleanup, not a new public API. |
| Open-coded `Node` traversal validity checks | Rejected. `Node::first_child()`, `next_sibling()`, and `children()` tried open-coding their validity checks instead of calling `data()`. The measured run improved the new ordered code traversal probe from 38.01M to 40.14M visits/s and `iterate_assets_10k` from 43.72M to 45.38M visits/s, but broader lookup moved the wrong way: `query_assets_10k` fell from 24.41M to 21.92M q/s, `query_string_view_10k` fell from 17.01M to 16.32M q/s, `query_find_arg_many_keys_last` fell from 7.65M to 7.00M q/s, and `query_nested_find_arg_5k` fell from 24.47M to 22.84M q/s. The simpler shared `data()` path was restored. |
| Benchmark-only child-span traversal probe | Kept. The benchmark suite now builds a temporary `node -> child range` arena after parsing code-shaped forms and reports `iterate_child_span_code_forms_2k`. First measured run: public ordered traversal reached 38.95M visits/s, direct internal sibling traversal reached 218.26M visits/s, and temporary child-span traversal reached 205.80M visits/s over the same 32.40M visits. Building the span arena took 0.001072 seconds and used about 1.94 MB. This shows contiguous child ranges can close most of the public traversal gap, but a real representation rewrite must avoid adding side storage that is slower than direct internal sibling traversal. |

Current pending Plan 11 queue:

1. Keep one public consumption API.
   Users should parse once, keep the `ParseResult` alive, then use `Node` for
   raw traversal and `FormView` for form lookup. Do not add a public
   scan-once, cursor, batch, or compiled-key API just to win a benchmark until
   internal state behind `FormView` has been exhausted.

2. Make `FormView` fast without making callers think about it.
   `FormView` may hold tiny mutable state or a cache handle, and
   `ParseStorage` may hold form-state tables, hot-key tables, and lazy indexes.
   The normal call site should still be `form.get_int("id")`,
   `form.get_float("x")`, and `form.get_string_view("path")`.
   `FormView` should not stay stateless by principle; it should only stay
   stateless if measured stateful designs fail.

3. Revisit storage-owned hot-form state with stricter triggers.
   Previous inline and storage-owned small-form caches were rejected because
   they were too eager or too costly. Retry only designs that build after
   repeated lookup on the same form, are gated by child count, avoid per-view
   heap allocation, and report memory in `StorageStats`.

4. Try a `FormView` cache handle instead of repeated storage lookup.
   If storage-owned form state is useful, let a `FormView` remember the small
   cache id for its node. The state must not make `FormView` ownership or
   lifetime surprising, and it must not require a second public type.

5. Add an internal hot-key identity cache.
   Repeated caller keys such as `id`, `path`, `x`, and `y` can be resolved to
   storage-owned key ids or hashes inside `FormView::get_*`. This should remain
   hidden unless benchmarks prove public compiled keys are necessary.

6. Retry symbol/head interning with a compact design.
   The unordered-map, node-indexed side-vector attempt was too expensive.
   Future attempts should target only form heads or repeated atom keys, avoid
   widening every node, and compare parse cost against lookup wins.

7. Replace sibling links only if the replacement is real.
   The side child-span arena was rejected because it layered a second child
   representation on top of sibling links. A future child-span attempt should
   replace sibling traversal during construction or be skipped.

8. Consider a parse-time tape or one-pass retained builder.
   A tape/event builder is allowed if it removes finalization cost or enables a
   better retained representation. Reject it if it merely adds another pass or
   another representation.

9. Keep scanner changes small and isolated.
   Remaining parser hot-loop candidates are direct delimiter tests in atom
   scanning, table-driven byte classification, optional SIMD quote/backslash
   search for long strings, and reserve-policy tuning. Each should be measured
   separately and reverted if it is only clever.

10. Add private ceiling probes only when they answer a specific question.
    The public scan-once probe was slower than normal `FormView`; future probes
    should use internal node indices or storage access to estimate real
    implementation ceilings, not propose another public API.

Work order:

1. Contiguous child-span arena.
   Replace sibling-chain traversal with a child-index arena. Each list node
   should identify its direct children with `first_child_slot + child_count`,
   where `child_indices[first_child_slot + i]` gives the child node index.
   Keep `Node::children()`, `Node::child_at()`, and `FormView` behavior stable.
   Measure parse cost, retained memory, child iteration, and lookup.

2. Node layout after child spans.
   Once `next_sibling` is no longer needed, repack `NodeData`. Target hot fields:
   type, text storage, text offset/size, first child slot, child count, and any
   head/symbol metadata proven useful. Keep fields debugger-readable; avoid
   clever bit packing until plain packing is measured.

3. Parser construction strategy.
   Try the simplest child-span construction first: keep a parse-time stack of
   child slots or temporary direct-child vectors, then finalize each list into
   the child-index arena. Reject designs that make parser control flow hard to
   audit unless they produce large wins.

4. Form head metadata.
   Store each list form's head node or head metadata directly when practical.
   `FormView::find()` should not have to call `child.head()` repeatedly if the
   child form's head is already known.

5. Symbol/head interning.
   Intern repeated atom heads such as `id`, `path`, `x`, and operators in
   code-like input. Start with form heads only, not arbitrary string values.
   Lookup should compare symbol IDs before falling back to text. Measure memory
   impact and parse cost.

6. Caller key resolution.
   Decide how `FormView::get_int("id")` resolves `"id"` to the interned symbol.
   Try parse-storage lookup by string view first. Do not add a complex public
   symbol API unless repeated caller-side key resolution becomes a proven
   bottleneck.

7. Flat arena-backed form indexes.
   Replace vector-of-vector child indexes with flat arrays if child spans and
   symbol IDs make that useful:
   `indexed_lists[]`, `index_entries[]`, and ranges into `index_entries`.
   Keep indexes lazy unless eager indexing is proven better.

8. Direct small-form lookup.
   For small forms, compare direct span scan against lazy index construction.
   The policy should stay simple: direct below a threshold, indexed above it,
   or no index if symbol-ID scans are already fast.

9. Stateful `FormView` internals without a second public API.
   Revisit state after the rejected per-view heap-vector cache and resume-hint
   attempts. The target is still one user-facing API: `form.get_int("id")`,
   `form.get_float("x")`, and similar calls. Try tiny inline state for one
   active form, a storage-owned form-state table keyed by node index, and fixed
   small-form indexes. Reject designs that allocate per view, require users to
   call a separate batch/cursor API, or make `FormView` lifetime surprising.

10. Scan-once small-form benchmark probe.
    Add a benchmark-only path that walks each asset form once and extracts
    `id`, `x`, `y`, and `path`. This is a ceiling test for the repeated-scan
    bottleneck, not a proposed public API. If it closes most of the yyjson gap,
    optimize normal `FormView::get_*` calls using internal state.

11. Storage-owned hot-form state.
    If repeated calls on one form are the lookup bottleneck, cache only the
    resolved head/value information needed by that form in `ParseStorage`.
    Candidate shapes are a small direct-mapped table keyed by node index, a
    compact side table populated after the second lookup on a form, or a
    threshold-gated sorted mini-index. Keep allocation lazy and visible in
    benchmark memory stats.

12. Stateful `FormView` cache handle.
    Let `FormView` cheaply remember that its form has a storage-owned state
    entry. This is still one public API because callers only construct
    `FormView` and call `get_*`. Measure whether avoiding repeated lookup-table
    probes helps the common multi-field asset query.

13. Adaptive small-form policy.
    Direct scans are good for one-off lookups, while mini-indexes may help
    repeated lookups on the same form. Try switching only after the second or
    third lookup on a form. Reject if the one-off benchmarks or retained memory
    get worse enough to outweigh the common asset case.

14. Fixed small-form state with field count gating.
    Re-test the small-form cache with a stricter trigger: only forms whose child
    count matches asset-like records and only after repeated lookups. Avoid the
    rejected eager/every-form cache shape.

15. Hot key identity cache.
    Track a tiny storage-owned mapping for repeated caller keys such as `id`,
    `path`, `x`, and `y`. Keep it internal to `FormView::get_*`; do not expose
    compiled keys unless repeated string-key resolution is proven to dominate.

16. Per-document caller-key cache.
    Resolve caller string keys to storage-local key ids or hashes the first time
    they are used. Reuse that resolution from normal `FormView::get_*` calls
    without requiring a public compiled-key object. Reject if map lookup costs
    more than direct string comparison.

17. Atom hash side metadata.
    Try storing a cheap hash for repeated atom heads in a side table or compact
    symbol table. Use hash equality as a fast reject before text comparison,
    and keep full text comparison for correctness. Do not widen every node until
    the side-table version proves useful.

18. Nested form lookup fast path.
    Measure and optimize the common `find_arg()` shape used by layout-like and
    code-like data, where a form has a named child form and callers want one of
    that child form's arguments. Keep it as `FormView::find_arg(...)`, not a new
    cursor or batch API.

19. Form-state memory accounting.
    Any stateful lookup structure must report capacity and byte usage in
    `StorageStats`, like child indexes and float caches. Hidden memory growth is
    not an acceptable speed win.

20. Numeric parse specialization.
    Try small custom parsers for common integer and simple decimal float shapes.
    Keep exact rejection behavior covered by tests. Reject if `from_chars` is
    still faster or if correctness gets harder to reason about.

21. Lazy numeric metadata.
    Preserve atom-first semantics, but try storage-owned numeric classification
    or conversion caches so repeated `get_int()` and `get_float()` do not
    re-validate and re-convert the same atom text. Keep this behind the normal
    `FormView::get_*` API.

22. Scanner success-path rewrite.
    Tighten whitespace/comment/atom/string scanning after the representation
    change. Separate syntax scanning from tree construction enough that hot
    loops stay simple. Preserve diagnostics.

23. SIMD integration.
    Integrate the existing SSE2 delimiter/string scan proof only after scalar
    representation changes settle. SIMD remains optional with scalar fallback.
    Benchmark parse cases and compile portability.

24. Allocation discipline.
    Reduce growth churn across nodes, child indices, decoded text, symbol table,
    and lazy indexes. Prefer flat arenas over per-list heap allocations.

25. Real workload fixtures.
    Add larger and more realistic generated asset database cases, and later
    real project fixtures when available. Keep synthetic yyjson comparisons,
    but do not optimize solely for synthetic records.

26. Code-like workload fixtures.
    Add fixtures with operator-like heads, mixed arity, nested forms, and
    non-record list shapes. Asset records are an important target, but `gsexp`
    should not become specialized only for `(key value)` records.

27. API audit after internals settle.
    Keep `Node` and `FormView` public behavior stable. Only add public API if
    benchmarks prove users need explicit symbol handles or caller-side compiled
    keys.

28. Stateful `FormView` must be cache state, not caller ceremony.
    A stateful view is acceptable only if the same public calls get faster.
    Prefer small storage handles, per-document key caches, or form-state ids
    hidden behind `FormView`. Do not require users to choose a different
    reader, cursor, or prepared query path for normal asset loading.

29. Do not assume every form is a record.
    Many lists may be code forms, n-ary forms, or positional tuples with more
    than two values. Any form-state or index design must preserve ordered child
    traversal and `find_arg()` semantics instead of assuming only `(key value)`
    pairs.

30. Separate ordered access from named access internally.
    Code-like forms often want child order, while asset-like forms often want
    named lookup. A future representation can cache both direct child spans and
    optional name indexes, but neither should make the other path slower.

31. Benchmark cache warmup explicitly.
    Lazy state can look good after it is built and bad on the first lookup.
    Keep first-use, repeated-use, and mixed-use cases in the benchmark output
    before keeping any stateful design.

Extended experiment queue:

1. Flat index-entry arena.
   Replace `std::vector<KeyIndexEntry>` inside each lazy child index with one
   shared `child_index_entries` arena plus `first_entry + entry_count` ranges.
   This should reduce retained heap objects for wide-form workloads. Keep only
   if lookup or retained memory improves without making the code harder to
   inspect.

2. Head-only side metadata.
   Store form-head metadata outside `NodeData` instead of widening every node or
   mutating parent list fields during parse. Candidate shapes are a vector
   indexed by list node or a compact table only for list nodes that have atom
   heads.

3. Head-only interning.
   Intern only form heads and operator-like atoms first. This targets lookup
   keys such as `id`, `path`, `x`, and code heads without forcing all string
   payloads through an intern table. Compare symbol-ID lookup to text lookup.

4. Caller key resolution without a second public API.
   If head interning helps, resolve caller string keys through storage-internal
   lookup inside `FormView`. Avoid exposed compiled-key handles unless measured
   repeated-call overhead proves they are necessary.

5. Stateful `FormView` as the normal API.
   Reintroduce `FormView` state only if it avoids the rejected Plan 10 shape.
   Candidate designs: fixed inline slots for a few cached key/value pairs,
   last-scan position for repeated calls, or a pointer/id into storage-owned
   cache data. This is not a second API; the public call site remains
   `form.get_int("id")`.

6. Storage-owned small-form cache.
   Keep small forms direct by default, but after repeated lookup on the same
   form, build a compact cache in `ParseStorage` keyed by form node. Avoid eager
   indexes for every form. Measure retained bytes and common one-off lookup.

7. Scan-once benchmark-only probe.
   Add a local benchmark helper that extracts multiple requested fields in one
   child scan. Use it to decide whether repeated `FormView::get_*` scans are the
   dominant `query_assets_10k` bottleneck. Do not expose it as public API unless
   the normal API cannot be optimized enough.

8. Small-form fixed index.
   For common small records, try a fixed-size stack or inline sorted array for
   head/value pairs. Keep only if it beats direct scan without heap allocation
   and without adding caller-visible state.

9. Per-form state generation.
   Add a small generation or lookup count per form node so repeated queries can
   trigger cached state lazily. This should distinguish one-off `find()` calls
   from asset-loading patterns without user involvement.

10. Caller-key identity cache.
   Cache recently used caller keys in parse storage or thread-local-free
   document state. The cache should map string bytes to key ids, atom hashes, or
   interned head ids and must remain invisible to callers.

11. Atom hash metadata.
   Add optional side metadata for atom-head hashes. Hashes are only a fast
   reject; equal hashes still compare text. Keep only if query wins cover parse
   and memory cost.

12. Lazy numeric value cache.
   Store successful numeric conversions in parse storage or compact side tables
   keyed by atom node. This should speed repeated `get_int()`/`get_float()` while
   keeping atoms as the retained source representation.

13. Flat node arena with child spans.
   Retry child spans with a cleaner construction strategy after other hot spots
   are measured. The first attempt proved that smaller nodes alone do not win
   if finalization and child lookup get slower.

14. Parser event/tape builder.
   Try a parse-time tape or event stream that records list starts, atoms, and
   closes, then builds the retained tree in one predictable pass. Reject if it
   only adds another representation without removing cost elsewhere.

15. One-pass retained builder.
   Try building final child ranges directly while parsing, using stack frames
   that own contiguous output ranges. This is harder than sibling links but may
   remove the expensive finalization pass from the rejected child-span attempt.

16. Atom scanner specialization.
   Split atom scanning into the common no-escape, no-comment, no-delimiter fast
   path and the slower diagnostic path. The hot loop should mostly advance over
   plain bytes and only branch at delimiters.

17. String scanner specialization.
   Keep plain string views when no escapes are present and decode only escaped
   strings into `owned_text`. Measure plain and escaped strings separately, as
   they have very different ceilings.

18. Whitespace and comment skipping.
    Tighten the skip loop because every benchmark pays it. Try simple
    table-driven classification before SIMD. Reject clever code if branch
    prediction already wins.

19. Numeric token classification.
    Preserve atom-first semantics, but classify likely integer and decimal
    tokens cheaply enough that `FormView::get_int()` and `get_float()` avoid
    duplicate work. Add tests before changing accepted numeric shapes.

20. Numeric conversion specialization.
    Try custom integer conversion and simple decimal float conversion for common
    asset data. Keep `from_chars` fallback for correctness and uncommon shapes.

21. Allocation reservation policy.
    Improve reserve estimates for nodes, owned text, child indexes, symbol
    tables, and lazy indexes. Track capacity in benchmark output so wins are not
    just hidden memory growth.

22. Small-vector avoidance.
    Avoid per-form heap allocation in lazy indexes and temporary builder data.
    Prefer flat arenas or fixed-size stack buffers where the measured workload
    repeatedly builds small structures.

23. Cache threshold tuning.
    Re-test the direct-scan versus lazy-index threshold after each
    representation change. The correct threshold may move once symbol IDs or
    flat indexes exist.

24. Common asset lookup benchmark.
    Keep the current lookup benchmarks, but add fixtures that are not only
    record-shaped. The parser may later handle code-like input, so optimizing
    only `(asset (id ...) ...)` would be too narrow.

25. Larger generated fixtures.
    Add larger asset databases, mixed strings, mixed numeric fields, deep forms,
    wide forms, and code-like forms. Compare both parse throughput and query
    throughput against yyjson equivalents where a JSON shape makes sense.

26. Nested lookup fixtures.
    Add mixed nested data that exercises `find_arg()` and child-form access
    without assuming all lists are two-item records.

27. Real fixture import.
    Add real project data once available. Synthetic data is useful for drag
    races, but final decisions should also use files that resemble actual game
    assets and tool output.

28. Form-state transition benchmark.
    Add a fixture that measures the same form through first lookup, second
    lookup, and several repeated lookups. Use it before retrying adaptive
    small-form caches so the threshold is based on data, not guesses.

29. Ordered child-span probe for non-record forms.
    Add a benchmark-only ordered traversal probe for code-like and tuple-like
    forms. This should show whether child spans help ordered access even when
    named lookup is not involved.

30. Mixed lookup pattern fixture.
    Add a fixture that alternates ordered child access, named `get_*`, and
    `find_arg()` on the same document. Reject caches that only improve one
    narrow pattern while hurting the others.

31. Larger wide-form index repeats.
    Re-run 16, 24, and 48-key lookup cases with cold first lookup separated
    from repeated lookup. The current 48-key result is noisy enough that the
    indexed lookup policy needs another pass before larger changes.

32. SIMD delimiter scan.
    Add optional SSE2 delimiter scanning for atom and whitespace-heavy input
    after scalar scanner changes settle. Keep scalar fallback compiled and
    benchmarked.

33. SIMD string scan.
    Add optional SIMD quote/backslash detection for long plain strings. This
    should target the plain string benchmark first and must not slow short
    strings.

34. Memory layout audit.
    Recheck `sizeof(NodeData)`, vector capacities, and retained bytes after each
    kept change. A parse win that bloats retained memory needs a workload-based
    justification.

35. Error-path audit.
    Keep diagnostics useful while moving hot loops. Fast success paths are fine,
    but malformed input should still report practical file offsets and reasons.

36. Public API audit.
    After internals settle, remove accidental exposure of implementation details
    and document the one normal consumption path. Avoid compatibility wrappers
    or parallel public lookup APIs because there are no external consumers yet.

37. Glayout integration check.
    Build `glayout` after any API or vendoring-impacting change. `glayout` is
    the current real consumer and should catch usability regressions earlier
    than synthetic benchmarks.

Candidate representation sketches:

1. Child-span node:

```cpp
struct NodeData {
    uint32_t text_offset;
    uint32_t text_size;
    uint32_t first_child_slot;
    uint32_t child_count;
    uint32_t head_symbol;
    ValueType type;
    TextStorage text_storage;
};
```

2. Child arena:

```cpp
std::vector<uint32_t> child_indices;
```

3. Symbol table:

```cpp
struct Symbol {
    uint32_t text_offset;
    uint32_t text_size;
    uint32_t hash;
};
```

Acceptance rules:

1. Each major representation attempt gets before/after benchmark output and a
   keep/reject note.
2. `gsexp ./scripts/build.sh` passes.
3. `gsexp ./scripts/bench.sh` passes with yyjson enabled.
4. Benchmark build with `GSEXP_BENCHMARK_YYJSON=OFF` still works.
5. `glayout ./scripts/build.sh` passes after any public or vendoring-impacting
   change.
6. Existing public `Node` and `FormView` behavior stays stable unless a
   deliberate API change is documented and `glayout` is updated.
7. Retained memory is documented alongside speed. Faster code that doubles
   retained memory needs a clear reason.
8. Numeric changes must add tests before changing behavior.
9. SIMD changes must compile out cleanly on non-x86 or non-SIMD builds.
10. Keep files inside the size guideline by splitting representation, form
    lookup, symbol table, and parser construction into cohesive files.

Rejected-by-default unless evidence changes:

1. Reintroducing public compatibility wrappers for old extraction helpers.
2. Interning every string value by default.
   Start with form heads and repeated atoms.
3. Eagerly building full lookup indexes for every form during parse.
4. Hard-required SIMD.
5. Dense bit-packed node formats that are painful to debug.
6. Optimizing for yyjson validation-only numbers instead of retained tree plus
   lookup workloads.
7. A second public batch/cursor lookup API before proving the normal
   `FormView::get_*` API cannot be made competitive with internal state.

## Optimization Plan 10

Goal: make the public query API cleaner while giving the implementation a
caller-owned place to keep temporary lookup state. The current free extraction
helpers are easy for one-off calls, but they encourage repeated stateless scans
over the same list. A `FormView` API should become the normal way to consume
headed S-expression forms and should unlock lookup optimizations without adding
parallel public styles.

Vocabulary:

1. A form is a list shaped as `(head arg0 arg1 ...)`.
2. A parent form may contain child forms, such as
   `(asset (id 10) (path "foo.png") (color 1 0 0))`.
3. `FormView` is a non-owning view over one `Node`. It does not own parsed
   storage; the `ParseResult` lifetime rule remains unchanged.

Target API shape:

```cpp
gsexp::FormView asset(asset_node);

gsexp::Node head = asset.head();
gsexp::Node first_arg = asset.arg(0);
gsexp::Node path_form = asset.find("path");
gsexp::Node green = asset.find_arg("color", 1);

std::optional<int> id = asset.get_int("id");
std::optional<float> x = asset.get_float("x");
std::optional<std::string_view> path = asset.get_string_view("path");
```

API cleanup:

1. Add `FormView` as the recommended convenience/query API.
2. Remove public free extraction helpers instead of keeping compatibility
   wrappers. There are no external consumers yet.
3. Decide whether public `find_child` remains. Default: remove it if
   `FormView::find()` covers the use cleanly.
4. Keep `Node` as the raw tree/traversal API: `children()`, `child_at()`,
   `head()`, `second()`, `text()`, `is_atom()`, and type checks.
5. Update README, examples, tests, benchmarks, and `glayout` in the same change
   so there is one documented consumption style.

Work order:

1. Add `FormView` without optimization first.
   Implement the simple version as a thin wrapper around the existing traversal
   behavior. Update tests and benchmarks to use it. This establishes the new API
   before performance changes muddy the diff.

2. Update `glayout`.
   Replace current `find_child` and `extract_*` usage with `FormView`. Build
   `glayout` after the API change before attempting optimization.

3. Benchmark the new API baseline.
   Add or rename query benchmarks so they measure `FormView` as the default
   consumption path. Keep yyjson comparisons for common asset lookup and
   many-key lookup.

4. Caller-owned lookup cache.
   Add a small cache inside `FormView` and build it lazily on repeated lookup.
   Try the simplest shape first: a vector of `{head_text, child_index}` entries
   collected from direct children and sorted by head. Keep only if it improves
   repeated lookup without hurting one-off lookup.

5. Single-pass small-form scan.
   For small forms, test whether direct scan beats cache construction. The view
   can choose direct scan below a threshold and cached lookup above it, but the
   policy must stay simple and measured.

6. Batch lookup experiment.
   Try a `FormView` internal path that can satisfy several requested keys after
   one child scan. Do not add a separate batch public API unless the simple
   `get_*` API cannot benefit from it.

7. String access policy.
   Make `get_string_view` the benchmark/default for asset database loading when
   the caller can retain `ParseResult`. Keep `get_string` only for callers that
   explicitly want ownership.

8. Numeric extraction path.
   Keep numeric behavior correct, then measure whether `FormView` can avoid
   repeated value lookup and `Node` construction around `from_chars`.

9. Storage-owned global cache audit.
   If `FormView` cache performs well, decide whether the current
   `ParseStorage::child_indexes` cache is still needed. Avoid two competing
   cache systems unless each has a clear job.

10. yyjson gap review.
    Re-run the comparison table after the API and view-state changes. The main
    target is `assets_10k lookup`; parse speed is secondary in this plan.

Acceptance rules:

1. `gsexp ./scripts/build.sh` passes.
2. `gsexp ./scripts/bench.sh` passes with yyjson enabled.
3. Benchmark build with `GSEXP_BENCHMARK_YYJSON=OFF` still works.
4. `glayout ./scripts/build.sh` passes after the API change.
5. README, examples, tests, and benchmarks show `FormView` as the normal query
   API.
6. No compatibility wrappers for removed free extraction helpers unless a real
   consumer appears.
7. Every optimization attempt gets before/after benchmark output and a
   keep/reject note.
8. Keep `FormView` low-abstraction: a small non-owning type with obvious state,
   no hidden allocation-heavy machinery unless benchmarks justify it.
9. Keep files inside the size guideline by splitting `FormView` implementation
   out of `node.cpp` if needed.

Rejected-by-default unless evidence changes:

1. Keeping both free `extract_*` helpers and `FormView` as equally documented
   public styles.
2. Naming the API `Record`, `Object`, or `Map`; those are too semantic for
   code-like S-expressions.
3. Eagerly indexing every parsed form during parse.
4. Adding a separate public batch extraction API before proving the normal
   `FormView::get_*` API cannot be optimized enough.
5. Chasing yyjson lookup numbers by making the S-expression model less
   debuggable or less structural.

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
