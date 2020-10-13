add_library(doctest::doctest INTERFACE IMPORTED)
target_include_directories(doctest::doctest INTERFACE doctest)

include(${CMAKE_CURRENT_LIST_DIR}/doctest/doctest.cmake)

find_package(Threads REQUIRED)

add_library(tests-main SHARED
	main.cpp
	counted.cpp
)
target_link_libraries(tests-main PUBLIC cppcoro doctest::doctest Threads::Threads)

set(tests
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
    list(APPEND tests
        scheduling_operator_tests.cpp
        io_service_tests.cpp
        file_tests.cpp
        socket_tests.cpp
    )
endif()

foreach(test ${tests})
	get_filename_component(test_name ${test} NAME_WE)
	add_executable(${test_name} ${test})
	target_link_libraries(${test_name} PRIVATE tests-main)
	doctest_discover_tests(${test_name} PROPERTIES TIMEOUT 30)
endforeach()
