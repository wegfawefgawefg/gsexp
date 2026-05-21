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
    require(result.root_count() == 1, "parse one root");

    gsexp::Node root = result.root(0);
    gsexp::Node width_node = gsexp::find_child(root, "width");
    require(width_node.valid(), "width node exists");
    require(width_node.second().type() == gsexp::ValueType::Atom, "numeric atom stays atom");
    require(gsexp::extract_string(root, "name") == "demo", "extract string");
    require(gsexp::extract_string_view(root, "name") == "demo", "extract string view");
    require(gsexp::extract_int(root, "width") == 1280, "extract int");
    require(gsexp::extract_float(root, "scale").has_value(), "extract float");

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
    require(gsexp::extract_string(copied_root, "name") == "demo", "copied result keeps string text");
    require(gsexp::extract_string(copied_root, "kind") == "atom", "copied result keeps atom text");
}

void test_parse_owned() {
    std::string text = "(root (name \"owned\"))";
    gsexp::ParseResult result = gsexp::parse_owned(std::move(text));
    require(result.ok, "parse owned source string");
    require(gsexp::extract_string(result.root(0), "name") == "owned", "owned source keeps text");
}

void test_escaped_string_storage() {
    gsexp::ParseResult result = gsexp::parse(R"((root (text "line\nquoted\"text")))");
    require(result.ok, "parse escaped string");

    gsexp::Node root = result.root(0);
    require(gsexp::extract_string(root, "text") == "line\nquoted\"text", "escaped string is decoded");
}

void test_int_range() {
    std::string out_of_range = std::to_string(std::numeric_limits<int>::max()) + "000";
    gsexp::ParseResult result = gsexp::parse("(root (value " + out_of_range + "))");
    require(result.ok, "parse out-of-range int");
    require(!gsexp::extract_int(result.root(0), "value").has_value(), "reject out-of-range int");
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
    gsexp::Node root = result.root(0);
    require(gsexp::extract_int(root, "good_int") == 42, "accept plus int");
    require(!gsexp::extract_int(root, "bad_int").has_value(), "reject int suffix");
    require(!gsexp::extract_int(root, "sign_only").has_value(), "reject sign-only int");
    require(gsexp::extract_float(root, "good_float").has_value(), "accept plus float");
    require(!gsexp::extract_float(root, "bad_float").has_value(), "reject float suffix");
    require(!gsexp::extract_float(root, "bad_exp").has_value(), "reject incomplete exponent");
    require(!gsexp::extract_float(root, "bad_nan").has_value(), "reject nan float");
    require(!gsexp::extract_float(root, "bad_inf").has_value(), "reject inf float");
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

    std::cout << "gsexp_tests passed\n";
    return 0;
}
