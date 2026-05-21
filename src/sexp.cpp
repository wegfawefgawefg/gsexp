#include "gsexp/sexp.hpp"

#include <cctype>
#include <cmath>
#include <limits>

namespace gsexp {
namespace {

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

std::optional<int> float_to_int(double value) {
    if (!std::isfinite(value))
        return std::nullopt;
    if (value < static_cast<double>(std::numeric_limits<int>::min()) ||
        value > static_cast<double>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

Value atom_value(std::string text) {
    Value atom;
    if (looks_like_integer(text)) {
        try {
            atom.type = ValueType::Int;
            atom.int_value = std::stoll(text);
        } catch (...) {
            atom.type = ValueType::Symbol;
            atom.text = std::move(text);
        }
    } else if (looks_like_float(text)) {
        try {
            atom.type = ValueType::Float;
            atom.float_value = std::stod(text);
        } catch (...) {
            atom.type = ValueType::Symbol;
            atom.text = std::move(text);
        }
    } else {
        atom.type = ValueType::Symbol;
        atom.text = std::move(text);
    }
    return atom;
}

class Parser {
  public:
    explicit Parser(std::string_view source) : text(source) {}

    ParseResult parse() {
        ParseResult result;

        while (true) {
            skip_space_and_comments();
            if (index >= text.size())
                break;

            Value value;
            if (!parse_value(value, result.diagnostics))
                return result;
            result.values.push_back(std::move(value));
        }

        result.ok = true;
        return result;
    }

  private:
    std::string_view text;
    std::size_t index = 0;
    int line = 1;
    int column = 1;

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
            if (std::isspace(static_cast<unsigned char>(c))) {
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

    bool parse_value(Value& out, std::vector<Diagnostic>& diagnostics) {
        if (index >= text.size()) {
            add_error(diagnostics, "expected value", line, column);
            return false;
        }

        char c = text[index];
        if (c == '(')
            return parse_list(out, diagnostics);
        if (c == ')') {
            add_error(diagnostics, "unexpected ')'", line, column);
            return false;
        }
        if (c == '"')
            return parse_string(out, diagnostics);

        parse_atom(out);
        return true;
    }

    bool parse_list(Value& out, std::vector<Diagnostic>& diagnostics) {
        int start_line = line;
        int start_column = column;
        advance();

        Value list;
        list.type = ValueType::List;
        list.list.reserve(2);

        while (true) {
            skip_space_and_comments();
            if (index >= text.size()) {
                add_error(diagnostics, "missing closing ')'", start_line, start_column);
                return false;
            }
            if (text[index] == ')') {
                advance();
                out = std::move(list);
                return true;
            }

            Value child;
            if (!parse_value(child, diagnostics))
                return false;
            list.list.push_back(std::move(child));
        }
    }

    bool parse_string(Value& out, std::vector<Diagnostic>& diagnostics) {
        int start_line = line;
        int start_column = column;
        advance();

        std::string buffer;
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
                out = Value{};
                out.type = ValueType::String;
                out.text = std::move(buffer);
                return true;
            } else {
                buffer.push_back(ch);
            }
        }

        add_error(diagnostics, "unterminated string", start_line, start_column);
        return false;
    }

    void parse_atom(Value& out) {
        std::size_t start = index;
        while (index < text.size()) {
            char ch = text[index];
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '(' || ch == ')')
                break;
            advance();
        }

        out = atom_value(std::string(text.substr(start, index - start)));
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
        if (!std::isdigit(static_cast<unsigned char>(text[index])))
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
        if (std::isdigit(static_cast<unsigned char>(c))) {
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

        if (std::isspace(static_cast<unsigned char>(c))) {
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
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '(' || ch == ')')
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

bool is_symbol(const Value& value, std::string_view symbol) {
    return value.type == ValueType::Symbol && value.text == symbol;
}

const Value* find_child(const Value& list, std::string_view symbol) {
    if (list.type != ValueType::List)
        return nullptr;

    for (std::size_t index = 1; index < list.list.size(); ++index) {
        const Value& child = list.list[index];
        if (child.type != ValueType::List || child.list.empty())
            continue;
        if (is_symbol(child.list.front(), symbol))
            return &child;
    }

    return nullptr;
}

std::optional<int> extract_int(const Value& list, std::string_view symbol) {
    const Value* node = find_child(list, symbol);
    if (!node || node->list.size() < 2)
        return std::nullopt;

    const Value& value = node->list[1];
    if (value.type == ValueType::Int) {
        if (value.int_value < std::numeric_limits<int>::min() ||
            value.int_value > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
        return static_cast<int>(value.int_value);
    }

    if (value.type == ValueType::Float)
        return float_to_int(value.float_value);

    if (value.type == ValueType::Symbol && looks_like_integer(value.text)) {
        try {
            return std::stoi(value.text);
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<float> extract_float(const Value& list, std::string_view symbol) {
    const Value* node = find_child(list, symbol);
    if (!node || node->list.size() < 2)
        return std::nullopt;

    const Value& value = node->list[1];
    if (value.type == ValueType::Float)
        return static_cast<float>(value.float_value);

    if (value.type == ValueType::Int)
        return static_cast<float>(value.int_value);

    if (value.type == ValueType::Symbol &&
        (looks_like_float(value.text) || looks_like_integer(value.text))) {
        try {
            return std::stof(value.text);
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<std::string> extract_string(const Value& list, std::string_view symbol) {
    const Value* node = find_child(list, symbol);
    if (!node || node->list.size() < 2)
        return std::nullopt;

    const Value& value = node->list[1];
    if (value.type == ValueType::String || value.type == ValueType::Symbol)
        return value.text;

    return std::nullopt;
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
