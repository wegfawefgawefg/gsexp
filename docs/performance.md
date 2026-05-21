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

The benchmark generates S-expression asset records in memory and parses them
repeatedly. It reports the best of three runs for each case. It is intended to
catch parser-level improvements, not full application startup behavior.

## Results

| Date | Change | assets_1k MiB/s | assets_10k MiB/s | Result |
| --- | --- | ---: | ---: | --- |
| 2026-05-21 | Baseline: token vector plus value parse | 17.30 | 13.69 | Baseline |
| 2026-05-21 | Direct one-pass `parse()`; public `tokenize()` unchanged | 17.69 | 19.52 | Kept |
| 2026-05-21 | Replace numeric conversion with `std::from_chars` | 16.34 | 16.00 | Reverted; slower |
| 2026-05-21 | Reserve 4 child slots for every list | 31.00 | 16.15 | Reverted; mixed, slower large case |
| 2026-05-21 | Reserve 2 child slots for every list | 27.34 | 21.44 | Kept |
| 2026-05-21 | Fast-path unescaped strings | 28.68 | 21.12 | Reverted; slower |
| 2026-05-21 | Skip numeric checks for atoms that cannot start numbers | 36.07 | 27.48 | Kept |
| 2026-05-21 | Reserve 32 bytes for parsed strings | 37.76 | 29.30 | Kept |
| 2026-05-21 | Retest reserve 4 child slots after numeric guard/string reserve | 34.00 | 20.03 | Reverted; slower |
| 2026-05-21 | Full lazy numeric parsing under old `Symbol` model | 42.35 | 30.94 | Superseded by deliberate `Atom` model |
| 2026-05-21 | Reader model: atoms stay atoms; helpers interpret numbers | 38.84-47.29 | 24.49-29.78 | Kept; public model changed deliberately; reruns were noisy |
| 2026-05-21 | ASCII classification and bulk atom column advance | 51.42 | 33.23 | Kept |
| 2026-05-21 | Build lists directly in output `Value` | 51.81 | 33.26 | Kept; small/mostly neutral simplification |
| 2026-05-21 | Retest fast-path unescaped strings after atom model | 52.69-54.26 | 29.26-33.53 | Reverted; large case mixed and code was less direct |
| 2026-05-21 | Bulk-skip horizontal whitespace in parser | 51.60 | 33.12 | Reverted; no gain over simpler code |
| 2026-05-21 | Fill parser output values directly instead of assigning `Value{}` | 59.22-60.97 | 36.45-36.85 | Kept |
| 2026-05-21 | Remove defensive output field clears | 62.33 | 36.17 | Reverted; large case slower and invariant was less explicit |

## Plan 3 Benchmark Baseline

The benchmark suite now covers more than the original asset parse shape. The
first run below is the baseline after adding the benchmark cases and before
Plan 3 optimization attempts.

| Date | Case | Metric | Result |
| --- | --- | --- | ---: |
| 2026-05-21 | assets_1k | MiB/s | 57.35 |
| 2026-05-21 | assets_10k | MiB/s | 35.73 |
| 2026-05-21 | assets_50k | MiB/s | 36.74 |
| 2026-05-21 | small_files_1k | MiB/s | 103.42 |
| 2026-05-21 | strings_plain_5k | MiB/s | 149.94 |
| 2026-05-21 | strings_escaped_5k | MiB/s | 159.95 |
| 2026-05-21 | deep_1k | MiB/s | 63.23 |
| 2026-05-21 | wide_10k | MiB/s | 104.25 |
| 2026-05-21 | query_assets_10k | queries/s | 6,521,070 |

## Plan 3 Results

