include(Catch)

add_executable(reflect_cpp_openapi_demo_tests
  cat_api_demo_tests.cpp
)

target_link_libraries(reflect_cpp_openapi_demo_tests
  PRIVATE
    Catch2::Catch2WithMain
    reflect_cpp_openapi_demo
)

catch_discover_tests(reflect_cpp_openapi_demo_tests)
