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

inline std::uint32_t next_valid_sibling(const gsexp::ParseStorage& storage, std::uint32_t node_index) {
    if (node_index == gsexp::invalid_node || node_index >= storage.nodes.size())
        return gsexp::invalid_node;
    return storage.nodes[node_index].next_sibling;
}

inline std::uint32_t required_value_child(const gsexp::ParseStorage& storage, std::uint32_t field_index) {
    if (field_index == gsexp::invalid_node || field_index >= storage.nodes.size()) {
        std::cerr << "internal ordered asset probe missing field\n";
        std::exit(1);
    }

    std::uint32_t value_index = value_child(storage, storage.nodes[field_index]);
    if (value_index == gsexp::invalid_node || value_index >= storage.nodes.size()) {
        std::cerr << "internal ordered asset probe missing value\n";
        std::exit(1);
    }
    return value_index;
}

inline std::uint32_t find_field(const gsexp::ParseStorage& storage,
                                const gsexp::NodeData& form,
                                std::string_view name) {
    bool first = true;
    std::uint32_t field_index = form.first_child;
    while (field_index != gsexp::invalid_node && field_index < storage.nodes.size()) {
        const gsexp::NodeData& field = storage.nodes[field_index];
        if (first) {
            first = false;
            field_index = field.next_sibling;
            continue;
        }

        if (field.type == gsexp::ValueType::List && field.first_child != gsexp::invalid_node &&
            field.first_child < storage.nodes.size() && text_equals(storage, storage.nodes[field.first_child], name))
            return field_index;

        field_index = field.next_sibling;
    }

    return gsexp::invalid_node;
}

