###############################################################################
# Copyright (c) Lewis Baker
# Licenced under MIT license. See LICENSE.txt for details.
###############################################################################

set(headers 
        counted.hpp
        io_service_fixture.hpp
        )

set(sources
        main.cpp
        counted.cpp
        generator_tests.cpp
        recursive_generator_tests.cpp
        async_generator_tests.cpp
        async_auto_reset_event_tests.cpp
        async_manual_reset_event_tests.cpp
        async_mutex_tests.cpp
        async_latch_tests.cpp
        cancellation_token_tests.cpp
        task_tests.cpp
        sequence_barrier_tests.cpp
        shared_task_tests.cpp
        sync_wait_tests.cpp
        single_consumer_async_auto_reset_event_tests.cpp
        single_producer_sequencer_tests.cpp
        multi_producer_sequencer_tests.cpp
        when_all_tests.cpp
        when_all_ready_tests.cpp
        ip_address_tests.cpp
        ip_endpoint_tests.cpp
        ipv4_address_tests.cpp
        ipv4_endpoint_tests.cpp
        ipv6_address_tests.cpp
        ipv6_endpoint_tests.cpp
        static_thread_pool_tests.cpp
        )

if(WIN32)
    list(APPEND sources
            scheduling_operator_tests.cpp
            io_service_tests.cpp
            file_tests.cpp
            socket_tests.cpp
            )
endif()

add_executable(test_exe ${sources} ${headers})
if (CMAKE_BUILD_TYPE STREQUAL Release OR CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    target_compile_definitions(test_exe PRIVATE CPPCORO_BUILD_OPTIMISED)
    else()
    target_compile_definitions(test_exe PRIVATE CPPCORO_BUILD_DEBUG)
endif()
target_link_libraries(test_exe PRIVATE cppcoro::cppcoro)

add_test(test_all COMMAND test_exe)

