#include "gsexp/sexp.hpp"

#include <iostream>

int main() {
    const char* text = R"(
(settings
  (name "demo")
  (width 1280)
  (scale 1.5)
)
)";

    gsexp::ParseResult result = gsexp::parse(text);
    if (!result.ok) {
        for (const gsexp::Diagnostic& diagnostic : result.diagnostics) {
            std::cerr << diagnostic.line << ":" << diagnostic.column << ": "
                      << diagnostic.message << "\n";
        }
        return 1;
    }

    if (result.root_count() == 0) {
        std::cerr << "no values parsed\n";
        return 1;
    }

    gsexp::FormView root(result.root(0));
    std::optional<std::string> name = root.get_string("name");
    std::optional<int> width = root.get_int("width");
    std::optional<float> scale = root.get_float("scale");

    std::cout << "name=" << (name ? *name : "<missing>") << "\n";
    std::cout << "width=" << (width ? std::to_string(*width) : "<missing>") << "\n";
    std::cout << "scale=" << (scale ? std::to_string(*scale) : "<missing>") << "\n";
    std::cout << "quoted=" << gsexp::quote_string("hello \"sexp\"") << "\n";

    return 0;
}
