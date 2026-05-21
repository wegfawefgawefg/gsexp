#include "gsexp/sexp.hpp"

#include <iostream>
#include <limits>
#include <string>

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
    require(result.root_count() == 1, "parse one root");

    gsexp::Node root = result.root(0);
    gsexp::FormView settings(root);
    gsexp::Node width_node = settings.find("width");
    require(width_node.valid(), "width node exists");
    require(settings.find_arg("width", 0).type() == gsexp::ValueType::Atom, "numeric atom stays atom");
    require(settings.get_string("name") == "demo", "get string");
    require(settings.get_string_view("name") == "demo", "get string view");
    require(settings.get_int("width") == 1280, "get int");
    require(settings.get_float("scale").has_value(), "get float");

    int children = 0;
    for (gsexp::Node child : root.children()) {
        require(child.valid(), "child iteration returns valid node");
        ++children;
    }
    require(children == 4, "iterate root children");
}

void test_parse_result_owns_text() {
    gsexp::ParseResult result = gsexp::parse(std::string("(root (name \"demo\") (kind atom))"));
    require(result.ok, "parse temporary source string");

    gsexp::ParseResult copied = result;
    gsexp::Node copied_root = copied.root(0);
    gsexp::FormView copied_form(copied_root);
    require(copied_form.get_string("name") == "demo", "copied result keeps string text");
    require(copied_form.get_string("kind") == "atom", "copied result keeps atom text");
}

void test_parse_owned() {
    std::string text = "(root (name \"owned\"))";
    gsexp::ParseResult result = gsexp::parse_owned(std::move(text));
    require(result.ok, "parse owned source string");
    require(gsexp::FormView(result.root(0)).get_string("name") == "owned", "owned source keeps text");
}

void test_escaped_string_storage() {
    gsexp::ParseResult result = gsexp::parse(R"((root (text "line\nquoted\"text")))");
    require(result.ok, "parse escaped string");

    gsexp::Node root = result.root(0);
    require(gsexp::FormView(root).get_string("text") == "line\nquoted\"text",
            "escaped string is decoded");
}

void test_int_range() {
    std::string max_int = std::to_string(std::numeric_limits<int>::max());
    std::string min_int = std::to_string(std::numeric_limits<int>::min());
    std::string too_large = max_int + "0";
    std::string too_small = min_int + "0";
    gsexp::ParseResult result = gsexp::parse("(root"
                                             " (max " +
                                             max_int +
                                             ")"
                                             " (min " +
                                             min_int +
                                             ")"
                                             " (too_large " +
                                             too_large +
                                             ")"
                                             " (too_small " +
                                             too_small +
                                             "))");
    require(result.ok, "parse int range input");
    gsexp::FormView root(result.root(0));
    require(root.get_int("max") == std::numeric_limits<int>::max(), "accept max int");
    require(root.get_int("min") == std::numeric_limits<int>::min(), "accept min int");
    require(!root.get_int("too_large").has_value(), "reject too-large int");
    require(!root.get_int("too_small").has_value(), "reject too-small int");
}

void test_numeric_rejections() {
    gsexp::ParseResult result = gsexp::parse(R"(
(root
  (good_int +42)
  (bad_int 12abc)
  (sign_only +)
  (good_float +1.25)
  (bad_float 1.25abc)
  (bad_exp 1e)
  (bad_nan nan)
  (bad_inf inf))
)");

    require(result.ok, "parse numeric rejection input");
    gsexp::FormView root(result.root(0));
    require(root.get_int("good_int") == 42, "accept plus int");
    require(!root.get_int("bad_int").has_value(), "reject int suffix");
    require(!root.get_int("sign_only").has_value(), "reject sign-only int");
    require(root.get_float("good_float").has_value(), "accept plus float");
    require(!root.get_float("bad_float").has_value(), "reject float suffix");
    require(!root.get_float("bad_exp").has_value(), "reject incomplete exponent");
    require(!root.get_float("bad_nan").has_value(), "reject nan float");
    require(!root.get_float("bad_inf").has_value(), "reject inf float");
}

void test_failed_escaped_string_rollback() {
    gsexp::ParseResult result = gsexp::parse("(root (text \"line\\nunterminated)");
    require(!result.ok, "escaped unterminated string fails");
    require(result.root_count() == 0, "failed escaped string has no public roots");

    gsexp::StorageStats stats = result.storage_stats();
    require(stats.decoded_string_count == 0, "failed escaped string has no decoded string count");
    require(stats.decoded_string_bytes == 0, "failed escaped string rolls decoded bytes back");
}

void test_errors_and_roots() {
    gsexp::ParseResult missing = gsexp::parse("(root");
    require(!missing.ok, "missing paren fails");
    require(!missing.diagnostics.empty(), "missing paren diagnostic");
    require(missing.diagnostics[0].line == 1, "missing paren line");
    require(missing.diagnostics[0].column == 1, "missing paren column");

    gsexp::ParseResult unexpected = gsexp::parse("\n  )");
    require(!unexpected.ok, "unexpected paren fails");
    require(!unexpected.diagnostics.empty(), "unexpected paren diagnostic");
    require(unexpected.diagnostics[0].line == 2, "unexpected paren line");
    require(unexpected.diagnostics[0].column == 3, "unexpected paren column");

    gsexp::ParseResult string = gsexp::parse("(root\n  \"unterminated)");
    require(!string.ok, "unterminated string fails");
    require(!string.diagnostics.empty(), "unterminated string diagnostic");
    require(string.diagnostics[0].line == 2, "unterminated string line");
    require(string.diagnostics[0].column == 3, "unterminated string column");

    gsexp::ParseResult roots = gsexp::parse("(a 1) (b 2)");
    require(roots.ok, "parse multiple roots");
    require(roots.root_count() == 2, "multiple roots count");
    require(roots.root(0).child_at(0).is_atom("a"), "first root is a");
    require(roots.root(1).child_at(0).is_atom("b"), "second root is b");
}

void test_large_child_count_overflow() {
    constexpr int child_count = 70000;
    std::string text;
    text.reserve(static_cast<std::size_t>(child_count) * 2 + 8);
    text += "(root";
    for (int index = 0; index < child_count; ++index)
        text += " a";
    text += ")";

    gsexp::ParseResult result = gsexp::parse(text);
    require(result.ok, "parse large child count input");
    require(result.root(0).child_count() == static_cast<std::size_t>(child_count + 1),
            "large child count remains exact");
}

} // namespace

int main() {
    test_parse_and_extract();
    test_parse_result_owns_text();
    test_parse_owned();
    test_escaped_string_storage();
    test_int_range();
    test_numeric_rejections();
    test_failed_escaped_string_rollback();
    test_errors_and_roots();
    test_large_child_count_overflow();

    std::cout << "gsexp_tests passed\n";
    return 0;
}
