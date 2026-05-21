#pragma once

#include "gsexp/sexp.hpp"

#include "internal_probe.hpp"
#include "query_bench.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace query_bench {

inline void run_internal_asset_query_case(const char* name,
                                          const std::string& text,
                                          int items,
                                          int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before internal asset query probe: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = internal_probe::run_asset_query_once(result, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t queries = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 4u;
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";
    print_storage_stats(name, result);
}

inline void run_internal_ordered_asset_query_case(const char* name,
                                                  const std::string& text,
                                                  int items,
                                                  int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before internal ordered asset query probe: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = internal_probe::run_ordered_asset_query_once(result, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t queries = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 4u;
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";
    print_storage_stats(name, result);
}

inline void run_internal_asset_database_query_case(const char* name,
                                                   const std::string& text,
                                                   int items,
                                                   int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before internal asset database query probe: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = internal_probe::run_asset_database_query_once(result, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t queries = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 5u;
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";
    print_storage_stats(name, result);
}

inline void run_internal_nested_find_arg_case(const char* name,
                                              const std::string& text,
                                              int items,
                                              int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before internal nested find_arg probe: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = internal_probe::run_nested_find_arg_once(result, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t queries = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 5u;
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";
    print_storage_stats(name, result);
}

} // namespace query_bench
