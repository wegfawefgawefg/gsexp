#include "gsexp/sexp.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class FlatNodeType {
    List,
    Atom,
    String,
};

struct FlatNode {
    FlatNodeType type = FlatNodeType::Atom;
    std::string_view text;
    std::size_t parent = static_cast<std::size_t>(-1);
};

bool bench_space(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == '\f';
}

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

class FlatParser {
  public:
    explicit FlatParser(std::string_view source) : text(source) {}

    bool parse() {
        while (true) {
            skip_space_and_comments();
            if (index >= text.size())
                return true;
            if (!parse_value(no_parent))
                return false;
        }
    }

    std::size_t node_count() const {
        return nodes.size();
    }

  private:
    static constexpr std::size_t no_parent = static_cast<std::size_t>(-1);

    std::string_view text;
    std::size_t index = 0;
    std::vector<FlatNode> nodes;

    void skip_space_and_comments() {
        while (index < text.size()) {
            char c = text[index];
            if (bench_space(c)) {
                ++index;
                continue;
            }
            if (c == ';' || c == '#') {
                while (index < text.size() && text[index] != '\n')
                    ++index;
                continue;
            }
            return;
        }
    }

    bool parse_value(std::size_t parent) {
        if (index >= text.size())
            return false;

        char c = text[index];
        if (c == '(')
            return parse_list(parent);
        if (c == ')' )
            return false;
        if (c == '"')
            return parse_string(parent);

        parse_atom(parent);
        return true;
    }

    bool parse_list(std::size_t parent) {
        std::size_t list_index = nodes.size();
        nodes.push_back(FlatNode{FlatNodeType::List, {}, parent});
        ++index;

        while (true) {
            skip_space_and_comments();
            if (index >= text.size())
                return false;
            if (text[index] == ')') {
                ++index;
                return true;
            }
            if (!parse_value(list_index))
                return false;
        }
    }

    bool parse_string(std::size_t parent) {
        ++index;
        std::size_t start = index;
        bool escaped = false;
        while (index < text.size()) {
            char c = text[index];
            if (escaped) {
                escaped = false;
                ++index;
                continue;
            }
            if (c == '\\') {
                escaped = true;
                ++index;
                continue;
            }
            if (c == '"') {
                nodes.push_back(FlatNode{FlatNodeType::String, text.substr(start, index - start), parent});
                ++index;
                return true;
            }
            ++index;
        }
        return false;
    }

    void parse_atom(std::size_t parent) {
        std::size_t start = index;
        while (index < text.size()) {
            char c = text[index];
            if (bench_space(c) || c == '(' || c == ')')
                break;
            ++index;
        }
        nodes.push_back(FlatNode{FlatNodeType::Atom, text.substr(start, index - start), parent});
    }
};

double run_once(const char* name, const std::string& text, int iterations) {
    std::size_t parsed_roots = 0;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        gsexp::ParseResult result = gsexp::parse(text);
        if (!result.ok) {
            std::cerr << "parse failed in benchmark case: " << name << "\n";
            std::exit(1);
        }
        parsed_roots += result.values.size();
    }
    auto end = std::chrono::steady_clock::now();
    if (parsed_roots == 0) {
        std::cerr << "benchmark parsed no roots\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

double run_flat_once(const char* name, const std::string& text, int iterations) {
    std::size_t parsed_nodes = 0;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        FlatParser parser(text);
        if (!parser.parse()) {
            std::cerr << "flat parse failed in benchmark case: " << name << "\n";
            std::exit(1);
        }
        parsed_nodes += parser.node_count();
    }
    auto end = std::chrono::steady_clock::now();
    if (parsed_nodes == 0) {
        std::cerr << "flat benchmark parsed no nodes\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

double best_parse_seconds(const char* name, const std::string& text, int iterations) {
    double best_seconds = 0.0;

    for (int run = 0; run < 3; ++run) {
        double seconds = run_once(name, text, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    return best_seconds;
}

void run_parse_case(const char* name, const std::string& text, int iterations) {
    double best_seconds = best_parse_seconds(name, text, iterations);
    double mib = (static_cast<double>(text.size()) * iterations) / (1024.0 * 1024.0);
    std::cout << name << " bytes=" << text.size() << " iterations=" << iterations
              << " best_of=3 seconds=" << best_seconds
              << " mib_per_second=" << (mib / best_seconds) << "\n";
}

void run_flat_parse_case(const char* name, const std::string& text, int iterations) {
    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_flat_once(name, text, iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

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
            parsed_roots += result.values.size();
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

double run_query_once(const gsexp::Value& root, int iterations) {
    double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (std::size_t index = 1; index < root.list.size(); ++index) {
            const gsexp::Value& asset = root.list[index];
            std::optional<int> id = gsexp::extract_int(asset, "id");
            std::optional<float> x = gsexp::extract_float(asset, "x");
            std::optional<float> y = gsexp::extract_float(asset, "y");
            std::optional<std::string> path = gsexp::extract_string(asset, "path");
            if (!id || !x || !y || !path) {
                std::cerr << "query benchmark missing expected field\n";
                std::exit(1);
            }
            sink += static_cast<double>(*id) + static_cast<double>(*x) + static_cast<double>(*y) +
                    static_cast<double>(path->size());
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "query benchmark did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

void run_query_case(const char* name, int items, int iterations) {
    gsexp::ParseResult result = gsexp::parse(make_asset_data(items));
    if (!result.ok || result.values.empty()) {
        std::cerr << "parse failed before query benchmark: " << name << "\n";
        std::exit(1);
    }

    double best_seconds = 0.0;
    for (int run = 0; run < 3; ++run) {
        double seconds = run_query_once(result.values.front(), iterations);
        if (best_seconds == 0.0 || seconds < best_seconds)
            best_seconds = seconds;
    }

    std::size_t queries = static_cast<std::size_t>(items) * static_cast<std::size_t>(iterations) * 4;
    double queries_per_second = static_cast<double>(queries) / best_seconds;
    std::cout << name << " items=" << items << " queries=" << queries
              << " best_of=3 seconds=" << best_seconds
              << " queries_per_second=" << queries_per_second << "\n";
}

} // namespace

int main() {
    run_asset_case("assets_1k", 1000, 500);
    run_asset_case("assets_10k", 10000, 50);
    run_asset_case("assets_50k", 50000, 10);
    run_small_files_case("small_files_1k", 1000, 500);
    run_parse_case("strings_plain_5k", make_string_data(5000, false, 12), 50);
    run_parse_case("strings_escaped_5k", make_string_data(5000, true, 12), 50);
    run_parse_case("deep_1k", make_deep_data(1000), 500);
    run_parse_case("wide_10k", make_wide_data(10000), 50);
    run_query_case("query_assets_10k", 10000, 100);
    run_flat_parse_case("flat_assets_50k", make_asset_data(50000), 10);
    run_flat_parse_case("flat_wide_10k", make_wide_data(10000), 50);
    return 0;
}
