#include "gsexp/sexp.hpp"

#include "bench_data.hpp"
#include "internal_query_bench.hpp"
#include "query_bench.hpp"
#include "query_transition_bench.hpp"
#include "scan_probe.hpp"
#include "traversal_bench.hpp"
#include "yyjson_bench.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

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

std::string read_file_or_exit(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "failed to open benchmark file: " << path << "\n";
        std::exit(1);
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

double run_parse_owned_file_once(const char* name, const char* path, int iterations) {
    std::size_t parsed_roots = 0;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string text = read_file_or_exit(path);
        gsexp::ParseResult result = gsexp::parse_owned(std::move(text));
        if (!result.ok) {
            std::cerr << "parse failed in file benchmark case: " << name << "\n";
            std::exit(1);
        }
        parsed_roots += result.root_count();
    }
    auto end = std::chrono::steady_clock::now();
    if (parsed_roots == 0) {
        std::cerr << "file benchmark parsed no roots\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

double run_file_read_once(const char* name, const char* path, int iterations) {
    std::size_t bytes_read = 0;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string text = read_file_or_exit(path);
        bytes_read += text.size();
    }
    auto end = std::chrono::steady_clock::now();
    if (bytes_read == 0) {
        std::cerr << "file read benchmark read no bytes: " << name << "\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

void write_benchmark_file(const char* path, const std::string& text) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "failed to write benchmark file: " << path << "\n";
        std::exit(1);
    }
    file << text;
}

void run_file_read_case(const char* name, const char* path, const std::string& text, int iterations) {
    write_benchmark_file(path, text);

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_file_read_once(name, path, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    double mib = (static_cast<double>(text.size()) * iterations) / (1024.0 * 1024.0);
    std::cout << name << " bytes=" << text.size() << " iterations=" << iterations
              << " best_of=3 seconds=" << best_seconds
              << " mib_per_second=" << (mib / best_seconds) << "\n";
}

void run_parse_owned_file_case(const char* name,
                               const char* path,
                               const std::string& text,
                               int iterations) {
    write_benchmark_file(path, text);

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_parse_owned_file_once(name, path, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

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

} // namespace

using namespace query_bench;

int main() {
    print_benchmark_context();

    std::string assets_10k = data::make_asset_data(10000);
    std::string assets_50k = data::make_asset_data(50000);
    std::string asset_database_5k = data::make_asset_database_data(5000);
    std::string asset_database_20k = data::make_asset_database_data(20000);
    std::string wide_10k = data::make_wide_data(10000);
    std::string code_forms_2k = data::make_code_data(2000);
    std::string asset_json_10k = data::make_asset_json(10000);
    std::string asset_json_50k = data::make_asset_json(50000);
    std::string asset_database_json_5k = data::make_asset_database_json(5000);
    std::string asset_database_json_20k = data::make_asset_database_json(20000);
    std::string code_json_2k = data::make_code_json(2000);
    std::string wide_json_10k = data::make_wide_json(10000);
    std::string many_keys_json = data::make_many_keys_json(5000, 24);
    std::string nested_arg_data = data::make_nested_arg_data(5000);

    run_asset_case("assets_1k", 1000, 500);
    run_parse_case("assets_10k", assets_10k, 50);
    run_parse_owned_case("assets_10k_owned", assets_10k, 50);
    run_parse_case("assets_50k", assets_50k, 10);
    run_parse_owned_case("assets_50k_owned", assets_50k, 10);
    run_parse_case("asset_database_5k", asset_database_5k, 50);
    run_parse_owned_case("asset_database_5k_owned", asset_database_5k, 50);
    run_parse_case("asset_database_20k", asset_database_20k, 10);
    run_parse_owned_case("asset_database_20k_owned", asset_database_20k, 10);
    run_file_read_case("asset_database_5k_file_read",
                       "/tmp/gsexp_asset_database_5k.sexp",
                       asset_database_5k,
                       50);
    run_parse_owned_file_case("asset_database_5k_file_owned",
                              "/tmp/gsexp_asset_database_5k.sexp",
                              asset_database_5k,
                              50);
    run_small_files_case("small_files_1k", 1000, 500);
    run_parse_case("strings_plain_5k", data::make_string_data(5000, false, 12), 50);
    run_parse_case("strings_escaped_5k", data::make_string_data(5000, true, 12), 50);
    run_parse_case("deep_1k", data::make_deep_data(1000), 500);
    run_parse_case("code_forms_2k", code_forms_2k, 50);
    run_parse_case("wide_10k", wide_10k, 50);
    run_query_case("query_assets_10k", assets_10k, 10000, 100, QueryMode::Common);
    query_transition_bench::run_asset_common_transition_case("query_assets_10k_cold_once",
                                                             "query_assets_10k_warm_repeated",
                                                             assets_10k,
                                                             10000,
                                                             100);
    run_internal_asset_query_case("query_internal_assets_10k", assets_10k, 10000, 100);
    run_internal_ordered_asset_query_case("query_internal_ordered_assets_10k", assets_10k, 10000, 100);
    run_query_case("query_first_10k", assets_10k, 10000, 500, QueryMode::First);
    run_query_case("query_last_10k", assets_10k, 10000, 500, QueryMode::Last);
    run_query_case("query_missing_10k", assets_10k, 10000, 500, QueryMode::Missing);
    run_query_case("query_string_view_10k", assets_10k, 10000, 500, QueryMode::StringView);
    run_query_case("query_text_only_10k", assets_10k, 10000, 200, QueryMode::TextOnly);
    run_query_case("query_symbol_compare_10k", assets_10k, 10000, 200, QueryMode::SymbolCompare);
    run_asset_database_query_case("query_asset_database_5k", asset_database_5k, 5000, 200);
    run_internal_asset_database_query_case("query_internal_asset_database_5k",
                                           asset_database_5k,
                                           5000,
                                           200);
    run_asset_database_query_case("query_asset_database_20k", asset_database_20k, 20000, 50);
    run_internal_asset_database_query_case("query_internal_asset_database_20k",
                                           asset_database_20k,
                                           20000,
                                           50);
    run_child_iteration_case("iterate_assets_10k", assets_10k, 10000, 200);
    traversal_bench::run_internal_asset_children_case("iterate_internal_assets_10k",
                                                      assets_10k,
                                                      10000,
                                                      200);
    traversal_bench::run_child_span_asset_children_case("iterate_child_span_assets_10k",
                                                        assets_10k,
                                                        10000,
                                                        200);
    run_many_key_get_case("query_many_keys_8_last",
                          data::make_many_keys_data(5000, 8),
                          5000,
                          200,
                          "key_7",
                          7);
    std::string many_keys_16_data = data::make_many_keys_data(5000, 16);
    run_many_key_get_case("query_many_keys_16_last", many_keys_16_data, 5000, 200, "key_15", 15);
    query_transition_bench::run_many_key_transition_case("query_many_keys_16_cold_once",
                                                         "query_many_keys_16_warm_repeated",
                                                         many_keys_16_data,
                                                         5000,
                                                         200,
                                                         "key_15",
                                                         15);
    std::string many_keys_data = data::make_many_keys_data(5000, 24);
    run_many_key_get_case("query_many_keys_24_last", many_keys_data, 5000, 200, "key_23", 23);
    run_many_key_get_case("query_many_keys_48_last",
                          data::make_many_keys_data(5000, 48),
                          5000,
                          200,
                          "key_47",
                          47);
    run_query_case("query_many_keys_last", many_keys_data, 5000, 200, QueryMode::ManyLast);
    run_query_case("query_find_many_keys_last", many_keys_data, 5000, 200, QueryMode::FindOnly);
    run_query_case("query_child_at_many_keys_last", many_keys_data, 5000, 200, QueryMode::ChildAtValue);
    run_query_case("query_find_arg_many_keys_last", many_keys_data, 5000, 200, QueryMode::FindArgValue);
    run_nested_find_arg_case("query_nested_find_arg_5k", nested_arg_data, 5000, 200);
    run_internal_nested_find_arg_case("query_internal_nested_find_arg_5k", nested_arg_data, 5000, 200);
    traversal_bench::run_ordered_code_case("iterate_code_forms_2k", code_forms_2k, 200);
    traversal_bench::run_internal_ordered_code_case("iterate_internal_code_forms_2k", code_forms_2k, 200);
    traversal_bench::run_child_span_ordered_code_case("iterate_child_span_code_forms_2k",
                                                      code_forms_2k,
                                                      200);
    scan_probe::run_case("scan_probe_asset_database_5k", asset_database_5k, 1000);
#if GSEXP_HAVE_YYJSON
    yyjson_bench::run_parse_case("yyjson_assets_10k", asset_json_10k, 50);
    yyjson_bench::run_parse_case("yyjson_assets_50k", asset_json_50k, 10);
    yyjson_bench::run_parse_case("yyjson_asset_database_5k", asset_database_json_5k, 50);
    yyjson_bench::run_parse_case("yyjson_asset_database_20k", asset_database_json_20k, 10);
    yyjson_bench::run_small_files_case("yyjson_small_files_1k", 1000, 500);
    yyjson_bench::run_parse_case("yyjson_strings_plain_5k", data::make_string_json(5000, false, 12), 50);
    yyjson_bench::run_parse_case("yyjson_strings_escaped_5k", data::make_string_json(5000, true, 12), 50);
    yyjson_bench::run_parse_case("yyjson_code_forms_2k", code_json_2k, 50);
    yyjson_bench::run_parse_case("yyjson_wide_10k", wide_json_10k, 50);
    yyjson_bench::run_query_case("yyjson_query_assets_10k", asset_json_10k, "assets", 10000, 100, false);
    yyjson_bench::run_asset_database_query_case("yyjson_query_asset_database_5k",
                                                asset_database_json_5k,
                                                5000,
                                                200);
    yyjson_bench::run_asset_database_query_case("yyjson_query_asset_database_20k",
                                                asset_database_json_20k,
                                                20000,
                                                50);
    yyjson_bench::run_query_case("yyjson_query_many_keys_last", many_keys_json, "records", 5000, 200, true);
#endif
    return 0;
}
