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
    require(gsexp::extract_string(root, "name") == "demo", "extract string");
    require(gsexp::extract_int(root, "width") == 1280, "extract int");
    require(gsexp::extract_float(root, "scale").has_value(), "extract float");
}

void test_int_range() {
    gsexp::Value root;
    root.type = gsexp::ValueType::List;

    gsexp::Value key;
    key.type = gsexp::ValueType::Symbol;
    key.text = "value";

    gsexp::Value value;
    value.type = gsexp::ValueType::Float;
    value.float_value = static_cast<double>(std::numeric_limits<int>::max()) + 1024.0;

    gsexp::Value child;
    child.type = gsexp::ValueType::List;
    child.list.push_back(key);
    child.list.push_back(value);
    gsexp::Value root_symbol;
    root_symbol.type = gsexp::ValueType::Symbol;
    root_symbol.text = "root";

    root.list.push_back(root_symbol);
    root.list.push_back(child);

    require(!gsexp::extract_int(root, "value").has_value(), "reject out-of-range float int");
}

} // namespace

int main() {
    test_parse_and_extract();
    test_int_range();

    std::cout << "gsexp_tests passed\n";
    return 0;
}
