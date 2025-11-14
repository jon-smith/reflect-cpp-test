cmake_minimum_required(VERSION 3.23)

project(ReflectCppTest CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_CXX_STANDARD 20)

find_package(reflectcpp REQUIRED)

add_subdirectory(src)
