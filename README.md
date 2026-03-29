# reflect-cpp-test

Repo for testing the reflect-cpp library

[![build](https://github.com/jon-smith/reflect-cpp-test/actions/workflows/build.yaml/badge.svg)](https://github.com/jon-smith/reflect-cpp-test/actions/workflows/build.yaml)

## Build (MacOS)

> Other OS/compiler configurations shown in build.yaml

Requires `CMake 3.23` and a compiler with C++23 library support, such as
`clang 16`.

```sh
cmake --preset clang-release # or -debug
cmake --build out/build/clang-release
out/build/clang-release/src/reflect_cpp_test
```

> Recommended: [VS code CMake extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)

## OpenAPI Demo

The application now demonstrates how to turn C++ DTOs into an OpenAPI 3.1
document for a CatLog webserver-style API.

This demo uses `rfl::json::to_schema<T>()` to generate JSON Schema for each C++
type, rewrites those schema references from `#/$defs/...` to
`#/components/schemas/...`, and assembles a minimal OpenAPI 3.1 document around
them.

The generated spec includes:

- `GET /cats`
- `POST /cats`
- `GET /cats/{catId}`
- `GET /cats/{catId}/logs`
- `POST /cats/{catId}/logs`

The component schemas are generated from C++ types such as:

- `Cat`
- `CatSummary`
- `CreateCatRequest`
- `CatLogEntry`
- `CreateCatLogEntryRequest`
- `CatListResponse`
- `CatLogListResponse`
- `ErrorResponse`

Cat data now models `dateOfBirth` instead of `ageYears`, and status log entries
track cat moods like `sassy`, `sleepy`, `zoomy`, and `cute`.

This is intended as a contract-generation example for a future C++ webserver.
The routes themselves are declared explicitly in C++, while the request and
response schemas come from `reflect-cpp`.

## Run

```sh
cmake --preset clang-release # or -debug
cmake --build out/build/clang-release
out/build/clang-release/src/reflect_cpp_test
```

The binary prints a pretty-formatted OpenAPI 3.1 JSON document to `stdout`.

## Verify

The repo also wires in a lightweight smoke test that validates the generated
document:

```sh
ctest --test-dir out/build/clang-release --output-on-failure
```

Or run the executable directly:

```sh
out/build/clang-release/src/reflect_cpp_test --check
```
