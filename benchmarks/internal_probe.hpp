#pragma once

#include "gsexp/sexp.hpp"

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string_view>

namespace internal_probe {

inline std::string_view node_text(const gsexp::ParseStorage& storage, const gsexp::NodeData& node) {
    if (node.text_size == 0)
        return {};

    if (node.text_storage == gsexp::TextStorage::Decoded) {
        if (node.text_offset > storage.decoded_text.size())
            return {};
        std::size_t available = storage.decoded_text.size() - node.text_offset;
        std::size_t size = node.text_size;
        if (size > available)
            size = available;
        return std::string_view(storage.decoded_text.data() + node.text_offset, size);
    }

    if (node.text_offset > storage.source.size())
        return {};
    std::size_t available = storage.source.size() - node.text_offset;
    std::size_t size = node.text_size;
    if (size > available)
        size = available;
    return std::string_view(storage.source.data() + node.text_offset, size);
}

inline bool text_equals(const gsexp::ParseStorage& storage,
                        const gsexp::NodeData& node,
                        std::string_view expected) {
    if (node.type != gsexp::ValueType::Atom || node.text_size != expected.size())
        return false;

    std::string_view text = node_text(storage, node);
    return text.size() == expected.size() &&
           (text.empty() || std::memcmp(text.data(), expected.data(), expected.size()) == 0);
}

inline int parse_required_int(std::string_view text) {
    int value = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    auto result = std::from_chars(begin, end, value);
    if (result.ec == std::errc{} && result.ptr == end)
        return value;

    std::cerr << "internal asset query probe failed to parse int\n";
    std::exit(1);
}

inline float parse_required_float(std::string_view text) {
    float value = 0.0f;
    const char* begin = text.data();
    const char* end = begin + text.size();
    auto result = std::from_chars(begin, end, value);
    if (result.ec == std::errc{} && result.ptr == end)
        return value;

    std::cerr << "internal asset query probe failed to parse float\n";
    std::exit(1);
}

inline float cached_required_float(const gsexp::ParseStorage& storage, std::uint32_t node_index) {
    if (node_index >= storage.nodes.size()) {
        std::cerr << "internal asset query probe has invalid float node\n";
        std::exit(1);
    }

    if (storage.float_cache.empty())
        storage.float_cache.resize(storage.nodes.size(), std::numeric_limits<float>::quiet_NaN());

    float cached = storage.float_cache[node_index];
    if (!std::isnan(cached)) {
        if (std::isinf(cached)) {
            std::cerr << "internal asset query probe found invalid cached float\n";
            std::exit(1);
        }
        return cached;
    }

    float parsed = parse_required_float(node_text(storage, storage.nodes[node_index]));
    storage.float_cache[node_index] = parsed;
    return parsed;
}

inline std::uint32_t value_child(const gsexp::ParseStorage& storage, const gsexp::NodeData& field) {
    if (field.type != gsexp::ValueType::List || field.first_child == gsexp::invalid_node ||
        field.first_child >= storage.nodes.size())
        return gsexp::invalid_node;

    return storage.nodes[field.first_child].next_sibling;
}

inline double run_asset_query_once(const gsexp::ParseResult& result, int iterations) {
    const gsexp::ParseStorage& storage = *result.storage;
    std::uint32_t root_index = result.roots[0];
    if (root_index >= storage.nodes.size()) {
        std::cerr << "internal asset query probe has invalid root\n";
        std::exit(1);
    }

    double sink = 0.0;
    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        bool root_head = true;
        std::uint32_t asset_index = storage.nodes[root_index].first_child;
        while (asset_index != gsexp::invalid_node && asset_index < storage.nodes.size()) {
            const gsexp::NodeData& asset = storage.nodes[asset_index];
            if (root_head) {
                root_head = false;
                asset_index = asset.next_sibling;
                continue;
            }

            bool found_id = false;
            bool found_x = false;
            bool found_y = false;
            bool found_path = false;
            int id = 0;
            float x = 0.0f;
            float y = 0.0f;
            std::string_view path;

            bool asset_head = true;
            std::uint32_t field_index = asset.first_child;
            while (field_index != gsexp::invalid_node && field_index < storage.nodes.size()) {
                const gsexp::NodeData& field = storage.nodes[field_index];
                if (asset_head) {
                    asset_head = false;
                    field_index = field.next_sibling;
                    continue;
                }

                if (field.first_child != gsexp::invalid_node && field.first_child < storage.nodes.size()) {
                    const gsexp::NodeData& head = storage.nodes[field.first_child];
                    std::uint32_t value_index = value_child(storage, field);
                    if (value_index != gsexp::invalid_node && value_index < storage.nodes.size()) {
                        const gsexp::NodeData& value = storage.nodes[value_index];
                        if (text_equals(storage, head, "id")) {
                            id = parse_required_int(node_text(storage, value));
                            found_id = true;
                        } else if (text_equals(storage, head, "x")) {
                            x = cached_required_float(storage, value_index);
                            found_x = true;
                        } else if (text_equals(storage, head, "y")) {
                            y = cached_required_float(storage, value_index);
                            found_y = true;
                        } else if (text_equals(storage, head, "path")) {
                            path = node_text(storage, value);
                            found_path = true;
                        }
                    }
                }

                field_index = field.next_sibling;
            }

            if (!found_id || !found_x || !found_y || !found_path) {
                std::cerr << "internal asset query probe missing expected field\n";
                std::exit(1);
            }
            sink += static_cast<double>(id) + static_cast<double>(x) + static_cast<double>(y) +
                    static_cast<double>(path.size());
            asset_index = asset.next_sibling;
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "internal asset query probe did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

} // namespace internal_probe
