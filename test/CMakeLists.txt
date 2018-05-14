file(GLOB tests_sources
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
  shared_task_tests.cpp
  sync_wait_tests.cpp
  single_consumer_async_auto_reset_event_tests.cpp
  when_all_tests.cpp
  when_all_ready_tests.cpp
  ip_address_tests.cpp
  ip_endpoint_tests.cpp
  ipv4_address_tests.cpp
  ipv4_endpoint_tests.cpp
  ipv6_address_tests.cpp
  ipv6_endpoint_tests.cpp)

if(WIN32)
  file(GLOB win32_tests_sources
    scheduling_operator_tests.cpp
    io_service_tests.cpp
    file_tests.cpp)
  list(APPEND tests_sources ${win32_tests_sources})
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX test FILES ${tests_sources})

add_executable(tests ${tests_sources})
target_link_libraries(tests PRIVATE cppcoro)
set_target_properties(tests PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
