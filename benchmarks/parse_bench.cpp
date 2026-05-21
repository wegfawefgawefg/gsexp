#include "gsexp/sexp.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string make_asset_data(int items) {
    std::ostringstream out;
    out << "(assets\n";
    for (int i = 0; i < items; ++i) {
        out << "  (asset"
            << " (id " << i << ")"
            << " (path \"textures/item_" << i << ".png\")"
            << " (type sprite)"
            << " (x " << (i % 100) / 100.0 << ")"
            << " (y 0.25)"
            << " (w 64)"
            << " (h 64)"
            << " (tags (tag ui) (tag item) (tag common)))\n";
    }
    out << ")\n";
    return out.str();
}

std::string make_small_config(int index) {
    std::ostringstream out;
    out << "(settings"
        << " (id " << index << ")"
        << " (name \"config_" << index << "\")"
        << " (enabled true)"
        << " (volume 0.75)"
        << " (window (w 1280) (h 720)))";
    return out.str();
}

std::vector<std::string> make_small_configs(int files) {
    std::vector<std::string> configs;
    configs.reserve(static_cast<std::size_t>(files));
    for (int i = 0; i < files; ++i)
        configs.push_back(make_small_config(i));
    return configs;
}

std::string make_string_data(int items, bool escaped, int tail_words) {
    std::ostringstream out;
    out << "(strings\n";
    for (int i = 0; i < items; ++i) {
        out << "  (entry"
            << " (id " << i << ")"
            << " (path \"assets/dialog/scene_" << i << "/line_" << i << ".txt\")"
            << " (text \"";
        for (int word = 0; word < tail_words; ++word) {
            if (escaped && word % 5 == 0)
                out << "quoted\\\"word\\\" tab\\t ";
            else
                out << "plain_word_" << word << ' ';
        }
        out << "\"))\n";
    }
    out << ")\n";
    return out.str();
}

std::string make_deep_data(int depth) {
    std::string out;
    out.reserve(static_cast<std::size_t>(depth) * 12);
    for (int i = 0; i < depth; ++i)
        out += "(node ";
    out += "leaf";
    for (int i = 0; i < depth; ++i)
        out += ')';
    return out;
}

std::string make_wide_data(int items) {
    std::ostringstream out;
    out << "(wide";
    for (int i = 0; i < items; ++i)
        out << " (item " << i << " value_" << i << ')';
    out << ')';
    return out.str();
}

std::string make_many_keys_data(int items, int keys_per_item) {
    std::ostringstream out;
    out << "(records\n";
    for (int item = 0; item < items; ++item) {
        out << "  (record";
        for (int key = 0; key < keys_per_item; ++key)
            out << " (key_" << key << ' ' << (item + key) << ')';
        out << ")\n";
    }
    out << ")\n";
    return out.str();
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
    run_parse_case(name, make_asset_data(items), iterations);
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
    std::vector<std::string> configs = make_small_configs(files);
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

} // namespace

int main() {
    std::string assets_10k = make_asset_data(10000);
    std::string assets_50k = make_asset_data(50000);
    std::string wide_10k = make_wide_data(10000);

    run_asset_case("assets_1k", 1000, 500);
    run_parse_case("assets_10k", assets_10k, 50);
    run_parse_owned_case("assets_10k_owned", assets_10k, 50);
    run_parse_case("assets_50k", assets_50k, 10);
    run_parse_owned_case("assets_50k_owned", assets_50k, 10);
    run_small_files_case("small_files_1k", 1000, 500);
    run_parse_case("strings_plain_5k", make_string_data(5000, false, 12), 50);
    run_parse_case("strings_escaped_5k", make_string_data(5000, true, 12), 50);
    run_parse_case("deep_1k", make_deep_data(1000), 500);
    run_parse_case("wide_10k", wide_10k, 50);
    run_query_case("query_assets_10k", assets_10k, 10000, 100, QueryMode::Common);
    run_query_case("query_first_10k", assets_10k, 10000, 500, QueryMode::First);
    run_query_case("query_last_10k", assets_10k, 10000, 500, QueryMode::Last);
    run_query_case("query_missing_10k", assets_10k, 10000, 500, QueryMode::Missing);
    run_query_case("query_string_view_10k", assets_10k, 10000, 500, QueryMode::StringView);
    run_query_case("query_many_keys_last", make_many_keys_data(5000, 24), 5000, 200, QueryMode::ManyLast);
    return 0;
}
