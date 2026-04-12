include(Catch)

add_executable(reflect_cpp_openapi_core_tests
  openapi_tests.cpp
)

target_link_libraries(reflect_cpp_openapi_core_tests
  PRIVATE
    Catch2::Catch2WithMain
    reflect_cpp_openapi_core
)

catch_discover_tests(reflect_cpp_openapi_core_tests)
