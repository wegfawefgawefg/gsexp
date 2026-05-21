#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace bench_data {

inline std::string make_asset_data(int items) {
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

inline std::string make_asset_json(int items) {
    std::ostringstream out;
    out << "{\"assets\":[\n";
    for (int i = 0; i < items; ++i) {
        if (i > 0)
            out << ",\n";
        out << "  {"
            << "\"id\":" << i << ','
            << "\"path\":\"textures/item_" << i << ".png\","
            << "\"type\":\"sprite\","
            << "\"x\":" << (i % 100) / 100.0 << ','
            << "\"y\":0.25,"
            << "\"w\":64,"
            << "\"h\":64,"
            << "\"tags\":[\"ui\",\"item\",\"common\"]"
            << '}';
    }
    out << "\n]}\n";
    return out.str();
}

inline std::string make_small_config(int index) {
    std::ostringstream out;
    out << "(settings"
        << " (id " << index << ")"
        << " (name \"config_" << index << "\")"
        << " (enabled true)"
        << " (volume 0.75)"
        << " (window (w 1280) (h 720)))";
    return out.str();
}

inline std::string make_small_config_json(int index) {
    std::ostringstream out;
    out << "{\"settings\":{"
        << "\"id\":" << index << ','
        << "\"name\":\"config_" << index << "\","
        << "\"enabled\":true,"
        << "\"volume\":0.75,"
        << "\"window\":{\"w\":1280,\"h\":720}"
        << "}}";
    return out.str();
}

inline std::vector<std::string> make_small_configs(int files) {
    std::vector<std::string> configs;
    configs.reserve(static_cast<std::size_t>(files));
    for (int i = 0; i < files; ++i)
        configs.push_back(make_small_config(i));
    return configs;
}

inline std::vector<std::string> make_small_config_jsons(int files) {
    std::vector<std::string> configs;
    configs.reserve(static_cast<std::size_t>(files));
    for (int i = 0; i < files; ++i)
        configs.push_back(make_small_config_json(i));
    return configs;
}

inline std::string make_string_data(int items, bool escaped, int tail_words) {
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

inline std::string make_string_json(int items, bool escaped, int tail_words) {
    std::ostringstream out;
    out << "{\"strings\":[\n";
    for (int i = 0; i < items; ++i) {
        if (i > 0)
            out << ",\n";
        out << "  {"
            << "\"id\":" << i << ','
            << "\"path\":\"assets/dialog/scene_" << i << "/line_" << i << ".txt\","
            << "\"text\":\"";
        for (int word = 0; word < tail_words; ++word) {
            if (escaped && word % 5 == 0)
                out << "quoted\\\"word\\\" tab\\t ";
            else
                out << "plain_word_" << word << ' ';
        }
        out << "\"}";
    }
    out << "\n]}\n";
    return out.str();
}

inline std::string make_deep_data(int depth) {
    std::string out;
    out.reserve(static_cast<std::size_t>(depth) * 12);
    for (int i = 0; i < depth; ++i)
        out += "(node ";
    out += "leaf";
    for (int i = 0; i < depth; ++i)
        out += ')';
    return out;
}

inline std::string make_wide_data(int items) {
    std::ostringstream out;
    out << "(wide";
    for (int i = 0; i < items; ++i)
        out << " (item " << i << " value_" << i << ')';
    out << ')';
    return out.str();
}

inline std::string make_wide_json(int items) {
    std::ostringstream out;
    out << "{\"wide\":[";
    for (int i = 0; i < items; ++i) {
        if (i > 0)
            out << ',';
        out << "{\"item\":" << i << ",\"value\":\"value_" << i << "\"}";
    }
    out << "]}";
    return out.str();
}

inline std::string make_many_keys_data(int items, int keys_per_item) {
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

inline std::string make_many_keys_json(int items, int keys_per_item) {
    std::ostringstream out;
    out << "{\"records\":[\n";
    for (int item = 0; item < items; ++item) {
        if (item > 0)
            out << ",\n";
        out << "  {";
        for (int key = 0; key < keys_per_item; ++key) {
            if (key > 0)
                out << ',';
            out << "\"key_" << key << "\":" << (item + key);
        }
        out << '}';
    }
    out << "\n]}\n";
    return out.str();
}

} // namespace bench_data
