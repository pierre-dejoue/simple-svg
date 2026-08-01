# Build examples
add_executable(examples
    main.cpp
)

target_link_libraries(examples
    PRIVATE
    bx
    ssvg
)

if(MSVC)
    # Correct definition of macro __cplusplus on Visual Studio
    # https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
    target_compile_options(examples PRIVATE "/Zc:__cplusplus")
endif()