| Date | Change | Main target | Result |
| --- | --- | --- | --- |
| 2026-05-21 | Add expanded benchmark suite | Measurement coverage | Kept |
| 2026-05-21 | Use `std::from_chars` in extraction helpers | `query_assets_10k` | Kept; improved from 6.52M to 9.93M-10.13M queries/s |
| 2026-05-21 | Reserve one top-level root slot | `small_files_1k` | Reverted; no clear gain and parse cases were noisy/worse |
| 2026-05-21 | Source-owned `std::string_view` value text | `assets_50k`, string-heavy, wide lists | Kept; large/string-heavy wins, `assets_1k` regressed |
| 2026-05-21 | Store atom text hashes for faster `is_atom` rejection | `query_assets_10k` | Reverted; query and large parse cases were slower |
| 2026-05-21 | Store list vectors in `std::pmr::monotonic_buffer_resource` | Allocation-heavy parse cases | Reverted; slower across most expanded benchmarks |
| 2026-05-21 | Add flat-node parser prototype benchmark | `flat_assets_50k`, `flat_wide_10k` | Kept as benchmark; shows large ceiling but not public API yet |

## Archived Plans

Older Plan 2 and Plan 3 design notes were moved to
[performance-history.md](performance-history.md). Current work should use Plan 4
status and Plan 5 below as the active context.

## Optimization Plan 4

Goal: replace the recursive public `Value` tree with flat storage and node
handles. This is an intentional greenfield API change. Do not keep the old
recursive tree as a second parser path.

Public shape:

1. Keep one parser entry point.
   `gsexp::parse(text)` remains the normal way to parse. Do not add `parse_fast`
   or a parallel parser API.

2. Make `ParseResult` own flat node storage.
   Store nodes contiguously, plus parse-owned source text and decoded escaped
   strings. Node handles/views are valid while the owning `ParseResult` remains
   alive.

3. Expose node handles as the base API.
   Callers should be able to write:

```cpp
gsexp::ParseResult result = gsexp::parse(text);
gsexp::Node root = result.root(0);

for (gsexp::Node child : root.children()) {
    // inspect child
}
```

4. Keep config helpers as the simple API.
   Helpers should work on `Node` handles:

```cpp
std::optional<int> width = gsexp::extract_int(root, "width");
std::optional<std::string> label = gsexp::extract_string(root, "label");
```

5. Remove the recursive `Value` API instead of maintaining both.
   There are no real external consumers yet. Update `glayout` directly and keep
   the library API small.

Proposed node model:

```cpp
struct NodeData {
    ValueType type;
    std::string_view text;
    uint32_t parent;
    uint32_t first_child;
    uint32_t next_sibling;
};
```

`Node` should be a lightweight handle containing a pointer/reference to the
owning storage plus a node index. It should provide direct, low-magic methods:

- `type()`
- `text()`
- `is_atom(value)`
- `children()`
- `first_child()`
- `next_sibling()`
- `empty()`

Implementation steps:

1. Move the benchmark-only flat parser shape into the real parser.
   Keep the existing diagnostics behavior and source-owned string lifetime.

2. Add `Node`, `ChildRange`, and `ParseResult::root(index)`.
   Keep iteration simple and debugger-friendly.

3. Port helpers to `Node`.
   Update `is_atom`, `find_child`, `extract_int`, `extract_float`, and
   `extract_string`.

4. Update tests.
   Cover root access, child iteration, extraction helpers, escaped strings,
   copied/moved `ParseResult`s, malformed input, comments, and multiple roots.

5. Update `glayout`.
   Replace direct `.list` usage with `Node` iteration and helper calls. Do not
   add compatibility shims just for old `Value` code.

6. Update docs and examples.
   README and spec should show `Node` handle usage plus helper usage.

7. Remove or rewrite the benchmark prototype.
   Once flat storage is the real parser, the benchmark should compare normal
   `parse(text)` directly. Keep no duplicate flat parser in benchmark code.

Acceptance rules:

1. `gsexp` build/tests pass.
2. `glayout` build/tests pass after migration.
3. Expanded benchmark suite runs.
4. Normal `parse(text)` should approach the flat prototype results on large and
   wide cases. It does not need to match the prototype exactly, but the rewrite
   must clearly beat the current public tree on `assets_10k`, `assets_50k`, and
   `wide_10k`.
5. The simple helper API must remain easy enough for config loading. If callers
   need to manually chase indices for common `(key value)` data, the API is too
   low-level.
