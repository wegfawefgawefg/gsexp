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
| assets_10k | 239.93 MiB/s |
| assets_50k | 209.92 MiB/s |
| asset_database_5k | 298.39 MiB/s |
| asset_database_5k_owned | 301.88 MiB/s |
| asset_database_5k_file_read | 410.32 MiB/s |
| asset_database_5k_file_owned | 167.10 MiB/s |
| small_files_1k | 195.64 MiB/s |
| strings_plain_5k | 1075.62 MiB/s |
| strings_escaped_5k | 739.07 MiB/s |
| deep_1k | 182.62 MiB/s |
| code_forms_2k | 234.70 MiB/s |
| wide_10k | 348.36 MiB/s |
| query_assets_10k | 17.23M queries/s |
| query_first_10k | 16.86M queries/s |
| query_last_10k | 8.52M queries/s |
| query_missing_10k | 12.53M queries/s |
| query_string_view_10k | 18.90M queries/s |
| query_text_only_10k | 43.18M queries/s |
| query_symbol_compare_10k | 42.55M queries/s |
| query_many_keys_last | 6.20M queries/s |
| query_find_many_keys_last | 7.93M queries/s |
| query_child_at_many_keys_last | 6.57M queries/s |

Latest yyjson comparison results:

| Equivalent case | gsexp | yyjson | yyjson/gsexp |
| --- | ---: | ---: | ---: |
| assets_10k parse | 239.93 MiB/s | 630.66 MiB/s | 2.63x |
| assets_50k parse | 209.92 MiB/s | 614.52 MiB/s | 2.93x |
| asset_database_5k parse | 298.39 MiB/s | 802.97 MiB/s | 2.69x |
| small_files_1k parse | 195.64 MiB/s | 572.29 MiB/s | 2.93x |
| strings_plain_5k parse | 1075.62 MiB/s | 1319.84 MiB/s | 1.23x |
| strings_escaped_5k parse | 739.07 MiB/s | 1217.24 MiB/s | 1.65x |
| code_forms_2k parse | 234.70 MiB/s | 646.47 MiB/s | 2.75x |
| wide_10k parse | 348.36 MiB/s | 843.96 MiB/s | 2.42x |
| assets_10k lookup | 17.23M queries/s | 131.91M queries/s | 7.66x |
| many_keys_last lookup | 6.20M queries/s | 15.24M queries/s | 2.46x |

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
Plan 11 is allowed to heavily change internals while keeping the public
`Node`/`FormView` API stable. The target is a representation that is faster to
walk, faster to query, and closer to contiguous memory.

Current gap during Plan 11:

1. `assets_10k` parse is about 2.63x behind yyjson on the latest Plan 11 run.
2. `asset_database_5k` parse is about 2.69x behind yyjson.
3. `code_forms_2k` parse is about 2.75x behind yyjson.
4. `assets_10k` lookup is about 7.66x behind yyjson.
5. `many_keys_last` lookup is about 2.46x behind yyjson.
6. The public API is now clean enough that internal representation churn should
   not force another user-facing rewrite.

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

9. Numeric parse specialization.
   Try small custom parsers for common integer and simple decimal float shapes.
   Keep exact rejection behavior covered by tests. Reject if `from_chars` is
   still faster or if correctness gets harder to reason about.

10. Scanner success-path rewrite.
    Tighten whitespace/comment/atom/string scanning after the representation
    change. Separate syntax scanning from tree construction enough that hot
    loops stay simple. Preserve diagnostics.

11. SIMD integration.
    Integrate the existing SSE2 delimiter/string scan proof only after scalar
    representation changes settle. SIMD remains optional with scalar fallback.
    Benchmark parse cases and compile portability.

12. Allocation discipline.
    Reduce growth churn across nodes, child indices, decoded text, symbol table,
    and lazy indexes. Prefer flat arenas over per-list heap allocations.

13. Real workload fixtures.
    Add larger and more realistic generated asset database cases, and later
    real project fixtures when available. Keep synthetic yyjson comparisons,
    but do not optimize solely for synthetic records.

