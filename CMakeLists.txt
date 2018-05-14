cmake_minimum_required(VERSION 3.11.0 FATAL_ERROR)
project(cppcoro LANGUAGES CXX)

option(CPPCORO_BUILD_TESTS "Build tests" ${BUILD_TESTS})
option(CPPCORO_EXTRA_WARNINGS "Build with additional warnings" ${EXTRA_WARNINGS})

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "MSVC|Clang")
  add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options(/MP)
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " /await")
  string(APPEND CMAKE_CXX_FLAGS_RELERASE " /await:heapelide")
  string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " /await:heapelide")
  string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL " /await:heapelide")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-fcoroutines-ts)
  option(LLVM_INSTALL_PREFIX "LLVM install prefix" OFF)
  if(LLVM_INSTALL_PREFIX)
    add_compile_options(-nostdinc++ -cxx-isystem "${LLVM_INSTALL_PREFIX}/include/c++/v1")
    link_directories(${LLVM_INSTALL_PREFIX}/lib)
    link_libraries(c c++)
  endif()
endif()

if(CPPCORO_EXTRA_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    string(REGEX REPLACE "[/-]W[0-9]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(APPEND CMAKE_CXX_FLAGS " /W4 /permissive-")
  else()
    string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wpedantic -Wconversion -Wno-unused-parameter")
    string(APPEND CMAKE_CXX_FLAGS " -fdiagnostics-absolute-paths -fcolor-diagnostics")
  endif()
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER build)

add_subdirectory(lib)

if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/cppcoro-config.cmake.in)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cppcoro-config.cmake.in
    [[@PACKAGE_INIT@]] "\n"
    [[include("${CMAKE_CURRENT_LIST_DIR}/cppcoro-targets.cmake")]])
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(${CMAKE_CURRENT_BINARY_DIR}/cppcoro-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/cppcoro-config.cmake INSTALL_DESTINATION lib/cmake/cppcoro)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.hpp")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cppcoro-config.cmake DESTINATION lib/cmake/cppcoro)
install(EXPORT cppcoro FILE cppcoro-targets.cmake NAMESPACE cppcoro:: DESTINATION lib/cmake/cppcoro)

if(CPPCORO_BUILD_TESTS)
  add_subdirectory(test)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT tests)
  add_custom_target(run_tests COMMAND $<TARGET_FILE:tests> DEPENDS tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} USES_TERMINAL)
  set_target_properties(run_tests PROPERTIES EXCLUDE_FROM_ALL ON)
endif()
