if(TARGET uring::uring)
    set(uring_FOUND TRUE)
endif()

include(ExternalProject)

ExternalProject_Add(_fetch_uring
        GIT_REPOSITORY https://github.com/axboe/liburing.git
        GIT_TAG liburing-0.6
        CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}
        BUILD_COMMAND make -j4
        BUILD_IN_SOURCE 1
        )
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include)
add_library(liburing INTERFACE IMPORTED GLOBAL)
target_link_libraries(liburing INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/lib/liburing.a)
target_include_directories(liburing INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include)
add_dependencies(liburing _fetch_uring)
add_library(uring::uring ALIAS liburing)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(uring "Cannot find uring libraries")
