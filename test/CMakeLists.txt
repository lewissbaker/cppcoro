cmake_minimum_required(VERSION 3.14)

link_libraries(${PROJECT_NAME})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(doctest QUIET)
if(NOT doctest)
    include(FetchContent)
    FetchContent_Declare(doctest
        PREFIX ${CMAKE_BINARY_DIR}/doctest
        GIT_REPOSITORY https://github.com/onqtam/doctest.git
        LOG_DOWNLOAD ON
    )
    FetchContent_MakeAvailable(doctest)
    link_libraries(doctest)
    include_directories(SYSTEM ${doctest_SOURCE_DIR})
endif()

link_libraries(doctest::doctest)

add_executable(async_generator_tests
    async_generator_tests.cpp
)

add_executable(file_tests
  file_tests.cpp
  )
