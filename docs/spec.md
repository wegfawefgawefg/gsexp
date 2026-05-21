# gsexp Specification

## Purpose

`gsexp` parses and writes small S-expression data files used by lightweight C++
game libraries.

It should stay small:

- No evaluator.
- No macros.
- No schema system.
- No global registry.
- No runtime reflection.
- No dependencies outside the C++ standard library.

## Responsibilities

- Tokenize atoms, strings, and list delimiters.
- Parse one or more root expressions from a string.
- Preserve syntax-level value types: list, atom, string.
- Own parsed source storage in `ParseResult`; value text views are valid for as
  long as the owning `ParseResult` remains alive.
- Report parse diagnostics with line and column.
- Quote strings for writers in dependent libraries.
- Provide small helper functions for interpreting atoms as ints/floats and for
  common `(key value)` extraction.

## Non-Responsibilities

- Evaluating Lisp.
- Owning file formats for higher-level libraries.
- Deciding whether unknown keys are valid.
- Managing files or search paths.
- Preserving comments or original whitespace.

## Dependency Model

Extracted libraries should link one `gsexp` target instead of copying parser
source into every repo.

For local development, sibling repos can use:

```cmake
add_subdirectory(../gsexp gsexp-build)
target_link_libraries(my_lib PUBLIC gsexp::gsexp)
```

For consumers, this can later be wrapped with `FetchContent`, a git submodule,
or a normal `add_subdirectory(third_party/gsexp)` workflow. No apt/Homebrew
packaging is assumed.
