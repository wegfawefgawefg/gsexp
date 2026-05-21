# gsexp Performance History

This file archives earlier optimization plans so `performance.md` can stay
focused on the current parser shape.

## Plan 2 Summary

Plan 2 worked on the original recursive `Value` tree:

```cpp
struct Value {
    ValueType type;
    std::string text;
    std::vector<Value> list;
};
```

Important outcomes:

1. Parsed atoms now stay atoms.
   Numeric conversion moved to helpers such as `extract_int` and
   `extract_float`. This matched normal S-expression reader behavior and removed
   eager numeric parsing from the hot path.

2. Direct one-pass parsing replaced token-vector-driven parsing for
   `parse(text)`.

3. ASCII character classification and bulk atom scanning were kept.

4. Building list nodes directly in the parser output was kept.

5. Filling output values directly instead of repeatedly assigning `Value{}`
   was kept.

Rejected attempts:

1. `std::from_chars` during eager parse-time numeric conversion.
2. Reserving four children for every list.
3. Fast-path unescaped strings in the old owned-string model.
4. Bulk horizontal whitespace skipping.
5. Removing defensive output field clears.

## Plan 3 Summary

Plan 3 added broader benchmarks and tested representation changes before the
flat-node rewrite.

Benchmark cases added:

1. `assets_50k`
2. `small_files_1k`
3. `strings_plain_5k`
4. `strings_escaped_5k`
5. `deep_1k`
6. `wide_10k`
7. `query_assets_10k`

Kept:

1. `std::from_chars` in extraction helpers.
   `query_assets_10k` improved from 6.52M to roughly 9.93M-10.13M queries/s.

2. Source-owned `std::string_view` value text.
   `ParseResult` owned copied source text plus decoded escaped strings. This
   improved large/string-heavy workloads but still used the recursive tree.

3. Flat-node benchmark prototype.
   The prototype showed the next ceiling: `flat_assets_50k` measured 164.51
   MiB/s and `flat_wide_10k` measured 277.66 MiB/s.

Rejected:

1. Top-level root reservation.
2. Stored atom hashes for faster `is_atom` rejection.
3. `std::pmr::vector<Value>` backed by a monotonic resource.

Conclusion:

The recursive tree was the main remaining bottleneck. The flat-node prototype
justified Plan 4, where the normal public parser moved to flat storage and
`Node` handles.

## Plan 4 Summary

Plan 4 replaced the recursive public `Value` tree with flat parse-owned storage
and lightweight `Node` handles. The old `Value` API was removed instead of
maintaining a compatibility layer.

Kept:

1. `ParseResult` owns contiguous `NodeData` records, copied source text, and
   decoded escaped string storage.
2. `ParseResult::root(index)` returns a `Node` handle.
3. Child traversal uses `children()`, `first_child()`, `next_sibling()`, and
   `child_at()`.
4. Config helpers work on `Node`: `find_child`, `extract_int`,
   `extract_float`, and `extract_string`.
5. `glayout` was migrated to the `Node` API.

Measured after the rewrite:

| Case | Result |
| --- | ---: |
| assets_10k | 59.42 MiB/s |
| assets_50k | 87.08 MiB/s |
| wide_10k | 160.36 MiB/s |
| strings_plain_5k | 371.92 MiB/s |

## Plan 5 Summary

Plan 5 tuned the flat-node parser while keeping the public API small.

Kept:

1. Storage metrics in `storage_stats()` and benchmark output.
2. `parse_owned(std::string)` for moving already-loaded source into the parse
   result.
3. Expanded query benchmarks for first, last, missing, string-view, and wide-key
   lookups.
4. `head()` and `second()` helpers for common `(key value)` access.
5. `extract_string_view` for callers that can keep `ParseResult` alive.
6. `nodes.reserve(source.size() / 4)` for parser storage.
7. Direct `NodeData` scans in `find_child`.
8. Lazy child indexes for lists with at least 16 children.
9. Packed retained `NodeData` fields; `parent` and retained `last_child` were
   removed.
10. A parse-owned decoded string byte arena for escaped strings.

Rejected:

1. `source.size() / 8`, `source.size() / 6`, and exact pre-scan node reserve
   strategies.
2. Lazy child indexing at threshold 8.
3. Bulk horizontal whitespace and comment skipping.
4. Diagnostics-off parser mode.
5. A second public parser path.
6. Mandatory global atom interning.

Final verified Plan 5 results:

| Case | Result |
| --- | ---: |
| assets_10k | 206.53 MiB/s |
| assets_50k | 171.93 MiB/s |
| small_files_1k | 175.52 MiB/s |
| strings_plain_5k | 432.42 MiB/s |
| strings_escaped_5k | 283.96 MiB/s |
| wide_10k | 255.03 MiB/s |
| query_assets_10k | 14.88M queries/s |
| query_many_keys_last | 3.70M queries/s |
