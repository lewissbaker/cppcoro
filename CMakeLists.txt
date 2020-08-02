cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

include(CheckIncludeFileCXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(cppcoro)

set(${PROJECT_NAME}_SOURCES
  # headers
  include/cppcoro/async_auto_reset_event.hpp
  include/cppcoro/async_generator.hpp
  include/cppcoro/async_latch.hpp
  include/cppcoro/async_manual_reset_event.hpp
  include/cppcoro/async_mutex.hpp
  include/cppcoro/async_scope.hpp
  include/cppcoro/awaitable_traits.hpp
  include/cppcoro/broken_promise.hpp
  include/cppcoro/cancellation_registration.hpp
  include/cppcoro/cancellation_source.hpp
  include/cppcoro/cancellation_token.hpp
  include/cppcoro/config.hpp
  include/cppcoro/detail/any.hpp
  include/cppcoro/detail/get_awaiter.hpp
  include/cppcoro/detail/is_awaiter.hpp
  include/cppcoro/detail/lightweight_manual_reset_event.hpp
  include/cppcoro/detail/manual_lifetime.hpp
  include/cppcoro/detail/remove_rvalue_reference.hpp
  include/cppcoro/detail/sync_wait_task.hpp
  include/cppcoro/detail/unwrap_reference.hpp
  include/cppcoro/detail/void_value.hpp
  include/cppcoro/detail/when_all_counter.hpp
  include/cppcoro/detail/when_all_ready_awaitable.hpp
  include/cppcoro/detail/when_all_task.hpp
  include/cppcoro/file.hpp
  include/cppcoro/file_buffering_mode.hpp
  include/cppcoro/file_open_mode.hpp
  include/cppcoro/file_read_operation.hpp
  include/cppcoro/file_share_mode.hpp
  include/cppcoro/file_write_operation.hpp
  include/cppcoro/fmap.hpp
  include/cppcoro/generator.hpp
  include/cppcoro/inline_scheduler.hpp
  include/cppcoro/is_awaitable.hpp
  include/cppcoro/multi_producer_sequencer.hpp
  include/cppcoro/on_scope_exit.hpp
  include/cppcoro/operation_cancelled.hpp
  include/cppcoro/readable_file.hpp
  include/cppcoro/read_only_file.hpp
  include/cppcoro/read_write_file.hpp
  include/cppcoro/recursive_generator.hpp
  include/cppcoro/resume_on.hpp
  include/cppcoro/round_robin_scheduler.hpp
  include/cppcoro/schedule_on.hpp
  include/cppcoro/sequence_barrier.hpp
  include/cppcoro/sequence_range.hpp
  include/cppcoro/sequence_traits.hpp
  include/cppcoro/shared_task.hpp
  include/cppcoro/single_consumer_async_auto_reset_event.hpp
  include/cppcoro/single_consumer_event.hpp
  include/cppcoro/single_producer_sequencer.hpp
  include/cppcoro/static_thread_pool.hpp
  include/cppcoro/sync_wait.hpp
  include/cppcoro/task.hpp
  include/cppcoro/when_all.hpp
  include/cppcoro/when_all_ready.hpp
  include/cppcoro/writable_file.hpp
  include/cppcoro/write_only_file.hpp

  include/cppcoro/net/ipv4_address.hpp
  include/cppcoro/net/ipv4_endpoint.hpp
  include/cppcoro/net/ipv6_address.hpp
  include/cppcoro/net/ipv6_endpoint.hpp
  include/cppcoro/net/ip_address.hpp
  include/cppcoro/net/ip_endpoint.hpp
  include/cppcoro/io_service.hpp

  include/cppcoro/file.hpp
  include/cppcoro/read_only_file.hpp
  include/cppcoro/file_read_operation.hpp
  include/cppcoro/readable_file.hpp

  include/cppcoro/net/socket.hpp
  include/cppcoro/net/socket_accept_operation.hpp
  include/cppcoro/net/socket_connect_operation.hpp
  include/cppcoro/net/socket_disconnect_operation.hpp
  include/cppcoro/net/socket_recv_from_operation.hpp
  include/cppcoro/net/socket_recv_operation.hpp
  include/cppcoro/net/socket_send_operation.hpp
  include/cppcoro/net/socket_send_to_operation.hpp

  # sources
  lib/async_auto_reset_event.cpp
  lib/async_manual_reset_event.cpp
  lib/async_mutex.cpp
  lib/auto_reset_event.cpp
  lib/cancellation_registration.cpp
  lib/cancellation_source.cpp
  lib/cancellation_state.cpp
  lib/cancellation_token.cpp
  lib/lightweight_manual_reset_event.cpp
  lib/spin_mutex.cpp
  lib/spin_wait.cpp
  lib/static_thread_pool.cpp
  lib/ipv4_address.cpp
  lib/ipv4_endpoint.cpp
  lib/ipv6_address.cpp
  lib/ipv6_endpoint.cpp
  lib/ip_address.cpp
  lib/ip_endpoint.cpp
  lib/io_service.cpp

  lib/file.cpp
  lib/read_only_file.cpp
  lib/file_read_operation.cpp
  lib/readable_file.cpp
  lib/file_write_operation.cpp
  lib/writable_file.cpp
  lib/write_only_file.cpp
  lib/read_write_file.cpp

  lib/socket.cpp
  lib/socket_accept_operation.cpp
  lib/socket_connect_operation.cpp
  lib/socket_disconnect_operation.cpp
  lib/socket_helpers.cpp
  lib/socket_recv_from_operation.cpp
  lib/socket_recv_operation.cpp
  lib/socket_send_operation.cpp
  lib/socket_send_to_operation.cpp
)


if(WIN32)
  list(APPEND ${PROJECT_NAME}_SOURCES
    # headers

    # sources
    lib/win32.cpp
  )

  list(APPEND ${PROJECT_NAME}_SOURCES
    # headers
    include/cppcoro/detail/win32.hpp
    include/cppcoro/detail/io_operation.hpp
  )
else()
  list(APPEND ${PROJECT_NAME}_SOURCES
          # headers
          include/cppcoro/detail/linux.hpp
          # sources
          lib/linux.cpp
          )
endif()

add_library(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC include)

if(WIN32)
  target_compile_options(${PROJECT_NAME} PUBLIC "/std:c++latest")
elseif(CMAKE_CXX_COMPILER_ID MATCHES GNU)
  target_compile_options(${PROJECT_NAME} PUBLIC -fcoroutines)
elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang)
  target_compile_options(${PROJECT_NAME} PUBLIC -stdlib=libc++)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if(UNIX)
  find_package(Threads REQUIRED)
  find_package(uring REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads uring::uring rt uuid)
