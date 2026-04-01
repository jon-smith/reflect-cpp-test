add_library(reflect_cpp_openapi
  openapi_builder.cpp
  openapi_demo.cpp
)

target_link_libraries(reflect_cpp_openapi PUBLIC reflectcpp::reflectcpp)
target_include_directories(reflect_cpp_openapi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(reflect_cpp_test main.cpp)
target_link_libraries(reflect_cpp_test PRIVATE reflect_cpp_openapi)
