# gsexp

<p>
  <img src="assets/logo.svg" alt="gsexp logo" width="96" height="96">
</p>

`gsexp` is a small C++20 S-expression parser/writer helper. It is meant for
simple config and data files used by vendored game libraries.

It parses symbols, strings, integers, floats, and lists into plain structs with
line/column diagnostics. It is not a Lisp interpreter. It has no evaluator,
macros, schema system, file search policy, or dependencies outside the C++
standard library.

## Target

- `gsexp::gsexp`: parser, tokenizer, value helpers, and string quoting.

## Add To A Project

The intended integration path is vendored source with CMake `add_subdirectory`.
Put `gsexp` under your project, for example:

```text
third_party/
  gsexp/
```

Then wire it into your CMake project:

```cmake
set(GSEXP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GSEXP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/gsexp)

target_link_libraries(my_lib PUBLIC gsexp::gsexp)
```

Libraries that use `gsexp`, such as `glayout`, should link the same
`gsexp::gsexp` target instead of copying parser source into each repo.

## Basic Use

```cpp
const char* text = R"(
(settings
  (name "demo")
  (width 1280)
  (scale 1.5))
)";

gsexp::ParseResult result = gsexp::parse(text);
if (!result.ok) {
    // Print result.diagnostics.
}

const gsexp::Value& root = result.values.front();
std::optional<std::string> name = gsexp::extract_string(root, "name");
std::optional<int> width = gsexp::extract_int(root, "width");
std::optional<float> scale = gsexp::extract_float(root, "scale");
```

`gsexp` does not decide whether a key is valid for your file format. Parse the
tree, then validate it in the owning library or application.

## Build

```sh
./scripts/build.sh
```

The build script also runs the test executable through CTest.

## Run Demo

```sh
./scripts/run.sh
```

See [docs/spec.md](docs/spec.md) for design boundaries.
