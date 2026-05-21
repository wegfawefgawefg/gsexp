#include "gsexp/sexp.hpp"

#include <iostream>
#include <limits>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void test_parse_and_extract() {
    gsexp::ParseResult result = gsexp::parse(R"(
(settings
  (name "demo")
  (width 1280)
  (scale 1.5))
)");

    require(result.ok, "parse simple settings");
    require(result.values.size() == 1, "parse one root");

    const gsexp::Value& root = result.values.front();
    const gsexp::Value* width_node = gsexp::find_child(root, "width");
    require(width_node != nullptr, "width node exists");
    require(width_node->list[1].type == gsexp::ValueType::Atom, "numeric atom stays atom");
    require(gsexp::extract_string(root, "name") == "demo", "extract string");
    require(gsexp::extract_int(root, "width") == 1280, "extract int");
    require(gsexp::extract_float(root, "scale").has_value(), "extract float");
}

void test_parse_result_owns_text() {
    gsexp::ParseResult result = gsexp::parse(std::string("(root (name \"demo\") (kind atom))"));
    require(result.ok, "parse temporary source string");

    gsexp::ParseResult copied = result;
    const gsexp::Value& copied_root = copied.values.front();
    require(gsexp::extract_string(copied_root, "name") == "demo", "copied result keeps string text");
    require(gsexp::extract_string(copied_root, "kind") == "atom", "copied result keeps atom text");
}

void test_escaped_string_storage() {
    gsexp::ParseResult result = gsexp::parse(R"((root (text "line\nquoted\"text")))");
    require(result.ok, "parse escaped string");

    const gsexp::Value& root = result.values.front();
    require(gsexp::extract_string(root, "text") == "line\nquoted\"text", "escaped string is decoded");
}

void test_int_range() {
    gsexp::Value root;
    root.type = gsexp::ValueType::List;

    gsexp::Value key;
    key.type = gsexp::ValueType::Atom;
    key.text = "value";

    std::string out_of_range = std::to_string(std::numeric_limits<int>::max()) + "000";
    gsexp::Value value;
    value.type = gsexp::ValueType::Atom;
    value.text = out_of_range;

    gsexp::Value child;
    child.type = gsexp::ValueType::List;
    child.list.push_back(key);
    child.list.push_back(value);
    gsexp::Value root_symbol;
    root_symbol.type = gsexp::ValueType::Atom;
    root_symbol.text = "root";

    root.list.push_back(root_symbol);
    root.list.push_back(child);

    require(!gsexp::extract_int(root, "value").has_value(), "reject out-of-range float int");
}

} // namespace

int main() {
    test_parse_and_extract();
    test_parse_result_owns_text();
    test_escaped_string_storage();
    test_int_range();

    std::cout << "gsexp_tests passed\n";
    return 0;
}
