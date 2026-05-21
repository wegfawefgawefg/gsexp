#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gsexp {

enum class TokenType {
    LParen,
    RParen,
    Atom,
    String,
};

enum class ValueType {
    List,
    String,
    Atom,
};

enum class DiagnosticSeverity {
    Warning,
    Error,
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    int line = 1;
    int column = 1;
};

struct Token {
    TokenType type = TokenType::Atom;
    std::string text;
    int line = 1;
    int column = 1;
};

constexpr std::uint32_t invalid_node = 0xffffffffu;

struct NodeData {
    std::string_view text;
    std::uint32_t first_child = invalid_node;
    std::uint32_t next_sibling = invalid_node;
    std::uint32_t child_count = 0;
    ValueType type = ValueType::List;
};

struct KeyIndexEntry {
    std::string_view key;
    std::uint32_t child = invalid_node;
};

struct ParseStorage {
    std::string source;
    std::vector<char> decoded_text;
    std::size_t decoded_string_count = 0;
    std::vector<NodeData> nodes;
    mutable std::unique_ptr<std::unordered_map<std::uint32_t, std::vector<KeyIndexEntry>>>
        child_indexes;
};

struct StorageStats {
    std::size_t source_bytes = 0;
    std::size_t node_count = 0;
    std::size_t node_capacity = 0;
    std::size_t decoded_string_count = 0;
    std::size_t decoded_string_bytes = 0;
    std::size_t child_index_count = 0;
    std::size_t child_index_entry_count = 0;
    std::size_t child_index_entry_capacity = 0;
    std::size_t root_count = 0;
    std::size_t approximate_bytes = 0;
};

class Node;
class ChildIterator;
class ChildRange;

struct ParseResult {
    bool ok = false;
    std::shared_ptr<ParseStorage> storage;
    std::vector<std::uint32_t> roots;
    std::vector<Diagnostic> diagnostics;

    std::size_t root_count() const;
    Node root(std::size_t index) const;
    StorageStats storage_stats() const;
};

class Node {
  public:
    Node() = default;
    Node(const ParseStorage* storage, std::uint32_t index);

    bool valid() const;
    bool empty() const;
    ValueType type() const;
    std::string_view text() const;
    std::size_t child_count() const;
    Node first_child() const;
    Node next_sibling() const;
    Node child_at(std::size_t child_index) const;
    Node head() const;
    Node second() const;
    ChildRange children() const;
    bool is_list() const;
    bool is_atom(std::string_view atom) const;
    bool is_string() const;

  private:
    friend class ChildIterator;
    friend class ChildRange;
    friend Node find_child(Node list, std::string_view symbol);

    const NodeData* data() const;

    const ParseStorage* storage = nullptr;
    std::uint32_t index = invalid_node;
};

class ChildIterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Node;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = Node;

    ChildIterator() = default;
    ChildIterator(const ParseStorage* storage, std::uint32_t index);

    Node operator*() const;
    ChildIterator& operator++();
    bool operator==(const ChildIterator& other) const;
    bool operator!=(const ChildIterator& other) const;

  private:
    const ParseStorage* storage = nullptr;
    std::uint32_t index = invalid_node;
};

class ChildRange {
  public:
    ChildRange() = default;
    ChildRange(const ParseStorage* storage, std::uint32_t first);

    ChildIterator begin() const;
    ChildIterator end() const;

  private:
    const ParseStorage* storage = nullptr;
    std::uint32_t first = invalid_node;
};

bool looks_like_integer(std::string_view text);
bool looks_like_float(std::string_view text);

ParseResult parse(std::string_view text);
ParseResult parse_owned(std::string text);
std::vector<Token> tokenize(std::string_view text, std::vector<Diagnostic>* diagnostics = nullptr);

bool is_atom(Node node, std::string_view atom);
bool is_symbol(Node node, std::string_view symbol);
Node find_child(Node list, std::string_view symbol);

std::optional<int> extract_int(Node list, std::string_view symbol);
std::optional<float> extract_float(Node list, std::string_view symbol);
std::optional<std::string> extract_string(Node list, std::string_view symbol);
std::optional<std::string_view> extract_string_view(Node list, std::string_view symbol);

std::string quote_string(std::string_view text);

} // namespace gsexp
