#include "gsexp/sexp.hpp"

#include <algorithm>
#include <charconv>

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
                                      std::string_view searched_head) {
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
        if (head.type == ValueType::Atom && node_text(storage, head) == searched_head)
            return child_index;

        child_index = child.next_sibling;
    }

    return invalid_node;
}

std::uint32_t find_child_index(const ParseStorage& storage,
                               std::uint32_t list_index,
                               const NodeData& list,
                               std::string_view searched_head) {
    if (list.child_count < indexed_child_threshold)
        return find_child_index_direct(storage, list, searched_head);

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
                                  searched_head,
                                  [](const KeyIndexEntry& item, std::string_view key) {
                                      return item.key < key;
                                  });
    if (entry != entries.end() && entry->key == searched_head)
        return entry->child;

    return invalid_node;
}

const NodeData* find_arg_data(const ParseStorage& storage,
                              std::uint32_t list_index,
                              const NodeData& list,
                              std::string_view searched_head,
                              std::size_t arg_index) {
    if (list.type != ValueType::List)
        return nullptr;

    std::uint32_t child_index = find_child_index(storage, list_index, list, searched_head);
    if (child_index == invalid_node || child_index >= storage.nodes.size())
        return nullptr;

    const NodeData& child = storage.nodes[child_index];
    if (child.type != ValueType::List || child.first_child == invalid_node)
        return nullptr;

    std::uint32_t value_index = storage.nodes[child.first_child].next_sibling;
    for (std::size_t offset = 0; offset < arg_index && value_index != invalid_node; ++offset) {
        if (value_index >= storage.nodes.size())
            return nullptr;
        value_index = storage.nodes[value_index].next_sibling;
    }

    if (value_index == invalid_node || value_index >= storage.nodes.size())
        return nullptr;
    return &storage.nodes[value_index];
}

std::optional<int> parse_int(std::string_view text) {
    if (!looks_like_integer(text))
        return std::nullopt;
    if (!text.empty() && text.front() == '+')
        text.remove_prefix(1);

    int parsed = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    auto result = std::from_chars(begin, end, parsed);
    if (result.ec == std::errc{} && result.ptr == end)
        return parsed;
    return std::nullopt;
}

std::optional<float> parse_float(std::string_view text) {
    if (!looks_like_float(text) && !looks_like_integer(text))
        return std::nullopt;
    if (!text.empty() && text.front() == '+')
        text.remove_prefix(1);

    float parsed = 0.0f;
    const char* begin = text.data();
    const char* end = begin + text.size();
    auto result = std::from_chars(begin, end, parsed);
    if (result.ec == std::errc{} && result.ptr == end)
        return parsed;
    return std::nullopt;
}

} // namespace

FormView::FormView(Node node) : form(node) {}

bool FormView::valid() const {
    return form.is_list();
}

Node FormView::node() const {
    return form;
}

Node FormView::head() const {
    return form.head();
}

Node FormView::arg(std::size_t index) const {
    return form.child_at(index + 1);
}

Node FormView::find(std::string_view searched_head) const {
    if (cached_form.valid() && cached_head == searched_head)
        return cached_form;

    const ParseStorage* storage = form.storage;
    const NodeData* form_data = form.data();
    if (!storage || !form_data || form_data->type != ValueType::List)
        return {};

    std::uint32_t child_index = find_child_index(*storage, form.index, *form_data, searched_head);
    cached_head = searched_head;
    cached_form = Node(storage, child_index);
    return cached_form;
}

Node FormView::find_arg(std::string_view searched_head, std::size_t index) const {
    return FormView(find(searched_head)).arg(index);
}

std::optional<int> FormView::get_int(std::string_view searched_head) const {
    const ParseStorage* storage = form.storage;
    const NodeData* form_data = form.data();
    if (!storage || !form_data)
        return std::nullopt;

    const NodeData* value = find_arg_data(*storage, form.index, *form_data, searched_head, 0);
    if (!value || value->type != ValueType::Atom)
        return std::nullopt;
    return parse_int(node_text(*storage, *value));
}

std::optional<float> FormView::get_float(std::string_view searched_head) const {
    const ParseStorage* storage = form.storage;
    const NodeData* form_data = form.data();
    if (!storage || !form_data)
        return std::nullopt;

    const NodeData* value = find_arg_data(*storage, form.index, *form_data, searched_head, 0);
    if (!value || value->type != ValueType::Atom)
        return std::nullopt;
    return parse_float(node_text(*storage, *value));
}

std::optional<std::string> FormView::get_string(std::string_view searched_head) const {
    const ParseStorage* storage = form.storage;
    const NodeData* form_data = form.data();
    if (!storage || !form_data)
        return std::nullopt;

    const NodeData* value = find_arg_data(*storage, form.index, *form_data, searched_head, 0);
    if (!value || (value->type != ValueType::Atom && value->type != ValueType::String))
        return std::nullopt;
    return std::string(node_text(*storage, *value));
}

std::optional<std::string_view> FormView::get_string_view(std::string_view searched_head) const {
    const ParseStorage* storage = form.storage;
    const NodeData* form_data = form.data();
    if (!storage || !form_data)
        return std::nullopt;

    const NodeData* value = find_arg_data(*storage, form.index, *form_data, searched_head, 0);
    if (!value || (value->type != ValueType::Atom && value->type != ValueType::String))
        return std::nullopt;
    return node_text(*storage, *value);
}

} // namespace gsexp
