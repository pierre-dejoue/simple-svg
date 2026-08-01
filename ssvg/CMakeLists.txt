#
# Simple-SVG (SSVG) library
#
#include(compiler_options)
include(bx)

set(SOURCES
    src/ssvg.cpp
    src/ssvg_builder.cpp
    src/ssvg_parser.cpp
    src/ssvg_writer.cpp
)

file(GLOB PUBLIC_HEADERS include/ssvg/*.h)
file(GLOB PRIVATE_HEADERS src/*.h)

add_library(ssvg ${SOURCES} ${PUBLIC_HEADERS} ${PRIVATE_HEADERS})

#set_target_warnings(ssvg ON)

target_include_directories(ssvg
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Debug trace
target_compile_definitions(ssvg PRIVATE
    $<$<CONFIG:Debug>:SSVG_CONFIG_DEBUG=1>
    $<$<CONFIG:Debug>:SSVG_CONFIG_BX_DEBUG=0>
)

target_link_libraries(ssvg
    PRIVATE
    bx
    stdutils
)

if(MSVC)
    # Correct definition of macro __cplusplus on Visual Studio
    # https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
    target_compile_options(ssvg PRIVATE "/Zc:__cplusplus")
endif()
