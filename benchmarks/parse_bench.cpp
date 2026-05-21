#include "gsexp/sexp.hpp"

#include "bench_data.hpp"
#include "scan_probe.hpp"

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if GSEXP_HAVE_YYJSON
#include "yyjson.h"
#endif

namespace {

namespace data = bench_data;

std::string read_cpu_model() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        std::string prefix = "model name";
        if (line.compare(0, prefix.size(), prefix) != 0)
            continue;

        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            return line;
        std::size_t value_start = colon + 1;
        while (value_start < line.size() && line[value_start] == ' ')
            ++value_start;
        return line.substr(value_start);
    }

    return "unknown";
}

void print_benchmark_context() {
    std::cout << "benchmark_context"
              << " cpu_model=\"" << read_cpu_model() << "\""
#if defined(__SSE4_2__)
              << " sse4_2=compiled"
#else
              << " sse4_2=not_compiled"
#endif
#if defined(__AVX2__)
              << " avx2=compiled"
#else
              << " avx2=not_compiled"
#endif
#if defined(GSEXP_HAVE_YYJSON)
              << " yyjson=enabled"
#else
              << " yyjson=disabled"
#endif
              << "\n";
}

void print_storage_stats(const char* name, const gsexp::ParseResult& result) {
    gsexp::StorageStats stats = result.storage_stats();
    std::cout << name << "_stats"
              << " roots=" << stats.root_count
              << " nodes=" << stats.node_count
              << " node_capacity=" << stats.node_capacity
              << " source_bytes=" << stats.source_bytes
              << " decoded_strings=" << stats.decoded_string_count
              << " decoded_bytes=" << stats.decoded_string_bytes
              << " child_indexes=" << stats.child_index_count
              << " child_index_entries=" << stats.child_index_entry_count
              << " approximate_bytes=" << stats.approximate_bytes << "\n";
}

