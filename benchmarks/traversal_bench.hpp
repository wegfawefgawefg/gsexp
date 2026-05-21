#pragma once

#include "gsexp/sexp.hpp"

#include "query_bench.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace traversal_bench {

struct ChildSpanNode {
    std::uint32_t first_child_slot = 0;
    std::uint32_t child_count = 0;
};

struct ChildSpanProbe {
    std::vector<ChildSpanNode> nodes;
    std::vector<std::uint32_t> child_indices;
};

inline ChildSpanProbe build_child_span_probe(const gsexp::ParseStorage& storage) {
    ChildSpanProbe probe;
    probe.nodes.resize(storage.nodes.size());
    probe.child_indices.reserve(storage.nodes.size());

    for (std::uint32_t index = 0; index < storage.nodes.size(); ++index) {
        const gsexp::NodeData& node = storage.nodes[index];
        ChildSpanNode& span = probe.nodes[index];
        span.first_child_slot = static_cast<std::uint32_t>(probe.child_indices.size());

        std::uint32_t child = node.first_child;
        while (child != gsexp::invalid_node && child < storage.nodes.size()) {
            probe.child_indices.push_back(child);
            ++span.child_count;
            child = storage.nodes[child].next_sibling;
        }
    }

    return probe;
}

inline double run_ordered_code_once(gsexp::Node root, int iterations, std::size_t& visits_out) {
    double sink = 0.0;
    std::size_t visits = 0;
    std::vector<gsexp::Node> stack;
    stack.reserve(128);

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        stack.clear();
        stack.push_back(root);

        while (!stack.empty()) {
            gsexp::Node node = stack.back();
            stack.pop_back();
            if (!node.valid())
                continue;

            ++visits;
            sink += static_cast<double>(node.child_count() + node.text().size());

            gsexp::Node sibling = node.next_sibling();
            if (sibling.valid())
                stack.push_back(sibling);

            gsexp::Node child = node.first_child();
            if (child.valid())
                stack.push_back(child);
        }
    }

    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0 || visits == 0) {
        std::cerr << "ordered code traversal benchmark did no work\n";
        std::exit(1);
    }
    visits_out = visits;
    return std::chrono::duration<double>(end - start).count();
}

inline void run_ordered_code_case(const char* name, const std::string& text, int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before ordered code traversal benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    std::size_t best_visits = 0;
    for (int run = 0; run < 3; ++run) {
        std::size_t visits = 0;
        double seconds = run_ordered_code_once(result.root(0), iterations, visits);
        if (best_seconds == 0.0 || seconds < best_seconds) {
            best_seconds = seconds;
            best_visits = visits;
        }
    }

    double visits_per_second = static_cast<double>(best_visits) / best_seconds;
    std::cout << name << " visits=" << best_visits
              << " best_of=3 seconds=" << best_seconds
              << " visits_per_second=" << visits_per_second << "\n";
    query_bench::print_storage_stats(name, result);
}

inline double run_internal_ordered_code_once(const gsexp::ParseStorage& storage,
                                             std::uint32_t root_index,
                                             int iterations,
                                             std::size_t& visits_out) {
    double sink = 0.0;
    std::size_t visits = 0;
    std::vector<std::uint32_t> stack;
    stack.reserve(128);

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        stack.clear();
        stack.push_back(root_index);

        while (!stack.empty()) {
            std::uint32_t index = stack.back();
            stack.pop_back();
            if (index == gsexp::invalid_node || index >= storage.nodes.size())
                continue;

            const gsexp::NodeData& node = storage.nodes[index];
            ++visits;
            sink += static_cast<double>(node.child_count + node.text_size);

            if (node.next_sibling != gsexp::invalid_node)
                stack.push_back(node.next_sibling);
            if (node.first_child != gsexp::invalid_node)
                stack.push_back(node.first_child);
        }
    }

    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0 || visits == 0) {
        std::cerr << "internal ordered code traversal benchmark did no work\n";
        std::exit(1);
    }
    visits_out = visits;
    return std::chrono::duration<double>(end - start).count();
}