6. No second parser API, no hidden recursive tree materialization, and no broad
   compatibility layer for the removed `Value` shape.

## Optimization Plan 4 Status

1. Flat storage parser.
   - Implemented. `gsexp::parse(text)` now writes contiguous `NodeData` records
     into `ParseResult` storage instead of materializing recursive `Value`
     vectors.
   - Measured normal `parse(text)` after the rewrite: `assets_10k` 59.42 MiB/s,
     `assets_50k` 87.08 MiB/s, `wide_10k` 160.36 MiB/s, and
     `strings_plain_5k` 371.92 MiB/s.
   - These results clearly beat the previous public tree on the Plan 4 required
     large and wide cases. `assets_1k` remains slower than the earlier small
     recursive-tree best, but the larger target data improved substantially.

2. Public node API.
   - Implemented. `ParseResult::root(index)` returns `Node`; child traversal uses
     `Node::children()`, `first_child()`, `next_sibling()`, and `child_at()`.

3. Helper API.
   - Implemented over `Node`: `is_atom`, `find_child`, `extract_int`,
     `extract_float`, and `extract_string`.

4. Recursive `Value` API.
   - Removed. No compatibility parser or recursive tree materialization is kept.

5. Downstream migration.
   - `glayout` was updated to use `Node` handles and helper calls.

6. Benchmark prototype.
   - Removed from the benchmark source. The expanded benchmark suite now measures
     normal `parse(text)` directly.

7. File layout.
   - Split node traversal and extraction helpers into `src/node.cpp`; parser,
     tokenizer, and quoting remain in `src/sexp.cpp`.

## Optimization Plan 5

Goal: tune the flat-node parser without making the API clever. The major
representation win is already done; Plan 5 should focus on measured targeted
improvements, memory evidence, and query behavior. The normal API should stay
close to `parse(text)` plus simple `Node` helpers.

Benchmark and measurement work:

1. Add memory/allocation metrics.
   Throughput alone is no longer enough. Add a simple way to measure node count,
   source bytes, decoded string count, and approximate storage bytes for each
   benchmark case. Use this before packing node fields or adding indexes.

2. Add owned-source benchmark cases.
   Measure parsing from temporary/generated `std::string` data separately from
   parsing string literals/views. This will show whether avoiding the source copy
   is worth an API addition.

3. Expand query benchmarks.
   Add cases for repeated lookup of existing keys, missing keys, first key,
   last key, and many keys per list. Current `query_assets_10k` only covers one
   common pattern.

4. Add escaped-heavy string benchmarks.
   Current escaped strings are covered, but not enough to decide whether decoded
   string storage deserves a different representation.

Candidate options:

1. `parse_owned(std::string)`.
   Add an overload that moves caller-owned source text into `ParseResult`
   storage instead of copying it. This keeps the same parser and representation;
   it is an ownership convenience, not a second parser. Keep only if it is useful
   for real file-load paths or at least neutral in parse benchmarks.

2. Query helper indexing.
   Add optional or lazy per-list key indexes for repeated `find_child` and
   `extract_*` calls. This targets query-heavy consumers. Keep only if last-key,
   missing-key, and many-key benchmarks improve enough to justify the memory and
   implementation cost. Do not build indexes eagerly for every list unless parse
   throughput and storage metrics stay acceptable.

3. Faster common child access.
   `child_at(n)` walks sibling links. Try storing child ranges contiguously or
   adding helper methods for `head()` and `second()` so common `(key value)`
   helpers do not repeatedly walk siblings.

4. Node field packing.
   The original flat nodes stored type, text view, parent, first child, last
   child, next sibling, and child count. Measure whether retained fields are
   needed after parsing, and whether fields can be packed without hurting
   clarity. This is mostly a memory/cache option, not a public API feature.

5. Root and node reservation.
   Try estimating node count before parse or reserving based on source length.
   The old top-level root reservation failed, but flat storage may benefit from
   one large `nodes.reserve(...)`.

6. Escaped string storage.
   The original decoded escaped strings lived in `std::deque<std::string>`. Try
   a contiguous decoded string buffer with views if escaped-heavy benchmarks show
   it matters.

