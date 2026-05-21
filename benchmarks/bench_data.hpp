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

inline std::string make_code_data(int functions) {
    std::ostringstream out;
    out << "(module"
        << " (name gameplay_rules)"
        << " (imports (import math) (import input) (import world))\n";

    for (int i = 0; i < functions; ++i) {
        out << "  (fn update_entity_" << i
            << " (params (entity e) (float dt) (int frame))"
            << " (returns bool)"
            << " (block"
            << " (let speed (+ base_speed " << (i % 17) << "))"
            << " (let active (and (visible e) (> health 0)))"
            << " (if active"
            << " (block"
            << " (set pos.x (+ pos.x (* speed dt)))"
            << " (set pos.y (+ pos.y (* (sin frame) 0.25)))"
            << " (emit \"entity_updated\" e))"
            << " (block"
            << " (set sleep_timer (+ sleep_timer dt))))"
            << " (return active)))\n";
    }

    out << ")\n";
    return out.str();
}

inline std::string make_code_json(int functions) {
    std::ostringstream out;
    out << "{\"module\":{"
        << "\"name\":\"gameplay_rules\","
        << "\"imports\":[\"math\",\"input\",\"world\"],"
        << "\"functions\":[\n";

    for (int i = 0; i < functions; ++i) {
        if (i > 0)
            out << ",\n";
        out << "  {"
            << "\"name\":\"update_entity_" << i << "\","
            << "\"params\":[[\"entity\",\"e\"],[\"float\",\"dt\"],[\"int\",\"frame\"]],"
            << "\"returns\":\"bool\","
            << "\"body\":["
            << "[\"let\",\"speed\",[\"+\",\"base_speed\"," << (i % 17) << "]],"
            << "[\"let\",\"active\",[\"and\",[\"visible\",\"e\"],[\">\",\"health\",0]]],"
            << "[\"if\",\"active\","
            << "[\"block\","
            << "[\"set\",\"pos.x\",[\"+\",\"pos.x\",[\"*\",\"speed\",\"dt\"]]],"
            << "[\"set\",\"pos.y\",[\"+\",\"pos.y\",[\"*\",[\"sin\",\"frame\"],0.25]]],"
            << "[\"emit\",\"entity_updated\",\"e\"]],"
            << "[\"block\",[\"set\",\"sleep_timer\",[\"+\",\"sleep_timer\",\"dt\"]]]],"
            << "[\"return\",\"active\"]]"
            << "}";
    }

    out << "\n]}}\n";
    return out.str();
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

inline std::string make_nested_arg_data(int items) {
    std::ostringstream out;
    out << "(layouts\n";
    for (int i = 0; i < items; ++i) {
        out << "  (layout"
            << " (id " << i << ")"
            << " (title \"title_" << i << "\" 0.25 0.08 0.50 0.12)"
            << " (play \"play_" << i << "\" 0.30 0.32 0.40 0.10)"
            << " (settings \"settings_" << i << "\" 0.30 0.46 0.40 0.10)"
            << " (credits \"credits_" << i << "\" 0.30 0.60 0.40 0.10)"
            << ")\n";
    }
    out << ")\n";
    return out.str();
}

inline std::string make_asset_database_data(int items) {
    std::ostringstream out;
    out << "(asset_database\n"
        << "  (schema 1)\n"
        << "  (root \"assets\")\n"
        << "  # generated mixed asset records\n";

    for (int i = 0; i < items; ++i) {
        if (i % 64 == 0)
            out << "  ; chunk " << (i / 64) << "\n";

        if (i % 3 == 0) {
            out << "  (texture"
                << " (id tex_" << i << ")"
                << " (path \"textures/zone_" << (i % 17) << "/item_" << i << ".png\")"
                << " (size (w " << (32 + (i % 5) * 16) << ") (h " << (32 + (i % 7) * 16) << "))"
                << " (atlas ui)"
                << " (tags (tag zone_" << (i % 17) << ") (tag prop) (tag common))";
            if (i % 4 != 0)
                out << " (mips true)";
            if (i % 11 == 0)
                out << " (variant \"damaged\")";
            out << ")\n";
        } else if (i % 3 == 1) {
            out << "  (sound"
                << " (id snd_" << i << ")"
                << " (path \"audio/events/event_" << i << ".ogg\")"
                << " (stream " << (i % 5 == 0 ? "true" : "false") << ")"
                << " (volume " << (0.5 + static_cast<double>(i % 30) / 100.0) << ")"
                << " (groups (group sfx) (group zone_" << (i % 9) << "))";
            if (i % 8 == 0)
                out << " (subtitle \"line_" << i << "\")";
            out << ")\n";
        } else {
            out << "  (prefab"
                << " (id prefab_" << i << ")"
                << " (path \"prefabs/room_" << (i % 23) << "/entity_" << i << ".sexp\")"
                << " (bounds (x " << (i % 100) << ") (y " << (i % 80) << ")"
                << " (w " << (16 + (i % 6) * 8) << ") (h " << (24 + (i % 4) * 8) << "))"
                << " (components (component transform) (component render))";
            if (i % 6 == 0)
                out << " (components (component physics))";
            out << ")\n";
        }
    }

    out << ")\n";
    return out.str();
}

inline std::string make_asset_database_json(int items) {
    std::ostringstream out;
    out << "{\"asset_database\":{\"schema\":1,\"root\":\"assets\",\"assets\":[\n";

    for (int i = 0; i < items; ++i) {
        if (i > 0)
            out << ",\n";

        if (i % 3 == 0) {
            out << "  {\"kind\":\"texture\","
                << "\"id\":\"tex_" << i << "\","
                << "\"path\":\"textures/zone_" << (i % 17) << "/item_" << i << ".png\","
                << "\"size\":{\"w\":" << (32 + (i % 5) * 16) << ",\"h\":"
                << (32 + (i % 7) * 16) << "},"
                << "\"atlas\":\"ui\","
                << "\"tags\":[\"zone_" << (i % 17) << "\",\"prop\",\"common\"]";
            if (i % 4 != 0)
                out << ",\"mips\":true";
            if (i % 11 == 0)
                out << ",\"variant\":\"damaged\"";
            out << '}';
        } else if (i % 3 == 1) {
            out << "  {\"kind\":\"sound\","
                << "\"id\":\"snd_" << i << "\","
                << "\"path\":\"audio/events/event_" << i << ".ogg\","
                << "\"stream\":" << (i % 5 == 0 ? "true" : "false") << ','
                << "\"volume\":" << (0.5 + static_cast<double>(i % 30) / 100.0) << ','
                << "\"groups\":[\"sfx\",\"zone_" << (i % 9) << "\"]";
            if (i % 8 == 0)
                out << ",\"subtitle\":\"line_" << i << "\"";
            out << '}';
        } else {
            out << "  {\"kind\":\"prefab\","
                << "\"id\":\"prefab_" << i << "\","
                << "\"path\":\"prefabs/room_" << (i % 23) << "/entity_" << i << ".sexp\","
                << "\"bounds\":{\"x\":" << (i % 100) << ",\"y\":" << (i % 80)
                << ",\"w\":" << (16 + (i % 6) * 8)
                << ",\"h\":" << (24 + (i % 4) * 8) << "},"
                << "\"components\":[\"transform\",\"render\"";
            if (i % 6 == 0)
                out << ",\"physics\"";
            out << "]}";
        }
    }

    out << "\n]}}\n";
    return out.str();
}

} // namespace bench_data