inline void run_internal_ordered_code_case(const char* name, const std::string& text, int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0 || !result.storage) {
        std::cerr << "parse failed before internal ordered code traversal benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    std::size_t best_visits = 0;
    for (int run = 0; run < 3; ++run) {
        std::size_t visits = 0;
        double seconds = run_internal_ordered_code_once(*result.storage, result.roots[0], iterations, visits);
        if (best_seconds == 0.0 || seconds < best_seconds) {
            best_seconds = seconds;
            best_visits = visits;
        }
    }

    double visits_per_second = static_cast<double>(best_visits) / best_seconds;
    std::cout << name << " visits=" << best_visits
              << " best_of=3 seconds=" << best_seconds
              << " visits_per_second=" << visits_per_second << "\n";
    query_bench::print_storage_stats(name, result);
}

inline double run_child_span_ordered_code_once(const gsexp::ParseStorage& storage,
                                               const ChildSpanProbe& probe,
                                               std::uint32_t root_index,
                                               int iterations,
                                               std::size_t& visits_out) {
    double sink = 0.0;
    std::size_t visits = 0;
    std::vector<std::uint32_t> stack;
    stack.reserve(128);

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        stack.clear();
        stack.push_back(root_index);

        while (!stack.empty()) {
            std::uint32_t index = stack.back();
            stack.pop_back();
            if (index == gsexp::invalid_node || index >= storage.nodes.size())
                continue;

            const gsexp::NodeData& node = storage.nodes[index];
            const ChildSpanNode& span = probe.nodes[index];
            ++visits;
            sink += static_cast<double>(span.child_count + node.text_size);

            std::uint32_t begin = span.first_child_slot;
            std::uint32_t end = begin + span.child_count;
            for (std::uint32_t slot = end; slot > begin; --slot)
                stack.push_back(probe.child_indices[slot - 1]);
        }
    }

    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0 || visits == 0) {
        std::cerr << "child-span ordered code traversal benchmark did no work\n";
        std::exit(1);
    }
    visits_out = visits;
    return std::chrono::duration<double>(end - start).count();
}

inline void run_child_span_ordered_code_case(const char* name, const std::string& text, int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0 || !result.storage) {
        std::cerr << "parse failed before child-span ordered code traversal benchmark: " << name << "\n";
        std::exit(1);
    }

    auto build_start = std::chrono::steady_clock::now();
    ChildSpanProbe probe = build_child_span_probe(*result.storage);
    auto build_end = std::chrono::steady_clock::now();
    double build_seconds = std::chrono::duration<double>(build_end - build_start).count();

    double best_seconds = 0.0;
    std::size_t best_visits = 0;
    for (int run = 0; run < 3; ++run) {
        std::size_t visits = 0;
        double seconds = run_child_span_ordered_code_once(*result.storage,
                                                          probe,
                                                          result.roots[0],
                                                          iterations,
                                                          visits);
        if (best_seconds == 0.0 || seconds < best_seconds) {
            best_seconds = seconds;
            best_visits = visits;
        }
    }

    double visits_per_second = static_cast<double>(best_visits) / best_seconds;
    std::size_t span_bytes = probe.nodes.capacity() * sizeof(ChildSpanNode) +
                             probe.child_indices.capacity() * sizeof(std::uint32_t);
    std::cout << name << " visits=" << best_visits
              << " best_of=3 seconds=" << best_seconds
              << " visits_per_second=" << visits_per_second
              << " build_seconds=" << build_seconds
              << " child_span_bytes=" << span_bytes << "\n";
    query_bench::print_storage_stats(name, result);
}

