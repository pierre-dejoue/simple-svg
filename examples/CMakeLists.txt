# Build examples

#
# Common utilities for all examples
#
add_library(examples_common STATIC
    common/utils.cpp
)

target_include_directories(examples_common
    PUBLIC
    common/include
)

target_link_libraries(examples_common
    PUBLIC
    ssvg
)

#
# example_01_round_trip
#
add_executable(example_01_round_trip
    example_01_round_trip/main.cpp
)

target_link_libraries(example_01_round_trip
    PRIVATE
    examples_common
    ssvg
    stdutils
)

#
# example_02_build_svg
#
add_executable(example_02_build_svg
    example_02_build_svg/main.cpp
)

target_link_libraries(example_02_build_svg
    PRIVATE
    examples_common
    ssvg
    stdutils
)

if(MSVC)
    # Correct definition of macro __cplusplus on Visual Studio
    # https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
    target_compile_options(examples_common       PRIVATE "/Zc:__cplusplus")
    target_compile_options(example_01_round_trip PRIVATE "/Zc:__cplusplus")
    target_compile_options(example_02_build_svg  PRIVATE "/Zc:__cplusplus")
endif()
