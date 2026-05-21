#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

#if defined(__SSE2__) &&                                                                      \
    (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86))
#include <emmintrin.h>
#define GSEXP_SCAN_PROBE_SSE2 1
#else
#define GSEXP_SCAN_PROBE_SSE2 0
#endif

namespace scan_probe {

inline bool is_probe_delimiter(char c) {
    return c == '(' || c == ')' || c == '"' || c == ';' || c == '#' || c == ' ' || c == '\n' ||
           c == '\r' || c == '\t' || c == '\v' || c == '\f';
}

inline std::uint32_t popcount_u32(std::uint32_t value) {
    std::uint32_t count = 0;
    while (value != 0) {
        value &= value - 1;
        ++count;
    }
    return count;
}

inline std::size_t count_delimiters_scalar(std::string_view text) {
    std::size_t count = 0;
    for (char c : text) {
        if (is_probe_delimiter(c))
            ++count;
    }
    return count;
}

#if GSEXP_SCAN_PROBE_SSE2
inline __m128i match_byte(__m128i bytes, char c) {
    return _mm_cmpeq_epi8(bytes, _mm_set1_epi8(c));
}

inline std::size_t count_delimiters_sse2(std::string_view text) {
    const char* data = text.data();
    std::size_t index = 0;
    std::size_t count = 0;

    while (index + 16 <= text.size()) {
        __m128i bytes = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + index));
        __m128i matches = match_byte(bytes, '(');
        matches = _mm_or_si128(matches, match_byte(bytes, ')'));
        matches = _mm_or_si128(matches, match_byte(bytes, '"'));
        matches = _mm_or_si128(matches, match_byte(bytes, ';'));
        matches = _mm_or_si128(matches, match_byte(bytes, '#'));
        matches = _mm_or_si128(matches, match_byte(bytes, ' '));
        matches = _mm_or_si128(matches, match_byte(bytes, '\n'));
        matches = _mm_or_si128(matches, match_byte(bytes, '\r'));
        matches = _mm_or_si128(matches, match_byte(bytes, '\t'));
        matches = _mm_or_si128(matches, match_byte(bytes, '\v'));
        matches = _mm_or_si128(matches, match_byte(bytes, '\f'));

        std::uint32_t mask = static_cast<std::uint32_t>(_mm_movemask_epi8(matches));
        count += popcount_u32(mask);
        index += 16;
    }

    while (index < text.size()) {
        if (is_probe_delimiter(text[index]))
            ++count;
        ++index;
    }

    return count;
}
#endif

template <typename Counter>
double run_once(std::string_view text, int iterations, Counter counter, std::size_t& out_count) {
    std::size_t count = 0;
    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration)
        count += counter(text);
    auto end = std::chrono::steady_clock::now();

    if (count == 0) {
        std::cerr << "scan probe did no work\n";
        std::exit(1);
    }

    out_count = count;
    return std::chrono::duration<double>(end - start).count();
}

template <typename Counter>
double best_seconds(std::string_view text, int iterations, Counter counter, std::size_t& out_count) {
    double best = 0.0;
    for (int run = 0; run < 3; ++run) {
        std::size_t count = 0;
        double seconds = run_once(text, iterations, counter, count);
        if (best == 0.0 || seconds < best) {
            best = seconds;
            out_count = count;
        }
    }
    return best;
}

inline void run_case(const char* name, std::string_view text, int iterations) {
    std::size_t scalar_count = 0;
    double scalar_seconds = best_seconds(text, iterations, count_delimiters_scalar, scalar_count);
    double mib = (static_cast<double>(text.size()) * iterations) / (1024.0 * 1024.0);

    std::cout << name << " bytes=" << text.size() << " iterations=" << iterations
              << " scalar_mib_per_second=" << (mib / scalar_seconds);

#if GSEXP_SCAN_PROBE_SSE2
    std::size_t sse2_count = 0;
    double sse2_seconds = best_seconds(text, iterations, count_delimiters_sse2, sse2_count);
    if (sse2_count != scalar_count) {
        std::cerr << "scan probe count mismatch\n";
        std::exit(1);
    }

    std::cout << " sse2_mib_per_second=" << (mib / sse2_seconds)
              << " sse2_speedup=" << (scalar_seconds / sse2_seconds);
#else
    std::cout << " sse2_mib_per_second=unavailable sse2_speedup=unavailable";
#endif

    std::cout << " delimiters=" << scalar_count << "\n";
}

} // namespace scan_probe
