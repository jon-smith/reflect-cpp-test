include(Catch)

add_executable(reflect_cpp_openapi_tests
  openapi_tests.cpp
)

target_link_libraries(reflect_cpp_openapi_tests
  PRIVATE
    Catch2::Catch2WithMain
    reflect_cpp_openapi
)

catch_discover_tests(reflect_cpp_openapi_tests)
