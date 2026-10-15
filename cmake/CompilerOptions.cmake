# cmake/CompilerOptions.cmake
#
# Provides two helper functions used by every module:
#
#   target_set_warnings(<target> <PRIVATE|PUBLIC|INTERFACE>)
#       Applies a strict, consistent warning set.
#
#   target_enable_sanitizers(<target> <PRIVATE|PUBLIC|INTERFACE>)
#       Enables ASan + UBSan. Use only for test and benchmark targets -
#       not for production binaries or cross-compiled targets.

set(_SYSMON_GCC_CLANG_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
)

function(target_set_warnings target scope)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} ${scope} ${_SYSMON_GCC_CLANG_WARNINGS})
    elseif(MSVC)
        target_compile_options(${target} ${scope} /W4 /permissive-)
    endif()
endfunction()

function(target_enable_sanitizers target scope)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} ${scope}
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} ${scope}
            -fsanitize=address,undefined
        )
    endif()
endfunction()
