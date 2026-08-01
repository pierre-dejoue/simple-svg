cmake_minimum_required(VERSION 3.31)

project(ssvg LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

option(SSVG_BUILD_EXAMPLES "Build examples" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/third_party/")
#list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/config/")

add_subdirectory(stdutils)
add_subdirectory(ssvg)

if(SSVG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
