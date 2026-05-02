# reflect-cpp-test

Small test repo for the `reflect-cpp` library.

[![build](https://github.com/jon-smith/reflect-cpp-test/actions/workflows/build.yaml/badge.svg)](https://github.com/jon-smith/reflect-cpp-test/actions/workflows/build.yaml)

## Build (MacOS)

> Other OS/compiler configurations are in `build.yaml`.

Requires `CMake 3.23` and a compiler with C++23 library support, such as
`clang 16`.

```sh
cmake --preset clang-release # or -debug
cmake --build out/build/clang-release
out/build/clang-release/src/reflect_cpp_test
```

> Recommended: [VS code CMake extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)

## OpenAPI demo

The app turns C++ DTOs into an OpenAPI 3.1 document for a small 'CatLog' API.

It uses `rfl::json::to_schema<T>()` to generate JSON Schema for each C++ type,
then moves schema references from `#/$defs/...` to
`#/components/schemas/...`.

Routes:

- `GET /cats`
- `POST /cats`
- `GET /cats/{catId}`
- `GET /cats/{catId}/logs`
- `POST /cats/{catId}/logs`

Schemas are generated from C++ types such as:

- `Cat`
- `CatSummary`
- `CreateCatRequest`
- `CatLogEntry`
- `CreateCatLogEntryRequest`
- `CatListResponse`
- `CatLogListResponse`
- `ErrorResponse`

Cat data uses `dateOfBirth`, and log entries track moods such as `sassy`,
`sleepy`, `zoomy`, and `cute`.

The repo is also an experiment in reusable OpenAPI and route-registration code
on top of `reflect-cpp`.

Reusable `clam` modules:

- `openapi_builder`
- `typed_api_routes`
- `api_server_support`

Source layout:

- `src/core`: reusable library code
- `src/demo`: CatLog demo code

CMake targets:

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

The binary prints formatted OpenAPI 3.1 JSON to `stdout`.

Run the demo HTTP server instead:

```sh
out/build/clang-release/src/reflect_cpp_test --serve
```

The server listens on `http://localhost:8080`, serves the spec from
`GET /openapi.json`, and mounts the stub CatLog routes.

## Verify

Run the Catch2 tests:

```sh
ctest --test-dir out/build/clang-release --output-on-failure
```

Test targets:

- `reflect_cpp_openapi_core_tests`
- `reflect_cpp_openapi_demo_tests`

Test sources:

- `src/core/tests`
- `src/demo/tests`

## Format

Requires `clang-format` 22 (defined in CI pipelines).

Apply `clang-format` to the tracked C++ files:

```sh
git ls-files '*.cpp' '*.hpp' | xargs clang-format -i
```

Run a non-mutating formatting check locally using the same file selection as CI:

```sh
git ls-files '*.cpp' '*.hpp' | xargs clang-format --dry-run --Werror
```
