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
