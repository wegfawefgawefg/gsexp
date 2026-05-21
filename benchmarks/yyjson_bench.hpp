#pragma once

#include "bench_data.hpp"

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#if GSEXP_HAVE_YYJSON
#include "yyjson.h"
#endif

namespace yyjson_bench {

#if GSEXP_HAVE_YYJSON
inline double run_once(const char* name, const std::string& text, int iterations) {
    std::size_t parsed_roots = 0;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        yyjson_doc* doc = yyjson_read(text.data(), text.size(), YYJSON_READ_NOFLAG);
        if (!doc) {
            std::cerr << "yyjson parse failed in benchmark case: " << name << "\n";
            std::exit(1);
        }
        if (yyjson_doc_get_root(doc))
            ++parsed_roots;
        yyjson_doc_free(doc);
    }
    auto end = std::chrono::steady_clock::now();
    if (parsed_roots == 0) {
        std::cerr << "yyjson benchmark parsed no roots\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_parse_case(const char* name, const std::string& text, int iterations) {
    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_once(name, text, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    double mib = (static_cast<double>(text.size()) * iterations) / (1024.0 * 1024.0);
    std::cout << name << " bytes=" << text.size() << " iterations=" << iterations
              << " best_of=3 seconds=" << best_seconds
              << " mib_per_second=" << (mib / best_seconds) << "\n";
}

inline double run_small_files_once(const char* name,
                                   const std::vector<std::string>& configs,
                                   int iterations) {
    std::size_t parsed_roots = 0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (const std::string& text : configs) {
            yyjson_doc* doc = yyjson_read(text.data(), text.size(), YYJSON_READ_NOFLAG);
            if (!doc) {
                std::cerr << "yyjson parse failed in benchmark case: " << name << "\n";
                std::exit(1);
            }
            if (yyjson_doc_get_root(doc))
                ++parsed_roots;
            yyjson_doc_free(doc);
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (parsed_roots == 0) {
        std::cerr << "yyjson benchmark parsed no roots\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_small_files_case(const char* name, int files, int iterations) {
    std::vector<std::string> configs = bench_data::make_small_config_jsons(files);
    std::size_t bytes = 0;
    for (const std::string& text : configs)
        bytes += text.size();

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_small_files_once(name, configs, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    double mib = (static_cast<double>(bytes) * iterations) / (1024.0 * 1024.0);
    std::cout << name << " files=" << files << " bytes=" << bytes
              << " iterations=" << iterations << " best_of=3 seconds=" << best_seconds
              << " mib_per_second=" << (mib / best_seconds) << "\n";
}

inline yyjson_val* require_member(yyjson_val* object, const char* key, const char* name) {
    yyjson_val* value = yyjson_obj_get(object, key);
    if (!value) {
        std::cerr << "yyjson missing expected field in benchmark case: " << name << "\n";
        std::exit(1);
    }
    return value;
}

inline double run_asset_query_once(yyjson_val* assets, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        yyjson_arr_iter iter = yyjson_arr_iter_with(assets);
        yyjson_val* asset = nullptr;
        while ((asset = yyjson_arr_iter_next(&iter))) {
            yyjson_val* id = require_member(asset, "id", "yyjson_query_assets_10k");
            yyjson_val* x = require_member(asset, "x", "yyjson_query_assets_10k");
            yyjson_val* y = require_member(asset, "y", "yyjson_query_assets_10k");
            yyjson_val* path = require_member(asset, "path", "yyjson_query_assets_10k");
            const char* path_text = yyjson_get_str(path);
            if (!path_text) {
                std::cerr << "yyjson path is not a string\n";
                std::exit(1);
            }
            sink += static_cast<double>(yyjson_get_int(id)) + yyjson_get_num(x) +
                    yyjson_get_num(y) + static_cast<double>(std::strlen(path_text));
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "yyjson query benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline double run_many_key_query_once(yyjson_val* records, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        yyjson_arr_iter iter = yyjson_arr_iter_with(records);
        yyjson_val* record = nullptr;
        while ((record = yyjson_arr_iter_next(&iter))) {
            yyjson_val* value = require_member(record, "key_23", "yyjson_query_many_keys_last");
            sink += static_cast<double>(yyjson_get_int(value));
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "yyjson many-key query benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline double run_asset_database_query_once(yyjson_val* records, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        yyjson_arr_iter iter = yyjson_arr_iter_with(records);
        yyjson_val* record = nullptr;
        while ((record = yyjson_arr_iter_next(&iter))) {
            yyjson_val* kind = require_member(record, "kind", "yyjson_query_asset_database_5k");
            yyjson_val* id = require_member(record, "id", "yyjson_query_asset_database_5k");
            yyjson_val* path = require_member(record, "path", "yyjson_query_asset_database_5k");
            const char* kind_text = yyjson_get_str(kind);
            const char* id_text = yyjson_get_str(id);
            const char* path_text = yyjson_get_str(path);
            if (!kind_text || !id_text || !path_text) {
                std::cerr << "yyjson asset database query expected string fields\n";
                std::exit(1);
            }

            sink += static_cast<double>(std::strlen(kind_text)) + static_cast<double>(std::strlen(id_text)) +
                    static_cast<double>(std::strlen(path_text));

            if (std::strcmp(kind_text, "texture") == 0) {
                yyjson_val* size = require_member(record, "size", "yyjson_query_asset_database_5k");
                yyjson_val* w = require_member(size, "w", "yyjson_query_asset_database_5k");
                yyjson_val* h = require_member(size, "h", "yyjson_query_asset_database_5k");
                sink += static_cast<double>(yyjson_get_int(w) + yyjson_get_int(h));
            } else if (std::strcmp(kind_text, "sound") == 0) {
                yyjson_val* volume = require_member(record, "volume", "yyjson_query_asset_database_5k");
                yyjson_val* stream = require_member(record, "stream", "yyjson_query_asset_database_5k");
                sink += yyjson_get_num(volume) + (yyjson_get_bool(stream) ? 4.0 : 5.0);
            } else {
                yyjson_val* bounds = require_member(record, "bounds", "yyjson_query_asset_database_5k");
                yyjson_val* x = require_member(bounds, "x", "yyjson_query_asset_database_5k");
                yyjson_val* y = require_member(bounds, "y", "yyjson_query_asset_database_5k");
                sink += static_cast<double>(yyjson_get_int(x) + yyjson_get_int(y));
            }
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "yyjson asset database query benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline void run_query_case(const char* name,
                           const std::string& text,
                           const char* array_key,
                           int items,
                           int iterations,
                           bool many_keys) {
    yyjson_doc* doc = yyjson_read(text.data(), text.size(), YYJSON_READ_NOFLAG);
    if (!doc) {
        std::cerr << "yyjson parse failed before query benchmark: " << name << "\n";
        std::exit(1);
    }

    yyjson_val* root = yyjson_doc_get_root(doc);
    yyjson_val* array = require_member(root, array_key, name);

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = many_keys ? run_many_key_query_once(array, iterations)
                                   : run_asset_query_once(array, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t fields_per_item = many_keys ? 1u : 4u;
    std::size_t queries =
        static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * fields_per_item;
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";

    yyjson_doc_free(doc);
}

inline void run_asset_database_query_case(const char* name, const std::string& text, int items, int iterations) {
    yyjson_doc* doc = yyjson_read(text.data(), text.size(), YYJSON_READ_NOFLAG);
    if (!doc) {
        std::cerr << "yyjson parse failed before query benchmark: " << name << "\n";
        std::exit(1);
    }

    yyjson_val* root = yyjson_doc_get_root(doc);
    yyjson_val* database = require_member(root, "asset_database", name);
    yyjson_val* records = require_member(database, "assets", name);

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_asset_database_query_once(records, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t queries = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 5u;
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";

    yyjson_doc_free(doc);
}
#endif

} // namespace yyjson_bench