7. Helper return shapes.
   `extract_string` returns `std::string`, which copies. Consider adding
   `extract_string_view` for callers that can respect `ParseResult` lifetime.
   Keep `extract_string` for easy ownership.

8. Diagnostics cost.
   Line/column tracking is always maintained. Try measuring a diagnostics-light
   parser mode only if benchmarks show line tracking is a real cost. Do not add
   a confusing parser API just to skip diagnostics.

9. Tokenizer isolation.
   `tokenize()` is still public and separate from the direct parser. Confirm it
   does not force parser-side structure or slow paths. Do not optimize
   `tokenize()` at the expense of `parse()` unless a real consumer needs it.

10. Character classification table.
    The parser already uses simple ASCII checks. Try a small lookup table only if
    profiles show delimiter/whitespace checks are hot. Revert if it makes the
    scanner harder to read without a clear parse win.

11. Source copy/file-load path.
    Add a benchmark that reads or simulates loaded file contents and then parses
    with `parse_owned`. This is different from repeatedly copying benchmark
    strings into `parse_owned`, which can hide the intended benefit.

12. Arena-style decoded strings.
    If escaped strings matter, replace many decoded `std::string` allocations
    with one growing byte buffer plus string views. Keep only if escaped-heavy
    cases improve and `Node::text()` lifetime remains obvious.

13. Small-list specialization.
    Most config records are `(key value)` pairs. Consider a representation or
    helper path that makes two-child lists cheaper without adding a second user
    model. Reject if it complicates child iteration.

14. Optional parse limits.
    Depth and node-count limits can protect asset pipelines from bad files. This
    is not a speed feature, but should be considered while touching parser
    internals. Defaults must preserve current behavior.

15. Error-path cleanup.
    Keep malformed-input handling non-crashy and predictable. If parser hot paths
    are simplified, re-run error tests and avoid moving complexity into public
    callers.

Plan 5 status table:

| Date | Option | Target case | Result |
| --- | --- | --- | --- |
| 2026-05-21 | Memory/storage stats | All parse cases | Kept; benchmarks now print node, decoded string, index, and approximate storage stats |
| 2026-05-21 | Owned-source parse path | `assets_10k_owned`, `assets_50k_owned` | Kept; single parser implementation with caller-owned source convenience |
| 2026-05-21 | Expanded query cases | first, last, missing, string view, many-key | Kept; used to evaluate direct scan and lazy index changes |
| 2026-05-21 | `head()` / `second()` helpers | `(key value)` helper lookup | Kept; simple helper path replacing repeated `child_at(1)` walks |
| 2026-05-21 | `extract_string_view` | string query helpers | Kept; avoids copies for callers that respect `ParseResult` lifetime |
| 2026-05-21 | Reserve nodes with `source.size() / 4` | `assets_10k`, `assets_50k`, `wide_10k` | Kept; large parse speed improved substantially, with deliberate over-reserve on long string-heavy files |
| 2026-05-21 | Reserve nodes with `source.size() / 8` | Reduce string-heavy over-reserve | Rejected; lower memory than `/4`, but much slower on large asset and wide parse cases |
| 2026-05-21 | Exact node-count pre-scan | Reduce node capacity waste | Rejected; exact capacity, but the extra scan lost too much throughput |
| 2026-05-21 | Reserve nodes with `source.size() / 6` | Middle ground between `/4` and `/8` | Rejected; slower than `/4` and overgrew `assets_50k` capacity |
| 2026-05-21 | Direct `NodeData` scan in `find_child` | Common helper lookup | Kept; reduces per-child handle churn and improves common query cases |
| 2026-05-21 | Lazy child index at 8 children | Repeated last/missing lookup | Rejected; helped missing/last queries, but hurt common asset queries too much |
| 2026-05-21 | Lazy child index at 16 children | Many-key repeated lookup | Kept; leaves normal asset records unindexed and improves wide lookup |
| 2026-05-21 | Remove stored `parent` from `NodeData` | Node field packing | Kept; field was never read after parse and storage/query results improved |
| 2026-05-21 | Move `last_child` to parser-local state and reorder `NodeData` | Node field packing | Kept; reduces retained node size and final storage substantially |
| 2026-05-21 | Store decoded escaped strings in one byte arena | Escaped string storage | Kept; escaped-heavy parse improved and API lifetime stayed unchanged |
| 2026-05-21 | Bulk-skip horizontal whitespace and comments | Diagnostics/scanner overhead | Rejected; mixed parse results and material regressions in string/wide cases |

