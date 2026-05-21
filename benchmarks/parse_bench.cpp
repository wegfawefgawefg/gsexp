#include "gsexp/sexp.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

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

void run_case(const char* name, int items, int iterations) {
    std::string text = make_asset_data(items);
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

} // namespace

int main() {
    run_case("assets_1k", 1000, 500);
    run_case("assets_10k", 10000, 50);
    return 0;
}