14. API audit after internals settle.
    Keep `Node` and `FormView` public behavior stable. Only add public API if
    benchmarks prove users need explicit symbol handles or caller-side compiled
    keys.

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

5. Flat node arena with child spans.
   Retry child spans with a cleaner construction strategy after other hot spots
   are measured. The first attempt proved that smaller nodes alone do not win
   if finalization and child lookup get slower.

6. Parser event/tape builder.
   Try a parse-time tape or event stream that records list starts, atoms, and
   closes, then builds the retained tree in one predictable pass. Reject if it
   only adds another representation without removing cost elsewhere.

7. One-pass retained builder.
   Try building final child ranges directly while parsing, using stack frames
   that own contiguous output ranges. This is harder than sibling links but may
   remove the expensive finalization pass from the rejected child-span attempt.

8. Atom scanner specialization.
   Split atom scanning into the common no-escape, no-comment, no-delimiter fast
   path and the slower diagnostic path. The hot loop should mostly advance over
   plain bytes and only branch at delimiters.

9. String scanner specialization.
   Keep plain string views when no escapes are present and decode only escaped
   strings into `owned_text`. Measure plain and escaped strings separately, as
   they have very different ceilings.

10. Whitespace and comment skipping.
    Tighten the skip loop because every benchmark pays it. Try simple
    table-driven classification before SIMD. Reject clever code if branch
    prediction already wins.

11. Numeric token classification.
    Preserve atom-first semantics, but classify likely integer and decimal
    tokens cheaply enough that `FormView::get_int()` and `get_float()` avoid
    duplicate work. Add tests before changing accepted numeric shapes.

12. Numeric conversion specialization.
    Try custom integer conversion and simple decimal float conversion for common
    asset data. Keep `from_chars` fallback for correctness and uncommon shapes.

13. Allocation reservation policy.
    Improve reserve estimates for nodes, owned text, child indexes, symbol
    tables, and lazy indexes. Track capacity in benchmark output so wins are not
    just hidden memory growth.

14. Small-vector avoidance.
    Avoid per-form heap allocation in lazy indexes and temporary builder data.
    Prefer flat arenas or fixed-size stack buffers where the measured workload
    repeatedly builds small structures.

15. Cache threshold tuning.
    Re-test the direct-scan versus lazy-index threshold after each
    representation change. The correct threshold may move once symbol IDs or
    flat indexes exist.

16. Common asset lookup benchmark.
    Keep the current lookup benchmarks, but add fixtures that are not only
    record-shaped. The parser may later handle code-like input, so optimizing
    only `(asset (id ...) ...)` would be too narrow.

17. Larger generated fixtures.
    Add larger asset databases, mixed strings, mixed numeric fields, deep forms,
    wide forms, and code-like forms. Compare both parse throughput and query
    throughput against yyjson equivalents where a JSON shape makes sense.

18. Real fixture import.
    Add real project data once available. Synthetic data is useful for drag
    races, but final decisions should also use files that resemble actual game
    assets and tool output.

19. SIMD delimiter scan.
    Add optional SSE2 delimiter scanning for atom and whitespace-heavy input
    after scalar scanner changes settle. Keep scalar fallback compiled and
    benchmarked.

20. SIMD string scan.
    Add optional SIMD quote/backslash detection for long plain strings. This
    should target the plain string benchmark first and must not slow short
    strings.

21. Memory layout audit.
    Recheck `sizeof(NodeData)`, vector capacities, and retained bytes after each
    kept change. A parse win that bloats retained memory needs a workload-based
    justification.

22. Error-path audit.
    Keep diagnostics useful while moving hot loops. Fast success paths are fine,
    but malformed input should still report practical file offsets and reasons.

23. Public API audit.
    After internals settle, remove accidental exposure of implementation details
    and document the one normal consumption path. Avoid compatibility wrappers
    because there are no external consumers yet.

24. Glayout integration check.
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
