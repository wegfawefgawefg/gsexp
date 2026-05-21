#include "gsexp/sexp.hpp"

#include <utility>

namespace gsexp {
namespace {

bool is_space(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == '\f';
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

void advance_position(char c, int& line, int& column) {
    if (c == '\n') {
        ++line;
        column = 1;
        return;
    }

    ++column;
}

void add_diagnostic(std::vector<Diagnostic>* diagnostics,
                    DiagnosticSeverity severity,
                    std::string message,
                    int line,
                    int column) {
    if (!diagnostics)
        return;

    diagnostics->push_back(Diagnostic{severity, std::move(message), line, column});
}

class Parser {
  public:
    explicit Parser(std::string_view source) : storage(std::make_shared<ParseStorage>()) {
        storage->source = source;
        text = storage->source;
    }

    ParseResult parse() {
        ParseResult result;
        result.storage = storage;

        while (true) {
            skip_space_and_comments();
            if (index >= text.size())
                break;

            std::uint32_t value = invalid_node;
            if (!parse_value(invalid_node, value, result.diagnostics))
                return result;
            result.roots.push_back(value);
        }

        result.ok = true;
        return result;
    }

  private:
    std::shared_ptr<ParseStorage> storage;
    std::string_view text;
    std::size_t index = 0;
    int line = 1;
    int column = 1;

    std::uint32_t add_node(ValueType type, std::string_view node_text, std::uint32_t parent) {
        std::uint32_t node_index = static_cast<std::uint32_t>(storage->nodes.size());
        storage->nodes.push_back(NodeData{type, node_text, parent});

        if (parent != invalid_node) {
            NodeData& parent_node = storage->nodes[parent];
            if (parent_node.first_child == invalid_node) {
                parent_node.first_child = node_index;
            } else {
                storage->nodes[parent_node.last_child].next_sibling = node_index;
            }
            parent_node.last_child = node_index;
            ++parent_node.child_count;
        }

        return node_index;
    }

    void advance() {
        advance_position(text[index], line, column);
        ++index;
    }

    void add_error(std::vector<Diagnostic>& diagnostics,
                   std::string message,
                   int error_line,
                   int error_column) {
        diagnostics.push_back(
            Diagnostic{DiagnosticSeverity::Error, std::move(message), error_line, error_column});
    }

    void skip_space_and_comments() {
        while (index < text.size()) {
            char c = text[index];
            if (is_space(c)) {
                advance();
                continue;
            }

            if (c == ';' || c == '#') {
                while (index < text.size() && text[index] != '\n')
                    advance();
                continue;
            }

            return;
        }
    }

    bool parse_value(std::uint32_t parent,
                     std::uint32_t& out,
                     std::vector<Diagnostic>& diagnostics) {
        if (index >= text.size()) {
            add_error(diagnostics, "expected value", line, column);
            return false;
        }

        char c = text[index];
        if (c == '(')
            return parse_list(parent, out, diagnostics);
        if (c == ')') {
            add_error(diagnostics, "unexpected ')'", line, column);
            return false;
        }
        if (c == '"')
            return parse_string(parent, out, diagnostics);

        parse_atom(parent, out);
        return true;
    }

    bool parse_list(std::uint32_t parent,
                    std::uint32_t& out,
                    std::vector<Diagnostic>& diagnostics) {
        int start_line = line;
        int start_column = column;
        advance();

        out = add_node(ValueType::List, {}, parent);

        while (true) {
            skip_space_and_comments();
            if (index >= text.size()) {
                add_error(diagnostics, "missing closing ')'", start_line, start_column);
                return false;
            }
            if (text[index] == ')') {
                advance();
                return true;
            }

            std::uint32_t child = invalid_node;
            if (!parse_value(out, child, diagnostics))
                return false;
        }
    }

    bool parse_string(std::uint32_t parent,
                      std::uint32_t& out,
                      std::vector<Diagnostic>& diagnostics) {
        int start_line = line;
        int start_column = column;
        advance();

        std::size_t content_start = index;
        std::size_t scan = index;
        while (scan < text.size()) {
            char ch = text[scan];
            if (ch == '"') {
                out = add_node(ValueType::String, text.substr(content_start, scan - content_start), parent);
                column += static_cast<int>(scan - content_start) + 1;
                index = scan + 1;
                return true;
            }
            if (ch == '\\' || ch == '\n' || ch == '\r')
                break;
            ++scan;
        }

        std::string buffer;
        buffer.reserve(32);
        while (index < text.size()) {
            char ch = text[index];
            advance();

            if (ch == '\\' && index < text.size()) {
                char esc = text[index];
                advance();
                switch (esc) {
                    case 'n': buffer.push_back('\n'); break;
                    case 'r': buffer.push_back('\r'); break;
                    case 't': buffer.push_back('\t'); break;
                    case '\\': buffer.push_back('\\'); break;
                    case '"': buffer.push_back('"'); break;
                    default: buffer.push_back(esc); break;
                }
            } else if (ch == '"') {
                storage->decoded_strings.push_back(std::move(buffer));
                out = add_node(ValueType::String, storage->decoded_strings.back(), parent);
                return true;
            } else {
                buffer.push_back(ch);
            }
        }

        add_error(diagnostics, "unterminated string", start_line, start_column);
        return false;
    }

    void parse_atom(std::uint32_t parent, std::uint32_t& out) {
        std::size_t start = index;
        while (index < text.size()) {
            char ch = text[index];
            if (is_space(ch) || ch == '(' || ch == ')')
                break;
            ++index;
        }

        column += static_cast<int>(index - start);
        out = add_node(ValueType::Atom, text.substr(start, index - start), parent);
    }
};

} // namespace

bool looks_like_integer(std::string_view text) {
    if (text.empty())
        return false;

    std::size_t index = 0;
    if (text[index] == '+' || text[index] == '-') {
        if (text.size() == 1)
            return false;
        ++index;
    }

    bool digit = false;
    for (; index < text.size(); ++index) {
        if (!is_digit(text[index]))
            return false;
        digit = true;
    }

    return digit;
}

bool looks_like_float(std::string_view text) {
    if (text.empty())
        return false;

    bool dot = false;
    bool exp = false;
    bool digit = false;
    std::size_t index = 0;

    if (text[index] == '+' || text[index] == '-') {
        if (text.size() == 1)
            return false;
        ++index;
    }

    for (; index < text.size(); ++index) {
        char c = text[index];
        if (is_digit(c)) {
            digit = true;
            continue;
        }
        if (c == '.' && !dot && !exp) {
            dot = true;
            continue;
        }
        if ((c == 'e' || c == 'E') && !exp && digit) {
            exp = true;
            digit = false;
            if (index + 1 < text.size() && (text[index + 1] == '+' || text[index + 1] == '-'))
                ++index;
            continue;
        }
        return false;
    }

    return digit && (dot || exp);
}

std::vector<Token> tokenize(std::string_view text, std::vector<Diagnostic>* diagnostics) {
    std::vector<Token> tokens;
    std::size_t index = 0;
    int line = 1;
    int column = 1;

    while (index < text.size()) {
        char c = text[index];

        if (is_space(c)) {
            advance_position(c, line, column);
            ++index;
            continue;
        }

        if (c == ';' || c == '#') {
            while (index < text.size() && text[index] != '\n') {
                advance_position(text[index], line, column);
                ++index;
            }
            continue;
        }

        int token_line = line;
        int token_column = column;

        if (c == '(') {
            tokens.push_back(Token{TokenType::LParen, "(", token_line, token_column});
            advance_position(c, line, column);
            ++index;
            continue;
        }

        if (c == ')') {
            tokens.push_back(Token{TokenType::RParen, ")", token_line, token_column});
            advance_position(c, line, column);
            ++index;
            continue;
        }

        if (c == '"') {
            advance_position(c, line, column);
            ++index;

            std::string buffer;
            bool closed = false;
            while (index < text.size()) {
                char ch = text[index];
                advance_position(ch, line, column);
                ++index;

                if (ch == '\\' && index < text.size()) {
                    char esc = text[index];
                    advance_position(esc, line, column);
                    ++index;
                    switch (esc) {
                        case 'n': buffer.push_back('\n'); break;
                        case 'r': buffer.push_back('\r'); break;
                        case 't': buffer.push_back('\t'); break;
                        case '\\': buffer.push_back('\\'); break;
                        case '"': buffer.push_back('"'); break;
                        default: buffer.push_back(esc); break;
                    }
                } else if (ch == '"') {
                    closed = true;
                    break;
                } else {
                    buffer.push_back(ch);
                }
            }

            if (!closed) {
                add_diagnostic(diagnostics,
                               DiagnosticSeverity::Error,
                               "unterminated string",
                               token_line,
                               token_column);
                return tokens;
            }

            tokens.push_back(Token{TokenType::String, buffer, token_line, token_column});
            continue;
        }

        std::size_t start = index;
        while (index < text.size()) {
            char ch = text[index];
            if (is_space(ch) || ch == '(' || ch == ')')
                break;

            advance_position(ch, line, column);
            ++index;
        }

        tokens.push_back(Token{
            TokenType::Atom,
            std::string(text.substr(start, index - start)),
            token_line,
            token_column,
        });
    }

    return tokens;
}

ParseResult parse(std::string_view text) {
    Parser parser(text);
    return parser.parse();
}

std::string quote_string(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');

    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }

    out.push_back('"');
    return out;
}

} // namespace gsexp
