#pragma once

#include "gsexp/sexp.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace query_bench {

inline void print_storage_stats(const char* name, const gsexp::ParseResult& result) {
    gsexp::StorageStats stats = result.storage_stats();
    std::cout << name << "_stats"
              << " roots=" << stats.root_count
              << " nodes=" << stats.node_count
              << " node_capacity=" << stats.node_capacity
              << " node_data_bytes=" << stats.node_data_bytes
              << " node_bytes=" << stats.node_bytes
              << " node_bytes_per_source_byte=" << stats.node_bytes_per_source_byte
              << " source_bytes=" << stats.source_bytes
              << " decoded_strings=" << stats.decoded_string_count
              << " decoded_bytes=" << stats.decoded_string_bytes
              << " child_indexes=" << stats.child_index_count
              << " child_index_capacity=" << stats.child_index_capacity
              << " child_index_cache_bytes=" << stats.child_index_cache_bytes
              << " child_index_lookup_capacity=" << stats.child_index_lookup_capacity
              << " child_index_lookup_bytes=" << stats.child_index_lookup_bytes
              << " child_index_entries=" << stats.child_index_entry_count
              << " child_index_entry_capacity=" << stats.child_index_entry_capacity
              << " child_index_entry_bytes=" << stats.child_index_entry_bytes
              << " float_cache_capacity=" << stats.float_cache_capacity
              << " float_cache_bytes=" << stats.float_cache_bytes
              << " approximate_bytes=" << stats.approximate_bytes << "\n";
}

enum class QueryMode {
    Common,
    First,
    Last,
    Missing,
    StringView,
    TextOnly,
    SymbolCompare,
    ManyLast,
    FindOnly,
    ChildAtValue,
    FindArgValue,
};

inline double run_query_once(gsexp::Node root, int iterations, QueryMode mode) {
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
            if (mode == QueryMode::TextOnly) {
                bool field_first = true;
                for (gsexp::Node field : asset.children()) {
                    if (field_first) {
                        field_first = false;
                        continue;
                    }
                    gsexp::Node value = field.child_at(1);
                    if (!value.valid()) {
                        std::cerr << "text-only query benchmark missing value\n";
                        std::exit(1);
                    }
                    sink += static_cast<double>(value.text().size());
                }
            } else if (mode == QueryMode::SymbolCompare) {
                bool saw_path = false;
                bool field_first = true;
                for (gsexp::Node field : asset.children()) {
                    if (field_first) {
                        field_first = false;
                        continue;
                    }
                    gsexp::Node head = field.head();
                    if (head.is_atom("path"))
                        saw_path = true;
                    sink += head.is_atom("missing") ? 2.0 : 1.0;
                }
                if (!saw_path) {
                    std::cerr << "symbol-compare query benchmark missing path\n";
                    std::exit(1);
                }
            } else if (mode == QueryMode::Common) {
                std::optional<int> id = asset_form.get_int("id");
                std::optional<float> x = asset_form.get_float("x");
                std::optional<float> y = asset_form.get_float("y");
                std::optional<std::string_view> path = asset_form.get_string_view("path");
                if (!id || !x || !y || !path) {
                    std::cerr << "query benchmark missing expected field\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*id) + static_cast<double>(*x) +
                        static_cast<double>(*y) + static_cast<double>(path->size());
            } else if (mode == QueryMode::First) {
                std::optional<int> id = asset_form.get_int("id");
                if (!id) {
                    std::cerr << "first query benchmark missing id\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*id);
            } else if (mode == QueryMode::Last) {
                std::optional<float> h = asset_form.get_float("h");
                if (!h) {
                    std::cerr << "last query benchmark missing h\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*h);
            } else if (mode == QueryMode::Missing) {
                std::optional<int> missing = asset_form.get_int("missing");
                if (missing) {
                    std::cerr << "missing query benchmark found unexpected field\n";
                    std::exit(1);
                }
                sink += 1.0;
            } else if (mode == QueryMode::StringView) {
                std::optional<std::string_view> path = asset_form.get_string_view("path");
                if (!path) {
                    std::cerr << "string_view query benchmark missing path\n";
                    std::exit(1);
                }
                sink += static_cast<double>(path->size());
            } else {
                if (mode == QueryMode::FindOnly) {
                    gsexp::Node node = asset_form.find("key_23");
                    if (!node.valid()) {
                        std::cerr << "find-only query benchmark missing key_23\n";
                        std::exit(1);
                    }
                    sink += static_cast<double>(node.child_count());
                    continue;
                }
                if (mode == QueryMode::ChildAtValue) {
                    gsexp::Node node = asset_form.find("key_23");
                    if (!node.valid()) {
                        std::cerr << "child-at query benchmark missing key_23\n";
                        std::exit(1);
                    }
                    gsexp::Node value = node.child_at(1);
                    if (!value.valid()) {
                        std::cerr << "child-at query benchmark missing value\n";
                        std::exit(1);
                    }
                    sink += static_cast<double>(value.text().size());
                    continue;
                }
                if (mode == QueryMode::FindArgValue) {
                    gsexp::Node value = asset_form.find_arg("key_23", 0);
                    if (!value.valid()) {
                        std::cerr << "find-arg query benchmark missing value\n";
                        std::exit(1);
                    }
                    sink += static_cast<double>(value.text().size());
                    continue;
                }
                std::optional<int> value = asset_form.get_int("key_23");
                if (!value) {
                    std::cerr << "many-key query benchmark missing key_23\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*value);
            }
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "query benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_query_case(const char* name,
                           const std::string& text,
                           int items,
                           int iterations,
                           QueryMode mode) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before query benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_query_once(result.root(0), iterations, mode);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t fields_per_item = mode == QueryMode::Common ? 4u : 1u;
    if (mode == QueryMode::TextOnly || mode == QueryMode::SymbolCompare)
        fields_per_item = 8u;
    std::size_t queries =
        static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * fields_per_item;
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";
    print_storage_stats(name, result);
}

inline double run_many_key_get_once(gsexp::Node root,
                                    int iterations,
                                    std::string_view key,
                                    int expected_offset) {
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
                std::cerr << "many-key width benchmark missing expected key\n";
                std::exit(1);
            }
            sink += static_cast<double>(*value - expected_offset);
        }
    }

    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "many-key width benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_many_key_get_case(const char* name,
                                  const std::string& text,
                                  int items,
                                  int iterations,
                                  std::string_view key,
                                  int expected_offset) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before many-key width benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_many_key_get_once(result.root(0), iterations, key, expected_offset);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t queries = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations);
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";
    print_storage_stats(name, result);
}