double run_once(const char* name, const std::string& text, int iterations, bool owned) {
    std::size_t parsed_roots = 0;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        gsexp::ParseResult result = owned ? gsexp::parse_owned(std::string(text)) : gsexp::parse(text);
        if (!result.ok) {
            std::cerr << "parse failed in benchmark case: " << name << "\n";
            std::exit(1);
        }
        parsed_roots += result.root_count();
    }
    auto end = std::chrono::steady_clock::now();
    if (parsed_roots == 0) {
        std::cerr << "benchmark parsed no roots\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

double best_parse_seconds(const char* name, const std::string& text, int iterations, bool owned) {
    double best_seconds = 0.0;

    for (int run = 0; run < 3; ++run) {
        double seconds = run_once(name, text, iterations, owned);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    return best_seconds;
}

void run_parse_case(const char* name, const std::string& text, int iterations) {
    double best_seconds = best_parse_seconds(name, text, iterations, false);
    double mib = (static_cast<double>(text.size()) * iterations) / (1024.0 * 1024.0);
    std::cout << name << " bytes=" << text.size() << " iterations=" << iterations
              << " best_of=3 seconds=" << best_seconds
              << " mib_per_second=" << (mib / best_seconds) << "\n";

    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok) {
        std::cerr << "parse failed for stats case: " << name << "\n";
        std::exit(1);
    }
    print_storage_stats(name, result);
}

void run_parse_owned_case(const char* name, const std::string& text, int iterations) {
    double best_seconds = best_parse_seconds(name, text, iterations, true);
    double mib = (static_cast<double>(text.size()) * iterations) / (1024.0 * 1024.0);
    std::cout << name << " bytes=" << text.size() << " iterations=" << iterations
              << " best_of=3 seconds=" << best_seconds
              << " mib_per_second=" << (mib / best_seconds) << "\n";
}

void run_asset_case(const char* name, int items, int iterations) {
    run_parse_case(name, data::make_asset_data(items), iterations);
}

double run_small_files_once(const char* name, const std::vector<std::string>& configs, int iterations) {
    std::size_t parsed_roots = 0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (const std::string& text : configs) {
            gsexp::ParseResult result = gsexp::parse(text);
            if (!result.ok) {
                std::cerr << "parse failed in benchmark case: " << name << "\n";
                std::exit(1);
            }
            parsed_roots += result.root_count();
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (parsed_roots == 0) {
        std::cerr << "benchmark parsed no roots\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

void run_small_files_case(const char* name, int files, int iterations) {
    std::vector<std::string> configs = data::make_small_configs(files);
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

#if GSEXP_HAVE_YYJSON
double run_yyjson_once(const char* name, const std::string& text, int iterations) {
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

void run_yyjson_parse_case(const char* name, const std::string& text, int iterations) {
    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_yyjson_once(name, text, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    double mib = (static_cast<double>(text.size()) * iterations) / (1024.0 * 1024.0);
    std::cout << name << " bytes=" << text.size() << " iterations=" << iterations
              << " best_of=3 seconds=" << best_seconds
              << " mib_per_second=" << (mib / best_seconds) << "\n";
}

double run_yyjson_small_files_once(const char* name,
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

void run_yyjson_small_files_case(const char* name, int files, int iterations) {
    std::vector<std::string> configs = data::make_small_config_jsons(files);
    std::size_t bytes = 0;
    for (const std::string& text : configs)
        bytes += text.size();

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_yyjson_small_files_once(name, configs, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    double mib = (static_cast<double>(bytes) * iterations) / (1024.0 * 1024.0);
    std::cout << name << " files=" << files << " bytes=" << bytes
              << " iterations=" << iterations << " best_of=3 seconds=" << best_seconds
              << " mib_per_second=" << (mib / best_seconds) << "\n";
}
#endif

enum class QueryMode {
    Common,
    First,
    Last,
    Missing,
    StringView,
    ManyLast,
};

double run_query_once(gsexp::Node root, int iterations, QueryMode mode) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        bool first = true;
        for (gsexp::Node asset : root.children()) {
            if (first) {
                first = false;
                continue;
            }
            if (mode == QueryMode::Common) {
                std::optional<int> id = gsexp::extract_int(asset, "id");
                std::optional<float> x = gsexp::extract_float(asset, "x");
                std::optional<float> y = gsexp::extract_float(asset, "y");
                std::optional<std::string> path = gsexp::extract_string(asset, "path");
                if (!id || !x || !y || !path) {
                    std::cerr << "query benchmark missing expected field\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*id) + static_cast<double>(*x) +
                        static_cast<double>(*y) + static_cast<double>(path->size());
            } else if (mode == QueryMode::First) {
                std::optional<int> id = gsexp::extract_int(asset, "id");
                if (!id) {
                    std::cerr << "first query benchmark missing id\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*id);
            } else if (mode == QueryMode::Last) {
                std::optional<float> h = gsexp::extract_float(asset, "h");
                if (!h) {
                    std::cerr << "last query benchmark missing h\n";
                    std::exit(1);
                }
                sink += static_cast<double>(*h);
            } else if (mode == QueryMode::Missing) {
                std::optional<int> missing = gsexp::extract_int(asset, "missing");
                if (missing) {
                    std::cerr << "missing query benchmark found unexpected field\n";
                    std::exit(1);
                }
                sink += 1.0;
            } else if (mode == QueryMode::StringView) {
                std::optional<std::string_view> path = gsexp::extract_string_view(asset, "path");
                if (!path) {
                    std::cerr << "string_view query benchmark missing path\n";
                    std::exit(1);
                }
                sink += static_cast<double>(path->size());
            } else {
                std::optional<int> value = gsexp::extract_int(asset, "key_23");
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

void run_query_case(const char* name, const std::string& text, int items, int iterations, QueryMode mode) {
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
    std::size_t queries =
        static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * fields_per_item;
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";
    print_storage_stats(name, result);
}

#if GSEXP_HAVE_YYJSON
yyjson_val* require_yyjson_member(yyjson_val* object, const char* key, const char* name) {
    yyjson_val* value = yyjson_obj_get(object, key);
    if (!value) {
        std::cerr << "yyjson missing expected field in benchmark case: " << name << "\n";
        std::exit(1);
    }
    return value;
}

double run_yyjson_asset_query_once(yyjson_val* assets, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        yyjson_arr_iter iter = yyjson_arr_iter_with(assets);
        yyjson_val* asset = nullptr;
        while ((asset = yyjson_arr_iter_next(&iter))) {
            yyjson_val* id = require_yyjson_member(asset, "id", "yyjson_query_assets_10k");
            yyjson_val* x = require_yyjson_member(asset, "x", "yyjson_query_assets_10k");
            yyjson_val* y = require_yyjson_member(asset, "y", "yyjson_query_assets_10k");
            yyjson_val* path = require_yyjson_member(asset, "path", "yyjson_query_assets_10k");
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

double run_yyjson_many_key_query_once(yyjson_val* records, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        yyjson_arr_iter iter = yyjson_arr_iter_with(records);
        yyjson_val* record = nullptr;
        while ((record = yyjson_arr_iter_next(&iter))) {
            yyjson_val* value = require_yyjson_member(record, "key_23", "yyjson_query_many_keys_last");
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

void run_yyjson_query_case(const char* name,
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
    yyjson_val* array = require_yyjson_member(root, array_key, name);

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = many_keys ? run_yyjson_many_key_query_once(array, iterations)
                                   : run_yyjson_asset_query_once(array, iterations);
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
#endif

} // namespace

int main() {
    print_benchmark_context();

    std::string assets_10k = data::make_asset_data(10000);
    std::string assets_50k = data::make_asset_data(50000);
    std::string asset_database_5k = data::make_asset_database_data(5000);
    std::string wide_10k = data::make_wide_data(10000);
    std::string asset_json_10k = data::make_asset_json(10000);
    std::string asset_json_50k = data::make_asset_json(50000);
    std::string asset_database_json_5k = data::make_asset_database_json(5000);
    std::string wide_json_10k = data::make_wide_json(10000);
    std::string many_keys_json = data::make_many_keys_json(5000, 24);

    run_asset_case("assets_1k", 1000, 500);
    run_parse_case("assets_10k", assets_10k, 50);
    run_parse_owned_case("assets_10k_owned", assets_10k, 50);
    run_parse_case("assets_50k", assets_50k, 10);
    run_parse_owned_case("assets_50k_owned", assets_50k, 10);
    run_parse_case("asset_database_5k", asset_database_5k, 50);
    run_small_files_case("small_files_1k", 1000, 500);
    run_parse_case("strings_plain_5k", data::make_string_data(5000, false, 12), 50);
    run_parse_case("strings_escaped_5k", data::make_string_data(5000, true, 12), 50);
    run_parse_case("deep_1k", data::make_deep_data(1000), 500);
    run_parse_case("wide_10k", wide_10k, 50);
    run_query_case("query_assets_10k", assets_10k, 10000, 100, QueryMode::Common);
    run_query_case("query_first_10k", assets_10k, 10000, 500, QueryMode::First);
    run_query_case("query_last_10k", assets_10k, 10000, 500, QueryMode::Last);
    run_query_case("query_missing_10k", assets_10k, 10000, 500, QueryMode::Missing);
    run_query_case("query_string_view_10k", assets_10k, 10000, 500, QueryMode::StringView);
    run_query_case("query_many_keys_last", data::make_many_keys_data(5000, 24), 5000, 200, QueryMode::ManyLast);
    scan_probe::run_case("scan_probe_asset_database_5k", asset_database_5k, 1000);
#if GSEXP_HAVE_YYJSON
    run_yyjson_parse_case("yyjson_assets_10k", asset_json_10k, 50);
    run_yyjson_parse_case("yyjson_assets_50k", asset_json_50k, 10);
    run_yyjson_parse_case("yyjson_asset_database_5k", asset_database_json_5k, 50);
    run_yyjson_small_files_case("yyjson_small_files_1k", 1000, 500);
    run_yyjson_parse_case("yyjson_strings_plain_5k", data::make_string_json(5000, false, 12), 50);
    run_yyjson_parse_case("yyjson_strings_escaped_5k", data::make_string_json(5000, true, 12), 50);
    run_yyjson_parse_case("yyjson_wide_10k", wide_json_10k, 50);
    run_yyjson_query_case("yyjson_query_assets_10k", asset_json_10k, "assets", 10000, 100, false);
    run_yyjson_query_case("yyjson_query_many_keys_last", many_keys_json, "records", 5000, 200, true);
#endif
    return 0;
}
