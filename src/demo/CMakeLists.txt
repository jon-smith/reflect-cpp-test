add_library(reflect_cpp_openapi_demo
  cat_api_routes.cpp
  cat_api_server.cpp
  openapi_demo.cpp
)

target_link_libraries(reflect_cpp_openapi_demo PUBLIC reflect_cpp_openapi_core)
target_include_directories(reflect_cpp_openapi_demo PUBLIC ${PROJECT_SOURCE_DIR}/src)

add_executable(reflect_cpp_test main.cpp)
target_link_libraries(reflect_cpp_test PRIVATE reflect_cpp_openapi_demo)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
