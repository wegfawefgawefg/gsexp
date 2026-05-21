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

## Optimization Plan 2

Constraint: keep the normal user-facing API centered on `gsexp::parse(text)`.
Do not add a second public parse API as a shortcut around slow internals unless
a measured ownership/lifetime problem proves it is necessary.

Current limiting shape:

```cpp
struct Value {
    ValueType type;
    std::string text;
    std::vector<Value> list;
};
```

This is easy to use, but expensive to build for large files because every list
owns a vector and many atoms own strings.

Candidate attempts:

1. Lazy numeric parsing.
   Store parsed atoms as text first and convert only in `extract_int` and
   `extract_float`. This is now the chosen model because it is closer to normal
   S-expression reader behavior and keeps `parse(text)` simple.

2. Compact child storage for common short lists.
   Most config data contains many `(key value)` lists. Try an internal
   small-vector style representation or another low-overhead way to avoid heap
   allocations for tiny lists while keeping `Value::list` usable to callers.

3. Arena-backed allocation.
   Use a parser-owned arena or monotonic resource internally to reduce allocation
   overhead, but keep returned `ParseResult` owning its data normally. This is
   only worth keeping if the public result remains safe after the input string
   goes out of scope.

4. Reduce repeated key allocation and comparison.
   Repeated keys such as `asset`, `id`, `path`, `type`, and `tags` dominate
   asset-style files. Try interning or compact key handling internally without
   requiring callers to use interned symbols.

5. Flat node construction with tree materialization at the edge.
   Parse into contiguous internal nodes first, then materialize the public
   `Value` tree. This may improve parser locality but still pays the public tree
   construction cost. Keep only if measurements show that the staging step is
   net-positive.

6. Reconsider public `Value` only if the above hits a hard ceiling.
   The existing recursive `std::vector<Value>` API may cap performance. Any
   public shape change must be deliberate, documented, and justified by measured
   gains large enough to offset the usability/API cost.

## Optimization Plan 2 Status

1. Lazy numeric parsing.
   - Kept. Parsed atoms remain `ValueType::Atom`; numeric helpers interpret atom
     text when requested.
   - This replaced the earlier compatibility-safe numeric-start guard because
     the all-atom reader model is more standard for S-expressions and better for
     future evaluator layers.
   - `is_symbol()` remains as a compatibility alias for `is_atom()`.
   - Follow-up ASCII classification and bulk atom column advancement was kept.
     It removes per-character locale checks and line/column function calls from
     the atom hot path.

2. Compact child storage for common short lists.
   - `reserve(2)` for parsed lists was kept.
   - `reserve(4)` was tested twice and rejected because it slowed the large
     asset-style benchmark.
   - Building list nodes directly in the output `Value` was kept. It removes a
     temporary `Value` and final move assignment per list.
   - Filling output values directly was kept. It avoids assigning a fresh
     default `Value` over parser-local values that are usually already empty.
   - A true small-vector representation would require changing `Value::list`
     away from `std::vector<Value>`, which is a public shape change.

3. Arena-backed allocation.
   - Not implemented under the current public API. Returned `ParseResult` owns
     recursive `std::string` and `std::vector<Value>` members directly, so an
     internal arena cannot back the returned data without changing public
     ownership semantics.

4. Reduce repeated key allocation and comparison.
   - No separate key-interning change was kept. Short repeated keys are usually
     handled by small-string optimization, and the current benchmark measures
     parse/tree construction rather than repeated `find_child` queries.

5. Flat node construction with tree materialization at the edge.
   - Not implemented. It would still need to materialize the public recursive
     `Value` tree, so it is unlikely to remove the main allocation cost without
     a larger public representation change.

6. Reconsider public `Value`.
   - Started with the atom-based reader model. This is a deliberate source-level
     cleanup: callers should inspect `ValueType::Atom` and use helper functions
     for numeric interpretation.
   - Further large gains probably require another deliberate `Value`
     representation redesign rather than more parser-local tricks.

## Optimization Plan 3

Goal: stop optimizing against one synthetic shape. Add benchmark coverage first,
then use those numbers to decide whether representation changes are worth the
API cost.

Benchmark work:

1. Add a many-small-files case.
   Parse hundreds or thousands of small config strings. This catches startup
   overhead and root allocation behavior that the current large in-memory asset
   benchmark can hide.

2. Add a huge asset database case.
   Parse one much larger asset-style file than `assets_10k`. This should model
   the intended asset database use better and make allocator/locality issues
   easier to see.

3. Add escaped-string and long-string cases.
   Measure strings with no escapes, many escapes, and longer paths/text blobs.
   This gives a safer basis for revisiting string fast paths.

4. Add deep nesting and wide-list cases.
   Measure recursion overhead, list allocation behavior, and diagnostics
   behavior on shapes unlike `(key value)` asset records.

5. Add extraction/query benchmarks.
   Measure repeated `find_child`, `extract_int`, `extract_float`, and
   `extract_string` calls after parsing. This is separate from parse throughput
   and should drive helper-level optimizations.

Parser-local attempts:

1. Revisit unescaped string fast path only after string benchmarks exist.
   Keep it only if it improves large and string-heavy cases without making the
   slow escaped path harder to audit.

2. Try `std::from_chars` in extraction helpers.
   This will not improve parse throughput, but may help asset database loading
   if consumers query many numeric fields after parsing.

