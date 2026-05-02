add_library(reflect_cpp_openapi_core
  api_server_support.cpp
  openapi_builder.cpp
  typed_api_routes.cpp
)

target_link_libraries(reflect_cpp_openapi_core PUBLIC reflectcpp::reflectcpp httplib::httplib)
target_include_directories(reflect_cpp_openapi_core PUBLIC ${PROJECT_SOURCE_DIR}/src)

if(APPLE)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  find_library(CFNETWORK_FRAMEWORK CFNetwork REQUIRED)
  target_link_libraries(reflect_cpp_openapi_core PUBLIC ${COREFOUNDATION_FRAMEWORK} ${CFNETWORK_FRAMEWORK})
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