inline double run_internal_asset_children_once(const gsexp::ParseStorage& storage,
                                               std::uint32_t root_index,
                                               int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        std::uint32_t asset_index = storage.nodes[root_index].first_child;
        while (asset_index != gsexp::invalid_node && asset_index < storage.nodes.size()) {
            const gsexp::NodeData& asset = storage.nodes[asset_index];
            if (asset.type == gsexp::ValueType::List) {
                std::uint32_t field_index = asset.first_child;
                while (field_index != gsexp::invalid_node && field_index < storage.nodes.size()) {
                    const gsexp::NodeData& field = storage.nodes[field_index];
                    sink += static_cast<double>(field.child_count);
                    if (field.first_child != gsexp::invalid_node && field.first_child < storage.nodes.size())
                        sink += static_cast<double>(storage.nodes[field.first_child].text_size);
                    field_index = field.next_sibling;
                }
            }
            asset_index = asset.next_sibling;
        }
    }

    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "internal asset child traversal benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_internal_asset_children_case(const char* name,
                                             const std::string& text,
                                             int items,
                                             int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0 || !result.storage) {
        std::cerr << "parse failed before internal asset child traversal benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_internal_asset_children_once(*result.storage, result.roots[0], iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t visits = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 9u;
    double visits_per_second = static_cast<double>(visits) / best_seconds;
    std::cout << name << " items=" << items << " visits=" << visits
              << " best_of=3 seconds=" << best_seconds
              << " visits_per_second=" << visits_per_second << "\n";
    query_bench::print_storage_stats(name, result);
}

inline double run_child_span_asset_children_once(const gsexp::ParseStorage& storage,
                                                 const ChildSpanProbe& probe,
                                                 std::uint32_t root_index,
                                                 int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const ChildSpanNode& root_span = probe.nodes[root_index];
        std::uint32_t root_begin = root_span.first_child_slot;
        std::uint32_t root_end = root_begin + root_span.child_count;
        for (std::uint32_t asset_slot = root_begin; asset_slot < root_end; ++asset_slot) {
            std::uint32_t asset_index = probe.child_indices[asset_slot];
            if (asset_index >= storage.nodes.size())
                continue;

            const gsexp::NodeData& asset = storage.nodes[asset_index];
            if (asset.type != gsexp::ValueType::List)
                continue;

            const ChildSpanNode& asset_span = probe.nodes[asset_index];
            std::uint32_t field_begin = asset_span.first_child_slot;
            std::uint32_t field_end = field_begin + asset_span.child_count;
            for (std::uint32_t field_slot = field_begin; field_slot < field_end; ++field_slot) {
                std::uint32_t field_index = probe.child_indices[field_slot];
                if (field_index >= storage.nodes.size())
                    continue;

                const ChildSpanNode& field_span = probe.nodes[field_index];
                sink += static_cast<double>(field_span.child_count);
                if (field_span.child_count > 0)
                    sink += static_cast<double>(storage.nodes[probe.child_indices[field_span.first_child_slot]].text_size);
            }
        }
    }

    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "child-span asset child traversal benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_child_span_asset_children_case(const char* name,
                                               const std::string& text,
                                               int items,
                                               int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0 || !result.storage) {
        std::cerr << "parse failed before child-span asset child traversal benchmark: " << name << "\n";
        std::exit(1);
    }

    auto build_start = std::chrono::steady_clock::now();
    ChildSpanProbe probe = build_child_span_probe(*result.storage);
    auto build_end = std::chrono::steady_clock::now();
    double build_seconds = std::chrono::duration<double>(build_end - build_start).count();

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_child_span_asset_children_once(*result.storage,
                                                            probe,
                                                            result.roots[0],
                                                            iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t visits = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 9u;
    double visits_per_second = static_cast<double>(visits) / best_seconds;
    std::size_t span_bytes = probe.nodes.capacity() * sizeof(ChildSpanNode) +
                             probe.child_indices.capacity() * sizeof(std::uint32_t);
    std::cout << name << " items=" << items << " visits=" << visits
              << " best_of=3 seconds=" << best_seconds
              << " visits_per_second=" << visits_per_second
              << " build_seconds=" << build_seconds
              << " child_span_bytes=" << span_bytes << "\n";
    query_bench::print_storage_stats(name, result);
}

} // namespace traversal_bench
