add_executable(reflect_cpp_test
  main.cpp
  openapi_builder.cpp
  openapi_demo.cpp
)

target_link_libraries(reflect_cpp_test PRIVATE reflectcpp::reflectcpp)

if(BUILD_TESTING)
  add_test(NAME reflect_cpp_test_openapi_check
           COMMAND reflect_cpp_test --check)
endif()
