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
document for a CatLog webserver-style API, while also carving out a reusable
core library for future APIs.

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

This is intended as both:

- a contract-generation example for a future C++ webserver
- an experiment in building a reusable OpenAPI/route-registration library on
  top of `reflect-cpp`

The reusable code now lives in `clam`-namespaced modules such as:

- `openapi_builder`
- `typed_api_routes`
- `api_server_support`

The current CMake targets are split into:

- `reflect_cpp_openapi_core`: reusable OpenAPI builder, typed route DSL, and
  shared server helpers
- `reflect_cpp_openapi_demo`: CatLog demo routes/server/spec wiring built on
  top of the core library
- `reflect_cpp_test`: demo executable

## Run

```sh
cmake --preset clang-release # or -debug
cmake --build out/build/clang-release
out/build/clang-release/src/reflect_cpp_test
```

The binary prints a pretty-formatted OpenAPI 3.1 JSON document to `stdout`.

Run the demo HTTP server instead:

```sh
out/build/clang-release/src/reflect_cpp_test --serve
```

The server listens on `http://localhost:8080`, serves the generated OpenAPI
document from `GET /openapi.json`, and mounts stub CatLog demo routes that
share the same route registry as the OpenAPI generator.

## Verify

The repo validates OpenAPI generation with a focused Catch2 test suite that
covers both the reusable library layer and the CatLog demo layer:

```sh
ctest --test-dir out/build/clang-release --output-on-failure
```

The tests are split into:

- `reflect_cpp_openapi_core_tests`
- `reflect_cpp_openapi_demo_tests`

## Format

Apply `clang-format` to the tracked C++ files:

```sh
git ls-files '*.cpp' '*.hpp' | xargs clang-format -i
```

Run a non-mutating formatting check locally using the same file selection as CI:

```sh
git ls-files '*.cpp' '*.hpp' | xargs clang-format --dry-run --Werror
```