inline double run_asset_database_query_once(gsexp::Node root, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        bool first = true;
        for (gsexp::Node record : root.children()) {
            if (first) {
                first = false;
                continue;
            }
            if (!record.is_list())
                continue;

            gsexp::Node head = record.head();
            if (!head.valid())
                continue;
            if (!head.is_atom("texture") && !head.is_atom("sound") && !head.is_atom("prefab"))
                continue;

            gsexp::FormView record_form(record);
            std::optional<std::string_view> id = record_form.get_string_view("id");
            std::optional<std::string_view> path = record_form.get_string_view("path");
            if (!id || !path) {
                std::cerr << "asset database query missing id/path\n";
                std::exit(1);
            }
            sink += static_cast<double>(head.text().size()) + static_cast<double>(id->size()) +
                    static_cast<double>(path->size());

            if (head.is_atom("texture")) {
                gsexp::FormView size(record_form.find("size"));
                std::optional<int> w = size.get_int("w");
                std::optional<int> h = size.get_int("h");
                if (!w || !h) {
                    std::cerr << "asset database texture query missing size\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*w + *h);
            } else if (head.is_atom("sound")) {
                std::optional<float> volume = record_form.get_float("volume");
                std::optional<std::string_view> stream = record_form.get_string_view("stream");
                if (!volume || !stream) {
                    std::cerr << "asset database sound query missing volume/stream\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*volume) + static_cast<double>(stream->size());
            } else {
                gsexp::FormView bounds(record_form.find("bounds"));
                std::optional<int> x = bounds.get_int("x");
                std::optional<int> y = bounds.get_int("y");
                if (!x || !y) {
                    std::cerr << "asset database prefab query missing bounds\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*x + *y);
            }
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "asset database query benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_asset_database_query_case(const char* name,
                                          const std::string& text,
                                          int items,
                                          int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before asset database query benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_asset_database_query_once(result.root(0), iterations);
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

inline double run_nested_find_arg_once(gsexp::Node root, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        bool first = true;
        for (gsexp::Node layout : root.children()) {
            if (first) {
                first = false;
                continue;
            }

            gsexp::FormView layout_form(layout);
            std::optional<int> id = layout_form.get_int("id");
            gsexp::Node title = layout_form.find_arg("title", 0);
            gsexp::Node play_x = layout_form.find_arg("play", 1);
            gsexp::Node settings_w = layout_form.find_arg("settings", 3);
            gsexp::Node credits_h = layout_form.find_arg("credits", 4);
            if (!id || !title.valid() || !play_x.valid() || !settings_w.valid() || !credits_h.valid()) {
                std::cerr << "nested find_arg query missing expected field\n";
                std::exit(1);
            }

            sink += static_cast<double>(*id) + static_cast<double>(title.text().size()) +
                    static_cast<double>(play_x.text().size()) +
                    static_cast<double>(settings_w.text().size()) +
                    static_cast<double>(credits_h.text().size());
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "nested find_arg query benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_nested_find_arg_case(const char* name,
                                     const std::string& text,
                                     int items,
                                     int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before nested find_arg benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_nested_find_arg_once(result.root(0), iterations);
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

inline double run_child_iteration_once(gsexp::Node root, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (gsexp::Node asset : root.children()) {
            if (!asset.is_list())
                continue;

            for (gsexp::Node field : asset.children()) {
                sink += static_cast<double>(field.child_count());
                gsexp::Node head = field.head();
                if (head.valid())
                    sink += static_cast<double>(head.text().size());
            }
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "child iteration benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_child_iteration_case(const char* name,
                                     const std::string& text,
                                     int items,
                                     int iterations) {
    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok || result.root_count() == 0) {
        std::cerr << "parse failed before child iteration benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_child_iteration_once(result.root(0), iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t visits = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 9u;
    double visits_per_second = static_cast<double>(visits) / best_seconds;
    std::cout << name << " items=" << items << " visits=" << visits
              << " best_of=3 seconds=" << best_seconds
              << " visits_per_second=" << visits_per_second << "\n";
    print_storage_stats(name, result);
}

} // namespace query_bench
