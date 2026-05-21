#include "gsexp/sexp.hpp"

#include <algorithm>
#include <charconv>
#include <utility>

namespace gsexp {
namespace {

constexpr std::uint32_t indexed_child_threshold = 16;

std::string_view node_text(const ParseStorage& storage, const NodeData& node) {
    if (node.text_size == 0)
        return {};

    if (node.text_storage == TextStorage::Decoded) {
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

std::vector<KeyIndexEntry> build_child_index(const ParseStorage& storage, const NodeData& list) {
    std::vector<KeyIndexEntry> entries;
    if (list.child_count < indexed_child_threshold)
        return entries;

    entries.reserve(list.child_count - 1);

    bool first = true;
    std::uint32_t child_index = list.first_child;
    while (child_index != invalid_node && child_index < storage.nodes.size()) {
        const NodeData& child = storage.nodes[child_index];
        if (first) {
            first = false;
            child_index = child.next_sibling;
            continue;
        }
        if (child.type == ValueType::List && child.first_child != invalid_node) {
            const NodeData& head = storage.nodes[child.first_child];
            if (head.type == ValueType::Atom)
                entries.push_back(KeyIndexEntry{node_text(storage, head), child_index});
        }
        child_index = child.next_sibling;
    }

    std::stable_sort(entries.begin(), entries.end(), [](const KeyIndexEntry& a, const KeyIndexEntry& b) {
        return a.key < b.key;
    });
    return entries;
}

std::uint32_t find_child_index_direct(const ParseStorage& storage,
                                      const NodeData& list,
                                      std::string_view symbol) {
    bool first = true;
    std::uint32_t child_index = list.first_child;
    while (child_index != invalid_node && child_index < storage.nodes.size()) {
        const NodeData& child = storage.nodes[child_index];
        if (first) {
            first = false;
            child_index = child.next_sibling;
            continue;
        }

        if (child.type != ValueType::List || child.first_child == invalid_node) {
            child_index = child.next_sibling;
            continue;
        }

        const NodeData& head = storage.nodes[child.first_child];
        if (head.type == ValueType::Atom && node_text(storage, head) == symbol)
            return child_index;

        child_index = child.next_sibling;
    }

    return invalid_node;
}

std::uint32_t find_child_index(const ParseStorage& storage,
                               std::uint32_t list_index,
                               const NodeData& list,
                               std::string_view symbol) {
    if (list.child_count < indexed_child_threshold)
        return find_child_index_direct(storage, list, symbol);

    auto it = std::lower_bound(storage.child_indexes.begin(),
                               storage.child_indexes.end(),
                               list_index,
                               [](const ChildIndexCache& item, std::uint32_t key) {
                                   return item.list < key;
                               });
    if (it == storage.child_indexes.end() || it->list != list_index) {
        ChildIndexCache cache;
        cache.list = list_index;
        cache.entries = build_child_index(storage, list);
        it = storage.child_indexes.insert(it, std::move(cache));
    }

    const std::vector<KeyIndexEntry>& entries = it->entries;
    auto entry = std::lower_bound(entries.begin(),
                                  entries.end(),
                                  symbol,
                                  [](const KeyIndexEntry& item, std::string_view key) {
                                      return item.key < key;
                                  });
    if (entry != entries.end() && entry->key == symbol)
        return entry->child;

    return invalid_node;
}

const NodeData* find_value_child(const ParseStorage& storage,
                                 std::uint32_t list_index,
                                 const NodeData& list,
                                 std::string_view symbol) {
    if (list.type != ValueType::List)
        return nullptr;

    std::uint32_t pair_index = find_child_index(storage, list_index, list, symbol);
    if (pair_index == invalid_node || pair_index >= storage.nodes.size())
        return nullptr;

    const NodeData& pair = storage.nodes[pair_index];
    if (pair.type != ValueType::List || pair.first_child == invalid_node)
        return nullptr;

    std::uint32_t value_index = storage.nodes[pair.first_child].next_sibling;
    if (value_index == invalid_node || value_index >= storage.nodes.size())
        return nullptr;

    return &storage.nodes[value_index];
}

} // namespace

std::size_t ParseResult::root_count() const {
    return roots.size();
}

Node ParseResult::root(std::size_t index) const {
    if (!storage || index >= roots.size())
        return {};
    return Node(storage.get(), roots[index]);
}

StorageStats ParseResult::storage_stats() const {
    StorageStats stats;
    stats.root_count = roots.size();
    if (!storage)
        return stats;

    stats.source_bytes = storage->source.size();
    stats.node_data_bytes = sizeof(NodeData);
    stats.node_count = storage->nodes.size();
    stats.node_capacity = storage->nodes.capacity();
    stats.node_bytes = storage->nodes.capacity() * sizeof(NodeData);
    if (stats.source_bytes > 0)
        stats.node_bytes_per_source_byte =
            static_cast<double>(stats.node_bytes) / static_cast<double>(stats.source_bytes);
    stats.decoded_string_count = storage->decoded_string_count;
    stats.decoded_string_bytes = storage->decoded_text.size();

    stats.child_index_count = storage->child_indexes.size();
    stats.child_index_capacity = storage->child_indexes.capacity();
    for (const ChildIndexCache& item : storage->child_indexes) {
        stats.child_index_entry_count += item.entries.size();
        stats.child_index_entry_capacity += item.entries.capacity();
    }

    stats.approximate_bytes = storage->source.capacity() +
                              storage->nodes.capacity() * sizeof(NodeData) +
                              storage->decoded_text.capacity() +
                              stats.child_index_capacity * sizeof(ChildIndexCache) +
                              stats.child_index_entry_capacity * sizeof(KeyIndexEntry);
    return stats;
}

Node::Node(const ParseStorage* node_storage, std::uint32_t node_index)
    : storage(node_storage), index(node_index) {}

bool Node::valid() const {
    return data() != nullptr;
}

bool Node::empty() const {
    return !valid();
}

ValueType Node::type() const {
    const NodeData* node = data();
    if (!node)
        return ValueType::Atom;
    return node->type;
}

std::string_view Node::text() const {
    const NodeData* node = data();
    if (!node)
        return {};
    return node_text(*storage, *node);
}

std::size_t Node::child_count() const {
    const NodeData* node = data();
    if (!node)
        return 0;
    return node->child_count;
}

Node Node::first_child() const {
    const NodeData* node = data();
    if (!node)
        return {};
    return Node(storage, node->first_child);
}

Node Node::next_sibling() const {
    const NodeData* node = data();
    if (!node)
        return {};
    return Node(storage, node->next_sibling);
}

Node Node::child_at(std::size_t child_index) const {
    Node child = first_child();
    for (std::size_t offset = 0; offset < child_index && child.valid(); ++offset)
        child = child.next_sibling();
    return child;
}

Node Node::head() const {
    return first_child();
}

Node Node::second() const {
    Node first = first_child();
    if (!first.valid())
        return {};
    return first.next_sibling();
}

ChildRange Node::children() const {
    const NodeData* node = data();
    if (!node)
        return {};
    return ChildRange(storage, node->first_child);
}

bool Node::is_list() const {
    return valid() && type() == ValueType::List;
}

bool Node::is_atom(std::string_view atom) const {
    return valid() && type() == ValueType::Atom && text() == atom;
}

bool Node::is_string() const {
    return valid() && type() == ValueType::String;
}

const NodeData* Node::data() const {
    if (!storage || index == invalid_node || index >= storage->nodes.size())
        return nullptr;
    return &storage->nodes[index];
}

ChildIterator::ChildIterator(const ParseStorage* iterator_storage, std::uint32_t node_index)
    : storage(iterator_storage), index(node_index) {}

Node ChildIterator::operator*() const {
    return Node(storage, index);
}

ChildIterator& ChildIterator::operator++() {
    if (!storage || index == invalid_node || index >= storage->nodes.size()) {
        index = invalid_node;
        return *this;
    }

    index = storage->nodes[index].next_sibling;
    return *this;
}

bool ChildIterator::operator==(const ChildIterator& other) const {
    return index == other.index && storage == other.storage;
}

bool ChildIterator::operator!=(const ChildIterator& other) const {
    return !(*this == other);
}

ChildRange::ChildRange(const ParseStorage* range_storage, std::uint32_t first_index)
    : storage(range_storage), first(first_index) {}

ChildIterator ChildRange::begin() const {
    return ChildIterator(storage, first);
}

ChildIterator ChildRange::end() const {
    return ChildIterator(storage, invalid_node);
}

bool is_atom(Node node, std::string_view atom) {
    return node.is_atom(atom);
}

bool is_symbol(Node node, std::string_view symbol) {
    return is_atom(node, symbol);
}

Node find_child(Node list, std::string_view symbol) {
    const ParseStorage* storage = list.storage;
    const NodeData* list_data = list.data();
    if (!storage || !list_data || list_data->type != ValueType::List)
        return {};

    std::uint32_t child_index = find_child_index(*storage, list.index, *list_data, symbol);
    return Node(storage, child_index);
}

std::optional<int> extract_int(Node list, std::string_view symbol) {
    if (!list.storage)
        return std::nullopt;

    const NodeData* list_data = list.data();
    if (!list_data)
        return std::nullopt;

    const NodeData* value = find_value_child(*list.storage, list.index, *list_data, symbol);
    if (!value || value->type != ValueType::Atom)
        return std::nullopt;

    std::string_view text = node_text(*list.storage, *value);
    if (looks_like_integer(text)) {
        if (!text.empty() && text.front() == '+')
            text.remove_prefix(1);
        int parsed = 0;
        const char* begin = text.data();
        const char* end = begin + text.size();
        auto result = std::from_chars(begin, end, parsed);
        if (result.ec == std::errc{} && result.ptr == end)
            return parsed;
    }

    return std::nullopt;
}

std::optional<float> extract_float(Node list, std::string_view symbol) {
    if (!list.storage)
        return std::nullopt;

    const NodeData* list_data = list.data();
    if (!list_data)
        return std::nullopt;

    const NodeData* value = find_value_child(*list.storage, list.index, *list_data, symbol);
    if (!value || value->type != ValueType::Atom)
        return std::nullopt;

    std::string_view text = node_text(*list.storage, *value);
    if (looks_like_float(text) || looks_like_integer(text)) {
        if (!text.empty() && text.front() == '+')
            text.remove_prefix(1);
        float parsed = 0.0f;
        const char* begin = text.data();
        const char* end = begin + text.size();
        auto result = std::from_chars(begin, end, parsed);
        if (result.ec == std::errc{} && result.ptr == end)
            return parsed;
    }

    return std::nullopt;
}

std::optional<std::string> extract_string(Node list, std::string_view symbol) {
    if (!list.storage)
        return std::nullopt;

    const NodeData* list_data = list.data();
    if (!list_data)
        return std::nullopt;

    const NodeData* value = find_value_child(*list.storage, list.index, *list_data, symbol);
    if (value && (value->type == ValueType::String || value->type == ValueType::Atom))
        return std::string(node_text(*list.storage, *value));

    return std::nullopt;
}

std::optional<std::string_view> extract_string_view(Node list, std::string_view symbol) {
    if (!list.storage)
        return std::nullopt;

    const NodeData* list_data = list.data();
    if (!list_data)
        return std::nullopt;

    const NodeData* value = find_value_child(*list.storage, list.index, *list_data, symbol);
    if (value && (value->type == ValueType::String || value->type == ValueType::Atom))
        return node_text(*list.storage, *value);

    return std::nullopt;
}

} // namespace gsexp
