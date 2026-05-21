#pragma once

#include "gsexp/sexp.hpp"

#include "query_bench.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace query_bench {

inline double run_mixed_layout_access_once(gsexp::Node root, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        bool first = true;
        for (gsexp::Node layout : root.children()) {
            if (first) {
                first = false;
                continue;
            }

            std::size_t ordered_children = 0;
            for (gsexp::Node child : layout.children()) {
                if (!child.valid())
                    continue;
                ++ordered_children;
                sink += static_cast<double>(child.child_count());
            }

            gsexp::FormView layout_form(layout);
            std::optional<int> id = layout_form.get_int("id");
            gsexp::Node title = layout_form.find_arg("title", 0);
            gsexp::Node play_x = layout_form.find_arg("play", 1);
            gsexp::Node settings = layout_form.find("settings");
            if (!id || !title.valid() || !play_x.valid() || !settings.valid() ||
                ordered_children != 6) {
                std::cerr << "mixed layout benchmark missing expected field\n";
                std::exit(1);
            }

            sink += static_cast<double>(*id) + static_cast<double>(title.text().size()) +
                    static_cast<double>(play_x.text().size()) +
                    static_cast<double>(settings.child_count());
        }
    }

    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "mixed layout benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_mixed_layout_access_case(const char* name,
                                         const std::string& text,
                                         int items,
                                         int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before mixed layout benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_mixed_layout_access_once(result.root(0), iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t operations =
        static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 10u;
    double operations_per_second = static_cast<double>(operations) / best_seconds;
    std::cout << name << " items=" << items << " operations=" << operations
              << " best_of=3 seconds=" << best_seconds
              << " operations_per_second=" << operations_per_second << "\n";
    print_storage_stats(name, result);
}

inline void print_mixed_operations(const char* name, int items, int iterations, double seconds) {
    std::size_t operations =
        static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 10u;
    double operations_per_second = static_cast<double>(operations) / seconds;
    std::cout << name << " items=" << items << " operations=" << operations
              << " best_of=3 seconds=" << seconds
              << " operations_per_second=" << operations_per_second << "\n";
}

inline void run_mixed_layout_transition_case(const char* cold_name,
                                             const char* warm_name,
                                             const std::string& text,
                                             int items,
                                             int warm_iterations) {
    double best_cold_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        gsexp::ParseResult result = gsexp::parse(text);
        if (!result.ok || result.root_count() == 0) {
            std::cerr << "parse failed before mixed layout cold benchmark\n";
            std::exit(1);
        }

        double seconds = run_mixed_layout_access_once(result.root(0), 1);
        if (best_cold_seconds == 0.0 || seconds < best_cold_seconds)
            best_cold_seconds = seconds;
    }
    print_mixed_operations(cold_name, items, 1, best_cold_seconds);

    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before mixed layout warm benchmark\n";
        std::exit(1);
    }
    run_mixed_layout_access_once(result.root(0), 1);

    double best_warm_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_mixed_layout_access_once(result.root(0), warm_iterations);
        if (best_warm_seconds == 0.0 || seconds < best_warm_seconds)
            best_warm_seconds = seconds;
    }
    print_mixed_operations(warm_name, items, warm_iterations, best_warm_seconds);
}

} // namespace query_bench