inline std::uint32_t required_field(const gsexp::ParseStorage& storage,
                                    const gsexp::NodeData& form,
                                    std::string_view name) {
    std::uint32_t field_index = find_field(storage, form, name);
    if (field_index == gsexp::invalid_node || field_index >= storage.nodes.size()) {
        std::cerr << "internal asset database probe missing field\n";
        std::exit(1);
    }
    return field_index;
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

inline double run_asset_database_query_once(const gsexp::ParseResult& result, int iterations) {
    const gsexp::ParseStorage& storage = *result.storage;
    std::uint32_t root_index = result.roots[0];
    if (root_index >= storage.nodes.size()) {
        std::cerr << "internal asset database probe has invalid root\n";
        std::exit(1);
    }

    double sink = 0.0;
    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        bool root_head = true;
        std::uint32_t record_index = storage.nodes[root_index].first_child;
        while (record_index != gsexp::invalid_node && record_index < storage.nodes.size()) {
            const gsexp::NodeData& record = storage.nodes[record_index];
            if (root_head) {
                root_head = false;
                record_index = record.next_sibling;
                continue;
            }

            if (record.type != gsexp::ValueType::List || record.first_child == gsexp::invalid_node ||
                record.first_child >= storage.nodes.size()) {
                record_index = record.next_sibling;
                continue;
            }

            const gsexp::NodeData& kind = storage.nodes[record.first_child];
            bool texture = text_equals(storage, kind, "texture");
            bool sound = text_equals(storage, kind, "sound");
            bool prefab = text_equals(storage, kind, "prefab");
            if (!texture && !sound && !prefab) {
                record_index = record.next_sibling;
                continue;
            }

            std::uint32_t id_value = required_value_child(storage, required_field(storage, record, "id"));
            std::uint32_t path_value = required_value_child(storage, required_field(storage, record, "path"));
            sink += static_cast<double>(node_text(storage, kind).size()) +
                    static_cast<double>(node_text(storage, storage.nodes[id_value]).size()) +
                    static_cast<double>(node_text(storage, storage.nodes[path_value]).size());

            if (texture) {
                std::uint32_t size_field = required_field(storage, record, "size");
                const gsexp::NodeData& size = storage.nodes[size_field];
                std::uint32_t w_value = required_value_child(storage, required_field(storage, size, "w"));
                std::uint32_t h_value = required_value_child(storage, required_field(storage, size, "h"));
                sink += static_cast<double>(parse_required_int(node_text(storage, storage.nodes[w_value])) +
                                            parse_required_int(node_text(storage, storage.nodes[h_value])));
            } else if (sound) {
                std::uint32_t volume_value =
                    required_value_child(storage, required_field(storage, record, "volume"));
                std::uint32_t stream_value =
                    required_value_child(storage, required_field(storage, record, "stream"));
                sink += static_cast<double>(cached_required_float(storage, volume_value)) +
                        static_cast<double>(node_text(storage, storage.nodes[stream_value]).size());
            } else {
                std::uint32_t bounds_field = required_field(storage, record, "bounds");
                const gsexp::NodeData& bounds = storage.nodes[bounds_field];
                std::uint32_t x_value = required_value_child(storage, required_field(storage, bounds, "x"));
                std::uint32_t y_value = required_value_child(storage, required_field(storage, bounds, "y"));
                sink += static_cast<double>(parse_required_int(node_text(storage, storage.nodes[x_value])) +
                                            parse_required_int(node_text(storage, storage.nodes[y_value])));
            }

            record_index = record.next_sibling;
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "internal asset database probe did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline double run_ordered_asset_query_once(const gsexp::ParseResult& result, int iterations) {
    const gsexp::ParseStorage& storage = *result.storage;
    std::uint32_t root_index = result.roots[0];
    if (root_index >= storage.nodes.size()) {
        std::cerr << "internal ordered asset probe has invalid root\n";
        std::exit(1);
    }

    double sink = 0.0;
    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        std::uint32_t asset_index = next_valid_sibling(storage, storage.nodes[root_index].first_child);
        while (asset_index != gsexp::invalid_node && asset_index < storage.nodes.size()) {
            const gsexp::NodeData& asset = storage.nodes[asset_index];
            std::uint32_t id_field = next_valid_sibling(storage, asset.first_child);
            std::uint32_t path_field = next_valid_sibling(storage, id_field);
            std::uint32_t type_field = next_valid_sibling(storage, path_field);
            std::uint32_t x_field = next_valid_sibling(storage, type_field);
            std::uint32_t y_field = next_valid_sibling(storage, x_field);

            std::uint32_t id_value = required_value_child(storage, id_field);
            std::uint32_t path_value = required_value_child(storage, path_field);
            std::uint32_t x_value = required_value_child(storage, x_field);
            std::uint32_t y_value = required_value_child(storage, y_field);

            int id = parse_required_int(node_text(storage, storage.nodes[id_value]));
            std::string_view path = node_text(storage, storage.nodes[path_value]);
            float x = cached_required_float(storage, x_value);
            float y = cached_required_float(storage, y_value);

            sink += static_cast<double>(id) + static_cast<double>(x) + static_cast<double>(y) +
                    static_cast<double>(path.size());
            asset_index = asset.next_sibling;
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "internal ordered asset probe did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

inline std::uint32_t nth_arg(const gsexp::ParseStorage& storage,
                             const gsexp::NodeData& field,
                             std::size_t arg_index) {
    std::uint32_t value_index = value_child(storage, field);
    for (std::size_t offset = 0; offset < arg_index && value_index != gsexp::invalid_node; ++offset) {
        if (value_index >= storage.nodes.size())
            return gsexp::invalid_node;
        value_index = storage.nodes[value_index].next_sibling;
    }
    return value_index;
}

inline std::uint32_t required_nth_arg(const gsexp::ParseStorage& storage,
                                      std::uint32_t field_index,
                                      std::size_t arg_index) {
    if (field_index == gsexp::invalid_node || field_index >= storage.nodes.size()) {
        std::cerr << "internal nested find_arg probe missing field\n";
        std::exit(1);
    }

    std::uint32_t value_index = nth_arg(storage, storage.nodes[field_index], arg_index);
    if (value_index == gsexp::invalid_node || value_index >= storage.nodes.size()) {
        std::cerr << "internal nested find_arg probe missing arg\n";
        std::exit(1);
    }
    return value_index;
}

inline double run_nested_find_arg_once(const gsexp::ParseResult& result, int iterations) {
    const gsexp::ParseStorage& storage = *result.storage;
    std::uint32_t root_index = result.roots[0];
    if (root_index >= storage.nodes.size()) {
        std::cerr << "internal nested find_arg probe has invalid root\n";
        std::exit(1);
    }

    double sink = 0.0;
    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        std::uint32_t layout_index = next_valid_sibling(storage, storage.nodes[root_index].first_child);
        while (layout_index != gsexp::invalid_node && layout_index < storage.nodes.size()) {
            const gsexp::NodeData& layout = storage.nodes[layout_index];
            std::uint32_t id_field = next_valid_sibling(storage, layout.first_child);
            std::uint32_t title_field = next_valid_sibling(storage, id_field);
            std::uint32_t play_field = next_valid_sibling(storage, title_field);
            std::uint32_t settings_field = next_valid_sibling(storage, play_field);
            std::uint32_t credits_field = next_valid_sibling(storage, settings_field);

            std::uint32_t id_value = required_value_child(storage, id_field);
            std::uint32_t title_value = required_nth_arg(storage, title_field, 0);
            std::uint32_t play_x = required_nth_arg(storage, play_field, 1);
            std::uint32_t settings_w = required_nth_arg(storage, settings_field, 3);
            std::uint32_t credits_h = required_nth_arg(storage, credits_field, 4);

            int id = parse_required_int(node_text(storage, storage.nodes[id_value]));
            std::string_view title = node_text(storage, storage.nodes[title_value]);
            std::string_view play_x_text = node_text(storage, storage.nodes[play_x]);
            std::string_view settings_w_text = node_text(storage, storage.nodes[settings_w]);
            std::string_view credits_h_text = node_text(storage, storage.nodes[credits_h]);

            sink += static_cast<double>(id) + static_cast<double>(title.size()) +
                    static_cast<double>(play_x_text.size()) +
                    static_cast<double>(settings_w_text.size()) +
                    static_cast<double>(credits_h_text.size());
            layout_index = layout.next_sibling;
        }
    }
    auto end = std::chrono::steady_clock::now();
    if (sink == 0.0) {
        std::cerr << "internal nested find_arg probe did no work\n";
        std::exit(1);
    }
    return std::chrono::duration<double>(end - start).count();
}

} // namespace internal_probe
