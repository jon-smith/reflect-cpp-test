add_library(reflect_cpp_openapi
  api_server_support.cpp
  cat_api_routes.cpp
  cat_api_server.cpp
  openapi_builder.cpp
  openapi_demo.cpp
  typed_api_routes.cpp
)

target_link_libraries(reflect_cpp_openapi PUBLIC reflectcpp::reflectcpp httplib::httplib)

if(APPLE)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  find_library(CFNETWORK_FRAMEWORK CFNetwork REQUIRED)
  target_link_libraries(reflect_cpp_openapi PUBLIC ${COREFOUNDATION_FRAMEWORK} ${CFNETWORK_FRAMEWORK})
endif()

target_include_directories(reflect_cpp_openapi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(reflect_cpp_test main.cpp)
target_link_libraries(reflect_cpp_test PRIVATE reflect_cpp_openapi)
