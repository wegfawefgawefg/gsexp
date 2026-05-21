#include "gsexp/sexp.hpp"

#include "bench_data.hpp"
#include "scan_probe.hpp"
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
              << " child_index_lookup_capacity=" << stats.child_index_lookup_capacity
              << " child_index_lookup_bytes=" << stats.child_index_lookup_bytes
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

double run_child_iteration_once(gsexp::Node root, int iterations) {
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

void run_child_iteration_case(const char* name, const std::string& text, int items, int iterations) {
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
    std::string code_json_2k = data::make_code_json(2000);
    std::string wide_json_10k = data::make_wide_json(10000);
    std::string many_keys_json = data::make_many_keys_json(5000, 24);

    run_asset_case("assets_1k", 1000, 500);
    run_parse_case("assets_10k", assets_10k, 50);
    run_parse_owned_case("assets_10k_owned", assets_10k, 50);
    run_parse_case("assets_50k", assets_50k, 10);
    run_parse_owned_case("assets_50k_owned", assets_50k, 10);
    run_parse_case("asset_database_5k", asset_database_5k, 50);
    run_parse_owned_case("asset_database_5k_owned", asset_database_5k, 50);
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
    run_parse_case("code_forms_2k", data::make_code_data(2000), 50);
    run_parse_case("wide_10k", wide_10k, 50);
    run_query_case("query_assets_10k", assets_10k, 10000, 100, QueryMode::Common);
    run_query_case("query_first_10k", assets_10k, 10000, 500, QueryMode::First);
    run_query_case("query_last_10k", assets_10k, 10000, 500, QueryMode::Last);
    run_query_case("query_missing_10k", assets_10k, 10000, 500, QueryMode::Missing);
    run_query_case("query_string_view_10k", assets_10k, 10000, 500, QueryMode::StringView);
    run_query_case("query_text_only_10k", assets_10k, 10000, 200, QueryMode::TextOnly);
    run_query_case("query_symbol_compare_10k", assets_10k, 10000, 200, QueryMode::SymbolCompare);
    run_child_iteration_case("iterate_assets_10k", assets_10k, 10000, 200);
    std::string many_keys_data = data::make_many_keys_data(5000, 24);
    run_query_case("query_many_keys_last", many_keys_data, 5000, 200, QueryMode::ManyLast);
    run_query_case("query_find_many_keys_last", many_keys_data, 5000, 200, QueryMode::FindOnly);
    run_query_case("query_child_at_many_keys_last", many_keys_data, 5000, 200, QueryMode::ChildAtValue);
    scan_probe::run_case("scan_probe_asset_database_5k", asset_database_5k, 1000);
#if GSEXP_HAVE_YYJSON
    yyjson_bench::run_parse_case("yyjson_assets_10k", asset_json_10k, 50);
    yyjson_bench::run_parse_case("yyjson_assets_50k", asset_json_50k, 10);
    yyjson_bench::run_parse_case("yyjson_asset_database_5k", asset_database_json_5k, 50);
    yyjson_bench::run_small_files_case("yyjson_small_files_1k", 1000, 500);
    yyjson_bench::run_parse_case("yyjson_strings_plain_5k", data::make_string_json(5000, false, 12), 50);
    yyjson_bench::run_parse_case("yyjson_strings_escaped_5k", data::make_string_json(5000, true, 12), 50);
    yyjson_bench::run_parse_case("yyjson_code_forms_2k", code_json_2k, 50);
    yyjson_bench::run_parse_case("yyjson_wide_10k", wide_json_10k, 50);
    yyjson_bench::run_query_case("yyjson_query_assets_10k", asset_json_10k, "assets", 10000, 100, false);
    yyjson_bench::run_query_case("yyjson_query_many_keys_last", many_keys_json, "records", 5000, 200, true);
#endif
    return 0;
}
