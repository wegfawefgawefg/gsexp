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
