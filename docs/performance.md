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
improvements and memory/allocation evidence.

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

Candidate optimizations:

1. `parse_owned(std::string)`.
   Add an overload that moves caller-owned source text into `ParseResult`
   storage instead of copying it. This keeps the same parser and representation;
   it is an ownership convenience, not a second parser.

2. Query helper indexing.
   Add optional or lazy per-list key indexes for `find_child` and `extract_*`.
   This targets query-heavy consumers. Keep only if repeated lookup benchmarks
   improve enough to justify the memory and implementation cost.

3. Faster common child access.
   `child_at(n)` walks sibling links. Try storing child ranges contiguously or
   adding helper methods for `head()` and `second()` so common `(key value)`
   helpers do not repeatedly walk siblings.

4. Node field packing.
   Current nodes store type, text view, parent, first child, last child, next
   sibling, and child count. Measure whether parent or last child are needed
   after parsing, and whether fields can be packed without hurting clarity.

5. Root and node reservation.
   Try estimating node count before parse or reserving based on source length.
   The old top-level root reservation failed, but flat storage may benefit from
   one large `nodes.reserve(...)`.

6. Escaped string storage.
   Decoded escaped strings currently live in `std::deque<std::string>`. Try a
   contiguous decoded string buffer with views if escaped-heavy benchmarks show
   it matters.

7. Helper return shapes.
   `extract_string` returns `std::string`, which copies. Consider adding
   `extract_string_view` for callers that can respect `ParseResult` lifetime.
   Keep `extract_string` for easy ownership.

8. Diagnostics cost.
   Line/column tracking is always maintained. Try measuring a diagnostics-light
   parser mode only if benchmarks show line tracking is a real cost. Do not add
   a confusing parser API just to skip diagnostics.

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
