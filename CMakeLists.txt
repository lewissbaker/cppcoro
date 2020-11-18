cmake_minimum_required(VERSION 3.16)

function(join outvar)
    cmake_parse_arguments(join "" "PATH" "FILES" ${ARGN})
    set(result "${${outvar}}")
    foreach (file IN LISTS join_FILES)
        string(PREPEND file "${join_PATH}/")
        list(APPEND result "${file}")
    endforeach ()
    set("${outvar}" "${result}" PARENT_SCOPE)
endfunction()

project(cppcoro VERSION 0.1.0)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

option (CPPCORO_ENABLE_TESTS "Enable tests for CppCoro" ${BUILD_TESTING})

include(GNUInstallDirs)
message(STATUS "LIBDIR=${CMAKE_INSTALL_LIBDIR}")
message(STATUS "INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}")
message(STATUS "VERSION=${PROJECT_VERSION}")

join(includes
        PATH "include/cppcoro"
        FILES
        awaitable_traits.hpp
        is_awaitable.hpp
        async_auto_reset_event.hpp
        async_manual_reset_event.hpp
        async_generator.hpp
        async_mutex.hpp
        async_latch.hpp
        async_scope.hpp
        broken_promise.hpp
        cancellation_registration.hpp
        cancellation_source.hpp
        cancellation_token.hpp
        task.hpp
        sequence_barrier.hpp
        sequence_traits.hpp
        single_producer_sequencer.hpp
        multi_producer_sequencer.hpp
        shared_task.hpp
        single_consumer_event.hpp
        single_consumer_async_auto_reset_event.hpp
        sync_wait.hpp
        task.hpp
        io_service.hpp
        config.hpp
        on_scope_exit.hpp
        file_share_mode.hpp
        file_open_mode.hpp
        file_buffering_mode.hpp
        file.hpp
        fmap.hpp
        when_all.hpp
        when_all_ready.hpp
        resume_on.hpp
        schedule_on.hpp
        generator.hpp
        readable_file.hpp
        recursive_generator.hpp
        writable_file.hpp
        read_only_file.hpp
        write_only_file.hpp
        read_write_file.hpp
        file_read_operation.hpp
        file_write_operation.hpp
        static_thread_pool.hpp
        )

join(netIncludes PATH "include/cppcoro/net" FILES
        ip_address.hpp
        ip_endpoint.hpp
        ipv4_address.hpp
        ipv4_endpoint.hpp
        ipv6_address.hpp
        ipv6_endpoint.hpp
        socket.hpp
        )

join(detailIncludes PATH "include/cppcoro/detail" FILES
        void_value.hpp
        when_all_ready_awaitable.hpp
        when_all_counter.hpp
        when_all_task.hpp
        get_awaiter.hpp
        is_awaiter.hpp
        any.hpp
        remove_rvalue_reference.hpp
        sync_wait_task.hpp
        unwrap_reference.hpp
        lightweight_manual_reset_event.hpp
        )

join(privateHeaders PATH "lib" FILES
        cancellation_state.hpp
        socket_helpers.hpp
        auto_reset_event.hpp
        spin_wait.hpp
        spin_mutex.hpp
        )

join(sources PATH "lib" FILES
        async_auto_reset_event.cpp
        async_manual_reset_event.cpp
        async_mutex.cpp
        cancellation_state.cpp
        cancellation_token.cpp
        cancellation_source.cpp
        cancellation_registration.cpp
        lightweight_manual_reset_event.cpp
        ip_address.cpp
        ip_endpoint.cpp
        ipv4_address.cpp
        ipv4_endpoint.cpp
        ipv6_address.cpp
        ipv6_endpoint.cpp
        static_thread_pool.cpp
        auto_reset_event.cpp
        spin_wait.cpp
        spin_mutex.cpp
        )


if (WIN32)
    join(detailIncludes PATH "include/cppcoro/detail" FILES
            win32.hpp
            win32_overlapped_operation.hpp
            )
    join(netIncludes PATH "include/cppcoro/net" FILES
            socket.hpp
            socket_accept_operation.hpp
            socket_connect_operation.hpp
            socket_disconnect_operation.hpp
            socket_recv_operation.hpp
            socket_recv_from_operation.hpp
            socket_send_operation.hpp
            socket_send_to_operation.hpp
            )

    join(sources PATH "lib" FILES
            win32.cpp
            io_service.cpp
            file.cpp
            readable_file.cpp
            writable_file.cpp
            read_only_file.cpp
            write_only_file.cpp
            read_write_file.cpp
            file_read_operation.cpp
            file_write_operation.cpp
            socket_helpers.cpp
            socket.cpp
            socket_accept_operation.cpp
            socket_connect_operation.cpp
            socket_disconnect_operation.cpp
            socket_send_operation.cpp
            socket_send_to_operation.cpp
            socket_recv_operation.cpp
            socket_recv_from_operation.cpp)
endif ()
message(STATUS "sources=${sources}")

add_library(cppcoro_cppcoro ${sources} ${privateHeaders} ${includes} ${netIncludes} ${detailIncludes})
add_library(cppcoro::cppcoro ALIAS cppcoro_cppcoro)
target_include_directories(cppcoro_cppcoro
        PRIVATE
        lib)
target_include_directories(cppcoro_cppcoro PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
find_package(Threads REQUIRED)
target_link_libraries(cppcoro_cppcoro PUBLIC Threads::Threads)
set_target_properties(cppcoro_cppcoro PROPERTIES EXPORT_NAME cppcoro)

if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR CPPCORO_ENABLE_TESTS) AND BUILD_TESTING)
    add_subdirectory(test)
endif()

install(TARGETS cppcoro_cppcoro
        EXPORT cppcoro_targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )

install(EXPORT cppcoro_targets
        FILE cppcoro-targets.cmake
        NAMESPACE cppcoro::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppcoro
        )

include(CMakePackageConfigHelpers)

configure_package_config_file(cmake/config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cppcoro-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppcoro
        )

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/cppcoro-config-version.cmake
        VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion
        )

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/cppcoro-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cppcoro-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppcoro
        )

install(DIRECTORY include/cppcoro/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cppcoro)
