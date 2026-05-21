#include "gsexp/sexp.hpp"

#include <charconv>
#include <utility>

namespace gsexp {

std::size_t ParseResult::root_count() const {
    return roots.size();
}

Node ParseResult::root(std::size_t index) const {
    if (!storage || index >= roots.size())
        return {};
    return Node(storage.get(), roots[index]);
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
    return node->text;
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
    if (!list.is_list())
        return {};

    bool first = true;
    for (Node child : list.children()) {
        if (first) {
            first = false;
            continue;
        }
        if (!child.is_list() || child.child_count() == 0)
            continue;
        if (child.child_at(0).is_atom(symbol))
            return child;
    }

    return {};
}

std::optional<int> extract_int(Node list, std::string_view symbol) {
    Node node = find_child(list, symbol);
    if (!node.valid() || node.child_count() < 2)
        return std::nullopt;

    Node value = node.child_at(1);
    if (value.type() == ValueType::Atom && looks_like_integer(value.text())) {
        int parsed = 0;
        const char* begin = value.text().data();
        const char* end = begin + value.text().size();
        auto result = std::from_chars(begin, end, parsed);
        if (result.ec == std::errc{} && result.ptr == end)
            return parsed;
    }

    return std::nullopt;
}

std::optional<float> extract_float(Node list, std::string_view symbol) {
    Node node = find_child(list, symbol);
    if (!node.valid() || node.child_count() < 2)
        return std::nullopt;

    Node value = node.child_at(1);
    if (value.type() == ValueType::Atom &&
        (looks_like_float(value.text()) || looks_like_integer(value.text()))) {
        float parsed = 0.0f;
        const char* begin = value.text().data();
        const char* end = begin + value.text().size();
        auto result = std::from_chars(begin, end, parsed);
        if (result.ec == std::errc{} && result.ptr == end)
            return parsed;
    }

    return std::nullopt;
}

std::optional<std::string> extract_string(Node list, std::string_view symbol) {
    Node node = find_child(list, symbol);
    if (!node.valid() || node.child_count() < 2)
        return std::nullopt;

    Node value = node.child_at(1);
    if (value.type() == ValueType::String || value.type() == ValueType::Atom)
        return std::string(value.text());

    return std::nullopt;
}

} // namespace gsexp
