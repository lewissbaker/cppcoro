include(CheckCXXCompilerFlag)
include(CheckIncludeFileCXX)
include(FindPackageHandleStandardArgs)

check_cxx_compiler_flag(/await Coroutines_SUPPORTS_MS_FLAG)
check_cxx_compiler_flag(-fcoroutines-ts Coroutines_SUPPORTS_COROUTINES_TS_FLAG)
check_cxx_compiler_flag(-fcoroutines Coroutines_SUPPORTS_COROUTINES_FLAG)
if(Coroutines_SUPPORTS_MS_FLAG OR Coroutines_SUPPORTS_COROUTINES_TS_FLAG OR Coroutines_SUPPORTS_COROUTINES_FLAG)
    set(Coroutines_COMPILER_SUPPORT ON)
endif()

if(Coroutines_SUPPORTS_MS_FLAG)
    set(Coroutines_EXTRA_FLAGS "/await")
elseif(Coroutines_SUPPORTS_COROUTINES_TS_FLAG)
    set(Coroutines_EXTRA_FLAGS "-fcoroutines-ts")
elseif(Coroutines_SUPPORTS_COROUTINES_FLAG)
    set(Coroutines_EXTRA_FLAGS "-fcoroutines")
endif()

check_include_file_cxx("coroutine" Coroutines_STANDARD_LIBRARY_SUPPORT ${Coroutines_EXTRA_FLAGS})
check_include_file_cxx("experimental/coroutine" Coroutines_EXPERIMENTAL_LIBRARY_SUPPORT ${Coroutines_EXTRA_FLAGS})

if(Coroutines_EXPERIMENTAL_LIBRARY_SUPPORT OR Coroutines_STANDARD_LIBRARY_SUPPORT)
    set(Coroutines_LIBRARY_SUPPORT ON)
endif()

find_package_handle_standard_args(CppcoroCoroutines
    REQUIRED_VARS Coroutines_LIBRARY_SUPPORT Coroutines_COMPILER_SUPPORT
    FAIL_MESSAGE "Verify that the compiler and the standard library both support the Coroutines TS")

if(NOT CppcoroCoroutines_FOUND OR TARGET cppcoro::coroutines)
    return()
endif()

add_library(cppcoro::coroutines INTERFACE IMPORTED)
target_compile_options(cppcoro::coroutines INTERFACE ${Coroutines_EXTRA_FLAGS})
