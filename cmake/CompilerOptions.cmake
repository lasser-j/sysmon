# cmake/CompilerOptions.cmake
#
# Provides two helper functions used by every module:
#
#   target_set_warnings(<target> <PRIVATE|PUBLIC|INTERFACE>)
#       Applies a strict, consistent warning set.
#
#   target_enable_sanitizers(<target> <PRIVATE|PUBLIC|INTERFACE>)
#       Enables the sanitizer selected by the SANITIZER cache variable.
#       Use only for test targets and the library copies they link against - not for benchmarks, production binaries or cross-compiled targets.
#
# The SANITIZER cache variable selects the sanitizer set for the whole build tree:
#
#   address (default)  ASan + UBSan
#   thread             TSan
#
# ASan and TSan cannot be combined, so TSan needs its own build directory (e.g. -B build-tsan -DSANITIZER=thread).

set(SANITIZER "address" CACHE STRING "Sanitizer for test targets: address (ASan + UBSan) or thread (TSan)")
set_property(CACHE SANITIZER PROPERTY STRINGS address thread)

if(NOT SANITIZER STREQUAL "address" AND NOT SANITIZER STREQUAL "thread")
    message(FATAL_ERROR "Invalid SANITIZER '${SANITIZER}', expected 'address' or 'thread'")
endif()

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
        if(SANITIZER STREQUAL "thread")
            set(_sanitizer_flags -fsanitize=thread)
        else()
            set(_sanitizer_flags -fsanitize=address,undefined)
        endif()
        target_compile_options(${target} ${scope} ${_sanitizer_flags} -fno-omit-frame-pointer)
        target_link_options(${target} ${scope} ${_sanitizer_flags})
    endif()
endfunction()
