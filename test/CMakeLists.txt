add_library(doctest::doctest INTERFACE IMPORTED)
target_include_directories(doctest::doctest INTERFACE doctest)

find_package(Threads REQUIRED)

add_executable(run
	counted.hpp
	io_service_fixture.hpp

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
    target_sources(run PRIVATE
        scheduling_operator_tests.cpp
        io_service_tests.cpp
        file_tests.cpp
        socket_tests.cpp
    )
endif()

target_link_libraries(run PRIVATE cppcoro Threads::Threads)

add_test(NAME test COMMAND run)
