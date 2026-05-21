#pragma once

#include <optional>
#include <string>
#include <string_view>
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

struct Value {
    ValueType type = ValueType::List;
    std::string text;
    std::vector<Value> list;
};

struct ParseResult {
    bool ok = false;
    std::vector<Value> values;
    std::vector<Diagnostic> diagnostics;
};

bool looks_like_integer(std::string_view text);
bool looks_like_float(std::string_view text);

ParseResult parse(std::string_view text);
std::vector<Token> tokenize(std::string_view text, std::vector<Diagnostic>* diagnostics = nullptr);

bool is_atom(const Value& value, std::string_view atom);
bool is_symbol(const Value& value, std::string_view symbol);
const Value* find_child(const Value& list, std::string_view symbol);

std::optional<int> extract_int(const Value& list, std::string_view symbol);
std::optional<float> extract_float(const Value& list, std::string_view symbol);
std::optional<std::string> extract_string(const Value& list, std::string_view symbol);

std::string quote_string(std::string_view text);

} // namespace gsexp