3. Try top-level root reservation.
   Count likely root expressions cheaply before parsing, or reserve a small
   fixed number if measurements show repeated root vector growth.

Representation attempts:

1. Small-list storage.
   Replace `std::vector<Value>` for very small lists with inline storage or a
   low-overhead list wrapper. This targets the common `(key value)` shape but is
   a public `Value` shape change unless wrapped carefully.

2. Source-owned string views.
   Let `ParseResult` own a source buffer and store atom/string slices into it
   where possible. This could remove many string allocations, but changes
   ownership semantics and needs careful escaping behavior for strings.

3. Atom/key interning.
   Intern repeated atoms such as `asset`, `id`, `path`, `type`, and `tags`.
   This can reduce memory and speed comparisons, but should not force callers to
   understand intern tables for normal use.

4. Flat node arena.
   Parse into contiguous nodes owned by `ParseResult` instead of recursive
   `std::vector<Value>` allocations. This is the most plausible path to another
   large speedup, but it is also the largest public representation decision.

5. Compatibility wrapper over a new core.
   If flat nodes or arenas are much faster, consider keeping convenience helpers
   while changing the low-level tree type deliberately. Do not maintain two
   unrelated parser APIs; the normal path should remain `parse(text)`.

Plan 3 acceptance rule:

1. Benchmark changes can be kept if they compile cleanly and measure useful,
   distinct workloads.
2. Parser-local optimizations must improve at least the workload they target and
   must not materially regress the huge asset case.
3. Representation changes need a larger bar: clear speed or memory wins, simple
   ownership rules, and no second confusing parse API.

## Optimization Plan 3 Status

1. Expanded benchmark suite.
   - Kept. Added many-small-files, huge asset database, plain/escaped string,
     deep nesting, wide list, and extraction/query cases.

2. Extraction helper numeric parsing.
   - Kept. `extract_int` and `extract_float` now use `std::from_chars` after the
     existing numeric shape checks.
   - `query_assets_10k` improved from 6.52M queries/s to 9.93M-10.13M queries/s
     in repeated runs.

3. Representation changes.
   - Started with source-owned `std::string_view` value text. `ParseResult`
     owns copied source text plus decoded escaped strings; text views
     remain valid while the owning result storage lives.
   - Kept despite an `assets_1k` regression because repeated runs improved
     `assets_10k`, `assets_50k`, `small_files_1k`, plain strings, wide lists,
     and query throughput.
   - Measured repeated results after the change: `assets_10k` 42.28-43.78
     MiB/s, `assets_50k` 45.49-46.29 MiB/s, `strings_plain_5k` 258.90-276.39
     MiB/s, `wide_10k` 121.04-125.83 MiB/s, `query_assets_10k` 10.20M-10.43M
     queries/s.
   - Small-list inline storage was not implemented directly because the current
     public recursive `Value` type cannot contain inline `Value` children
     without replacing `.list` with a wrapper type. The PMR allocation attempt
     was used as the lower-risk allocation experiment for the current shape and
     was rejected.

4. Top-level root reservation.
   - Reverted. Reserving one root slot did not clearly improve the many-small
     files case and added no value for the dominant single-root data shapes.

5. Atom hash lookup.
   - Reverted. Adding a stored FNV-style atom hash increased parse work and did
     not improve repeated `find_child`/`is_atom` queries. `query_assets_10k`
     dropped to 9.57M-9.62M queries/s, below the source-view result.
   - Full atom/key interning was not implemented after this result. Source-owned
     views already removed atom string allocation, and the measured lookup
     shortcut did not justify adding an intern table to the normal API.

6. PMR list allocation.
   - Reverted. Changing `Value::list` to `std::pmr::vector<Value>` backed by a
     `ParseResult` monotonic resource preserved normal `.list` usage, but the
     allocator indirection was slower across most parse cases and query
     throughput dropped to 9.43M-9.53M queries/s.

7. Flat node arena prototype.
   - Kept as a benchmark-only prototype. It parses into contiguous flat nodes
     with parent indices and source text views, without materializing the public
     recursive `Value` tree.
   - Measured `flat_assets_50k` at 164.51 MiB/s versus the public tree around
     45-46 MiB/s in nearby runs.
   - Measured `flat_wide_10k` at 277.66 MiB/s versus the public tree around
     119-132 MiB/s in nearby runs.
   - This proves there is still a large ceiling, but making it the normal
     `parse(text)` path requires a deliberate public representation rewrite or a
     compatibility wrapper over flat storage.

8. String fast path.
   - Covered by the source-owned view representation. Unescaped strings now
     return views into parse-owned source text without decoding or allocating.
     Escaped strings are still decoded into parse-owned storage.

9. Compatibility wrapper.
   - Not implemented in Plan 3. The flat benchmark proves a wrapper may be worth
     designing, but it would be a new public representation decision rather than
     a safe local optimization. Keep the current `parse(text)` API until that
     rewrite is explicitly chosen.

Plan 3 execution status:

1. Benchmark coverage was added and kept.
2. Parser-local attempts were tried, kept, or reverted with measurements.
3. Representation attempts were either implemented, rejected with measurements,
   or narrowed to a documented follow-up public representation decision.
4. The remaining major opportunity is a flat-storage `Value` redesign. That is
   outside incremental Plan 3 optimization and should be treated as a new design
   task before changing the normal API.

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