Plan 5 current baseline before node reservation:

| Case | Metric | Result |
| --- | --- | ---: |
| assets_10k | MiB/s | 55.44 |
| assets_50k | MiB/s | 81.68 |
| wide_10k | MiB/s | 145.15 |
| strings_plain_5k | MiB/s | 385.14 |
| strings_escaped_5k | MiB/s | 241.70 |

Node reservation attempt results:

| Attempt | assets_10k MiB/s | assets_50k MiB/s | wide_10k MiB/s | strings_plain_5k MiB/s | Notes |
| --- | ---: | ---: | ---: | ---: | --- |
| `source.size() / 4` | 199.35 | 172.93 | 279.80 | 437.06 | Best parse throughput; keeps simple parser-local code |
| `source.size() / 8` | 158.75 | 129.07 | 234.61 | 456.43 | Better string-heavy capacity, weaker large parse throughput |
| exact pre-scan | 121.06 | 104.36 | 145.84 | 256.13 | Exact node capacity, but scan cost is too high |
| `source.size() / 6` | 142.60 | 104.75 | 179.78 | 429.20 | Not a useful middle ground |

Final kept-code verification:

| Case | Metric | Result |
| --- | --- | ---: |
| assets_10k | MiB/s | 187.36 |
| assets_50k | MiB/s | 156.62 |
| wide_10k | MiB/s | 230.42 |
| strings_plain_5k | MiB/s | 419.22 |
| strings_escaped_5k | MiB/s | 248.91 |
| query_assets_10k | queries/s | 9,787,960 |
| query_many_keys_last | queries/s | 2,882,830 |

Memory note: `/4` intentionally trades extra capacity for throughput. On the
measured string-heavy case, node capacity rises to 294,170 for 55,002 nodes.
That is acceptable for now because the target asset cases improve by roughly
2.1x to 3.6x and the code stays one line. Revisit with a better density
heuristic only if string-heavy files become a real memory problem.

Query lookup attempt results:

| Attempt | query_assets_10k q/s | query_first_10k q/s | query_last_10k q/s | query_missing_10k q/s | query_string_view_10k q/s | query_many_keys_last q/s | Result |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Before query changes | 9,787,960 | 9,069,900 | 4,491,430 | 5,604,800 | 7,188,450 | 2,882,830 | Baseline after node reservation |
| Direct `NodeData` scan | 10,891,600 | 8,560,020 | 4,548,420 | 5,879,680 | 8,146,870 | 2,587,660 | Mixed but useful; no storage cost |
| Lazy index threshold 8 | 8,460,980 | 8,233,890 | 5,314,900 | 13,582,300 | 9,530,050 | 3,353,450 | Rejected; normal asset queries regressed |
| Lazy index threshold 16 | 10,479,100 | 9,490,840 | 4,486,900 | 5,869,370 | 8,981,610 | 3,237,910 | Kept; improves common/string/many-key without indexing asset records |

Index memory note: with threshold 16, the asset query cases build no child
indexes. The many-key benchmark builds 5,000 child indexes with 120,000 entries;
reported approximate storage rises to 24,448,404 bytes for that parsed result.
The estimate counts entry capacity but not every `unordered_map` bucket/control
allocation, so it is useful for relative tracking, not exact heap accounting.

Node packing attempt results:

