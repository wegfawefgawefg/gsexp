#include "gsexp/sexp.hpp"

#include <cctype>
#include <cstdlib>
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

bool parse_value(const std::vector<Token>& tokens,
                 std::size_t& index,
                 Value& out,
                 std::vector<Diagnostic>& diagnostics) {
    if (index >= tokens.size()) {
        diagnostics.push_back(
            Diagnostic{DiagnosticSeverity::Error, "expected value", 1, 1});
        return false;
    }

    const Token& token = tokens[index];
    switch (token.type) {
        case TokenType::LParen: {
            ++index;
            Value list;
            list.type = ValueType::List;

            while (index < tokens.size() && tokens[index].type != TokenType::RParen) {
                Value child;
                if (!parse_value(tokens, index, child, diagnostics))
                    return false;
                list.list.push_back(std::move(child));
            }

            if (index >= tokens.size()) {
                diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::Error,
                    "missing closing ')'",
                    token.line,
                    token.column,
                });
                return false;
            }

            ++index;
            out = std::move(list);
            return true;
        }
        case TokenType::RParen:
            diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::Error,
                "unexpected ')'",
                token.line,
                token.column,
            });
            return false;
        case TokenType::String:
            ++index;
            out = Value{};
            out.type = ValueType::String;
            out.text = token.text;
            return true;
        case TokenType::Atom: {
            ++index;
            Value atom;
            if (looks_like_integer(token.text)) {
                try {
                    atom.type = ValueType::Int;
                    atom.int_value = std::stoll(token.text);
                } catch (...) {
                    atom.type = ValueType::Symbol;
                    atom.text = token.text;
                }
            } else if (looks_like_float(token.text)) {
                try {
                    atom.type = ValueType::Float;
                    atom.float_value = std::stod(token.text);
                } catch (...) {
                    atom.type = ValueType::Symbol;
                    atom.text = token.text;
                }
            } else {
                atom.type = ValueType::Symbol;
                atom.text = token.text;
            }
            out = std::move(atom);
            return true;
        }
    }

    return false;
}

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
    ParseResult result;
    std::vector<Token> tokens = tokenize(text, &result.diagnostics);

    for (const Diagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.severity == DiagnosticSeverity::Error)
            return result;
    }

    std::size_t index = 0;
    while (index < tokens.size()) {
        Value value;
        if (!parse_value(tokens, index, value, result.diagnostics))
            return result;
        result.values.push_back(std::move(value));
    }

    result.ok = true;
    return result;
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
        return static_cast<int>(value.float_value);

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
