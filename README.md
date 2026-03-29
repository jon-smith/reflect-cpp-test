# reflect-cpp-test

Repo for testing the reflect-cpp library

[![build](https://github.com/jon-smith/reflect-cpp-test/actions/workflows/build.yaml/badge.svg)](https://github.com/jon-smith/reflect-cpp-test/actions/workflows/build.yaml)

## Build (MacOS)

> Other OS/compiler configurations shown in build.yaml

Requires `CMake 3.23` and `clang 16`.

```sh
cmake --preset clang-release # or -debug
cmake --build out/build/clang-release
out/build/clang-release/src/reflect_cpp_test
```

> Recommended: [VS code CMake extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)

## Progress

Work done so far:

- Simple `CMake` + `vcpkg` setup
- Code example showing JSON serialisation using `reflect-cpp`
