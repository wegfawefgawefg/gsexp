#include "gsexp/sexp.hpp"

#include <algorithm>
#include <utility>

namespace gsexp {
namespace {

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

} // namespace gsexp
