add_library(doctest::doctest INTERFACE IMPORTED)
target_include_directories(doctest::doctest INTERFACE doctest)

include(${CMAKE_CURRENT_LIST_DIR}/doctest/doctest.cmake)

find_package(Threads REQUIRED)

add_library(tests-main STATIC
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
else()
	# let more time for some tests
	set(async_auto_reset_event_tests_TIMEOUT 60)
endif()

foreach(test ${tests})
	get_filename_component(test_name ${test} NAME_WE)
	add_executable(${test_name} ${test})
	target_link_libraries(${test_name} PRIVATE tests-main)
	string(REPLACE "_" " " test_prefix ${test_name})
	if (NOT DEFINED ${test_name}_TIMEOUT)
		set(${test_name}_TIMEOUT 30)
	endif()
	doctest_discover_tests(${test_name} TEST_PREFIX ${test_prefix}- PROPERTIES TIMEOUT ${${test_name}_TIMEOUT})
endforeach()
