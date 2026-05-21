#pragma once

#include "gsexp/sexp.hpp"

#include "query_bench.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace traversal_bench {

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

} // namespace traversal_bench
