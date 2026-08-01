if(TARGET bx)
    return()
endif()

message(STATUS "Third-party: bx")

include(FetchContent)
FetchContent_Populate(
    bx
    QUIET
    GIT_REPOSITORY https://github.com/bkaradzic/bx.git
    GIT_TAG 09fa25f3aeb5fe8688f9acc0f326c706f0c44515        # Jul 15, 2023
)

# Limit the scope of bx to what is strictly necessary for simple-svg
set(BX_SOURCES
    ${bx_SOURCE_DIR}/src/allocator.cpp
    ${bx_SOURCE_DIR}/src/bx.cpp
    ${bx_SOURCE_DIR}/src/dtoa.cpp
    ${bx_SOURCE_DIR}/src/file.cpp
    ${bx_SOURCE_DIR}/src/filepath.cpp
    ${bx_SOURCE_DIR}/src/math.cpp
    ${bx_SOURCE_DIR}/src/os.cpp
    ${bx_SOURCE_DIR}/src/string.cpp
)

add_library(bx STATIC ${BX_SOURCES})

target_include_directories(bx
    PRIVATE
    ${bx_SOURCE_DIR}/3rdparty
    PUBLIC
    ${bx_SOURCE_DIR}/include
)

target_compile_definitions(bx PUBLIC BX_CONFIG_DEBUG=0)

# Macro added for retro-compatibility
target_compile_definitions(bx
    PUBLIC
    BX_ALLOC=bx::alloc
    BX_FREE=bx::free
    BX_REALLOC=bx::realloc
)

if(APPLE)
    # Required to include <malloc.h>
    target_include_directories(bx
        PUBLIC
        ${bx_SOURCE_DIR}/include/compat/osx
    )
elseif(MSVC)
    # Required to include <alloca.h>
    target_include_directories(bx
        PUBLIC
        ${bx_SOURCE_DIR}/include/compat/msvc
    )

    # Correct definition of macro __cplusplus on Visual Studio
    # https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
    target_compile_options(bx PRIVATE "/Zc:__cplusplus")

    target_compile_definitions(bx PRIVATE __STDC_FORMAT_MACROS)
endif()

set_property(TARGET bx PROPERTY FOLDER "third_parties")

set_property(TARGET bx PROPERTY THIRD_PARTY_NAME bx)
set_property(TARGET bx PROPERTY THIRD_PARTY_URL  https://github.com/bkaradzic/bx)
set_property(TARGET bx PROPERTY LICENSE_NAME     BSD-2-Clause)
set_property(TARGET bx PROPERTY LICENSE_FILE     ${bx_SOURCE_DIR}/LICENSE)
