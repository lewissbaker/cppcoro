cmake_minimum_required(VERSION 3.14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(FetchContent)
set(DOCTEST_NO_INSTALL OFF CACHE BOOL "" FORCE)
fetchcontent_declare(_fetch_doctest
  PREFIX ${CMAKE_BINARY_DIR}/doctest
  GIT_REPOSITORY https://github.com/onqtam/doctest.git
  LOG_DOWNLOAD ON
  )
fetchcontent_makeavailable(_fetch_doctest)
set_target_properties(doctest_with_main PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
add_library(test_main main.cpp counted.cpp)

# note: doctest_with_main looks broken
target_link_libraries(test_main PUBLIC doctest::doctest)
link_libraries(${PROJECT_NAME} test_main)

macro(make_test _target)
  add_executable(${_target} ${ARGV})
  doctest_discover_tests(${_target})
endmacro()

make_test(async_generator_tests async_generator_tests.cpp)
make_test(async_latch_tests async_latch_tests.cpp)
make_test(async_manual_reset_event_tests async_manual_reset_event_tests.cpp)
make_test(async_mutex_tests async_mutex_tests.cpp)
make_test(cancellation_token_tests cancellation_token_tests.cpp)
make_test(file_tests file_tests.cpp)
make_test(generator_tests generator_tests.cpp)
make_test(io_service_tests io_service_tests.cpp)
make_test(ip_address_tests ip_address_tests.cpp)
make_test(ip_endpoint_tests ip_endpoint_tests.cpp)
make_test(ipv4_address_tests ipv4_address_tests.cpp)
make_test(ipv4_endpoint_tests ipv4_endpoint_tests.cpp)
make_test(ipv6_address_tests ipv6_address_tests.cpp)
make_test(ipv6_endpoint_tests ipv6_endpoint_tests.cpp)
make_test(multi_producer_sequencer_tests multi_producer_sequencer_tests.cpp)
make_test(recursive_generator_tests recursive_generator_tests.cpp)
make_test(scheduling_operator_tests scheduling_operator_tests.cpp)
make_test(sequence_barrier_tests sequence_barrier_tests.cpp)
make_test(shared_task_tests shared_task_tests.cpp)
make_test(single_consumer_async_auto_reset_event_tests single_consumer_async_auto_reset_event_tests.cpp)
make_test(single_producer_sequencer_tests single_producer_sequencer_tests.cpp)
make_test(socket_tests socket_tests.cpp)
make_test(static_thread_pool_tests static_thread_pool_tests.cpp)
make_test(sync_wait_tests sync_wait_tests.cpp)
make_test(task_tests task_tests.cpp)
make_test(when_all_ready_tests when_all_ready_tests.cpp)
make_test(when_all_tests when_all_tests.cpp)