| Attempt | assets_10k MiB/s | assets_50k MiB/s | wide_10k MiB/s | assets_50k approx bytes | query_assets_10k q/s | query_string_view_10k q/s | Result |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Before node packing | 183.65 | 154.66 | 228.74 | 90,633,246 | 10,479,100 | 8,981,610 | Baseline after query index |
| Remove stored `parent` | 189.48 | 155.63 | 203.94 | 76,689,670 | 11,976,400 | 10,159,500 | Kept |
| Move `last_child` parser-local and reorder fields | 186.96 | 164.43 | 224.46 | 62,746,094 | 13,831,400 | 12,730,800 | Kept |

Packing note: `parent` was retained only from the original tree shape and was
not used by traversal or helpers. `last_child` is needed only while parsing to
append siblings, so it now lives in a parser-local vector and is not retained in
`ParseResult`. Reordering the remaining fields makes the storage reduction real
instead of just moving padding around.

Escaped string storage attempt results:

| Attempt | strings_escaped_5k MiB/s | strings_escaped_5k approx bytes | assets_50k MiB/s | strings_plain_5k MiB/s | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| `std::deque<std::string>` decoded storage | 253.54 | 12,645,121 | 164.43 | 428.51 | Baseline after node packing |
| Single decoded byte arena | 296.05 | 12,916,802 | 168.45 | 430.36 | Kept |

Escaped storage note: the arena reserves `source.size()` bytes the first time an
escaped string is decoded. That is intentionally conservative because
`Node::text()` returns `std::string_view`; the backing buffer must not reallocate
after earlier nodes point into it. The benchmark shows the speed win is worth the
small measured storage increase on escaped-heavy files.

Diagnostics/scanner attempt results:

| Attempt | assets_10k MiB/s | assets_50k MiB/s | strings_plain_5k MiB/s | strings_escaped_5k MiB/s | wide_10k MiB/s | Result |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Before bulk scanner change | 189.70 | 168.45 | 430.36 | 296.05 | 249.57 | Baseline after decoded arena |
| Bulk-skip spaces/tabs/comments | 186.68 | 157.38 | 384.39 | 266.42 | 206.07 | Rejected |
| Reverted to simpler scanner | 203.12 | 162.57 | 422.57 | 274.17 | 241.33 | Kept current simpler code |

Scanner note: line/column tracking remains always on. A diagnostics-light parser
mode is still rejected by default because it would complicate the API; the local
scanner change did not justify that direction.

Plan 5 closure notes:

1. `parse_owned` uses the same parser as `parse`; it is not a second parser
   path. The benchmark necessarily reconstructs source strings between
   iterations, so it is a useful ownership-path check rather than a pure file I/O
   benchmark.
2. `tokenize()` remains separate from the direct parser. No Plan 5 change made
   parser hot paths depend on tokenization.
3. Character classification table work is covered by the scanner attempt above.
   The simpler branch checks are retained because the measured table-adjacent
   scanner change did not win.
4. Optional parse limits are not implemented in Plan 5. They are a safety/API
   feature, not a demonstrated parser-speed optimization, and defaults would need
   to preserve current behavior.
5. Error-path behavior remains covered by `gsexp_tests`; malformed input stays
   non-crashy and diagnostic-producing after the storage changes.

Rejected-by-default unless evidence changes:

1. A second parser path such as `parse_fast`.
   Plan 4 intentionally kept one parser. Do not split behavior unless a concrete
   use case proves the normal parser cannot satisfy it.

2. Mandatory global atom interning.
   The atom hash attempt did not help. Interning should stay off the table unless
   memory metrics or query benchmarks show a clear need.

3. Deep generic wrappers around `Node`.
   Keep node handles explicit and debugger-friendly.

Acceptance rules:

1. `gsexp` build/tests pass.
2. `glayout` build/tests pass after any API change.
3. Expanded benchmark suite runs.
4. Any kept optimization must improve its target benchmark and avoid material
   regressions in `assets_10k`, `assets_50k`, and `wide_10k`.
5. API additions must be small and explainable in the README. Prefer helper
   additions over parallel parser concepts.
6. File sizes should stay near the existing split. If `node.cpp`, `sexp.cpp`, or
   benchmarks grow past the target range, split by responsibility.
