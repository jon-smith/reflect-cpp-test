add_library(reflect_cpp_openapi_core
  api_server_support.cpp
  openapi_builder.cpp
  typed_api_routes.cpp
)

target_link_libraries(reflect_cpp_openapi_core PUBLIC reflectcpp::reflectcpp httplib::httplib)
target_include_directories(reflect_cpp_openapi_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(reflect_cpp_openapi_demo
  cat_api_routes.cpp
  cat_api_server.cpp
  openapi_demo.cpp
)

target_link_libraries(reflect_cpp_openapi_demo PUBLIC reflect_cpp_openapi_core)
target_include_directories(reflect_cpp_openapi_demo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(APPLE)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  find_library(CFNETWORK_FRAMEWORK CFNetwork REQUIRED)
  target_link_libraries(reflect_cpp_openapi_core PUBLIC ${COREFOUNDATION_FRAMEWORK} ${CFNETWORK_FRAMEWORK})
endif()

add_executable(reflect_cpp_test main.cpp)
target_link_libraries(reflect_cpp_test PRIVATE reflect_cpp_openapi_demo)