endif()


if(CMAKE_CXX_COMPILER_ID STREQUAL GNU AND CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL 10)
  # TODO remove this check when cmake is able to detect it
  set(HAS_STD_COROUTINE_HEADER 1)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    # TODO remove this check when cmake is able to detect it
    unset(HAS_STD_COROUTINE_HEADER)
    set(HAS_STD_EXPERIMENTAL_COROUTINES_HEADER 1)
else()
  check_include_file_cxx(coroutine HAS_STD_COROUTINE_HEADER)
  check_include_file_cxx(experimental/coroutine HAS_STD_EXPERIMENTAL_COROUTINES_HEADER)
endif()
message(STATUS "HAS_STD_COROUTINE_HEADER: ${HAS_STD_COROUTINE_HEADER}")
message(STATUS "HAS_STD_EXPERIMENTAL_COROUTINES_HEADER: ${HAS_STD_EXPERIMENTAL_COROUTINES_HEADER}")

if(HAS_STD_COROUTINE_HEADER)
  target_compile_options(${PROJECT_NAME} PUBLIC ${CMAKE_REQUIRED_FLAGS}
    -DHAS_STD_COROUTINE_HEADER=${HAS_STD_COROUTINE_HEADER}
  )
endif()
target_compile_definitions(${PROJECT_NAME} PUBLIC ${CMAKE_REQUIRED_DEFINITIONS})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_REQUIRED_INCLUDES})
add_library(cppcoro::coro ALIAS ${PROJECT_NAME})

option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
