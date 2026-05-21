#pragma once

#include "gsexp/sexp.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace query_transition_bench {

inline double run_asset_common_once(gsexp::Node root, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        bool first = true;
        for (gsexp::Node asset : root.children()) {
            if (first) {
                first = false;
                continue;
            }

            gsexp::FormView asset_form(asset);
            std::optional<int> id = asset_form.get_int("id");
            std::optional<float> x = asset_form.get_float("x");
            std::optional<float> y = asset_form.get_float("y");
            std::optional<std::string_view> path = asset_form.get_string_view("path");
            if (!id || !x || !y || !path) {
                std::cerr << "asset transition benchmark missing expected field\n";
                std::exit(1);
            }
            sink += static_cast<double>(*id) + static_cast<double>(*x) +
                    static_cast<double>(*y) + static_cast<double>(path->size());
        }
    }

    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "asset transition benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline double run_many_key_once(gsexp::Node root, int iterations, std::string_view key, int expected_offset) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        bool first = true;
        for (gsexp::Node record : root.children()) {
            if (first) {
                first = false;
                continue;
            }

            gsexp::FormView record_form(record);
            std::optional<int> value = record_form.get_int(key);
            if (!value) {
                std::cerr << "many-key transition benchmark missing expected key\n";
                std::exit(1);
            }
            sink += static_cast<double>(*value - expected_offset);
        }
    }

    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "many-key transition benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void print_queries(const char* name, int items, int iterations, std::size_t fields, double seconds) {
    std::size_t queries =
        static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * fields;
    double queries_per_second = static_cast<double>(queries) / seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << seconds
              << " queries_per_second=" << queries_per_second << "\n";
}

inline void run_asset_common_transition_case(const char* cold_name,
                                             const char* warm_name,
                                             const std::string& text,
                                             int items,
                                             int warm_iterations) {
    double best_cold_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        gsexp::ParseResult result = gsexp::parse(text);
        if (!result.ok || result.root_count() == 0) {
            std::cerr << "parse failed before asset cold transition benchmark\n";
            std::exit(1);
        }
        double seconds = run_asset_common_once(result.root(0), 1);
        if (best_cold_seconds == 0.0 || seconds < best_cold_seconds)
            best_cold_seconds = seconds;
    }
    print_queries(cold_name, items, 1, 4u, best_cold_seconds);

    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before asset warm transition benchmark\n";
        std::exit(1);
    }
    run_asset_common_once(result.root(0), 1);

    double best_warm_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_asset_common_once(result.root(0), warm_iterations);
        if (best_warm_seconds == 0.0 || seconds < best_warm_seconds)
            best_warm_seconds = seconds;
    }
    print_queries(warm_name, items, warm_iterations, 4u, best_warm_seconds);
}

inline void run_many_key_transition_case(const char* cold_name,
                                         const char* warm_name,
                                         const std::string& text,
                                         int items,
                                         int warm_iterations,
                                         std::string_view key,
                                         int expected_offset) {
    double best_cold_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        gsexp::ParseResult result = gsexp::parse(text);
        if (!result.ok || result.root_count() == 0) {
            std::cerr << "parse failed before many-key cold transition benchmark\n";
            std::exit(1);
        }
        double seconds = run_many_key_once(result.root(0), 1, key, expected_offset);
        if (best_cold_seconds == 0.0 || seconds < best_cold_seconds)
            best_cold_seconds = seconds;
    }
    print_queries(cold_name, items, 1, 1u, best_cold_seconds);

    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before many-key warm transition benchmark\n";
        std::exit(1);
    }
    run_many_key_once(result.root(0), 1, key, expected_offset);

    double best_warm_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_many_key_once(result.root(0), warm_iterations, key, expected_offset);
        if (best_warm_seconds == 0.0 || seconds < best_warm_seconds)
            best_warm_seconds = seconds;
    }
    print_queries(warm_name, items, warm_iterations, 1u, best_warm_seconds);
}

} // namespace query_transition_bench
