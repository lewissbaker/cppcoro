cmake_minimum_required(VERSION 3.8)
project(cppcoro VERSION 1.0.0 LANGUAGES CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # TODO: check the version
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  # require at least clang 5.0
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
    message(FATAL_ERROR "Clang version must be at least 5.0!")
  endif()
else()
  message(WARNING "You are using an unsupported compiler!")
endif()

option(CPPCORO_IO_ENABLED "Enable cppcoro async I/O feature." FALSE)
option(CPPCORO_TEST_ENABLED "Build the cppcoro tests." TRUE)

configure_file (
  "include/cppcoro/config_options.hpp.in"
  "${CMAKE_CURRENT_LIST_DIR}/include/cppcoro/config_options.hpp"
  )

set(cppcoro_includes
  include/cppcoro/async_auto_reset_event.hpp
  include/cppcoro/async_manual_reset_event.hpp
  include/cppcoro/async_generator.hpp
  include/cppcoro/async_mutex.hpp
  include/cppcoro/async_latch.hpp
  include/cppcoro/broken_promise.hpp
  include/cppcoro/cancellation_registration.hpp
  include/cppcoro/cancellation_source.hpp
  include/cppcoro/cancellation_token.hpp
  include/cppcoro/task.hpp
  include/cppcoro/shared_task.hpp
  include/cppcoro/shared_task.hpp
  include/cppcoro/single_consumer_event.hpp
  include/cppcoro/single_consumer_async_auto_reset_event.hpp
  include/cppcoro/sync_wait.hpp
  include/cppcoro/task.hpp
  include/cppcoro/config.hpp
  include/cppcoro/on_scope_exit.hpp
  include/cppcoro/fmap.hpp
  include/cppcoro/when_all.hpp
  include/cppcoro/when_all_ready.hpp
  include/cppcoro/resume_on.hpp
  include/cppcoro/schedule_on.hpp
  include/cppcoro/generator.hpp
  include/cppcoro/recursive_generator.hpp

  # detailIncludes
  include/cppcoro/detail/continuation.hpp
  include/cppcoro/detail/when_all_awaitable.hpp
  include/cppcoro/detail/unwrap_reference.hpp
  include/cppcoro/detail/lightweight_manual_reset_event.hpp

  # privateHeaders
  lib/cancellation_state.hpp

  )

set(cppcoro_ioIncludes
  include/cppcoro/io_service.hpp
  include/cppcoro/file_share_mode.hpp
  include/cppcoro/file_open_mode.hpp
  include/cppcoro/file_buffering_mode.hpp
  include/cppcoro/file.hpp
  include/cppcoro/readable_file.hpp
  include/cppcoro/writable_file.hpp
  include/cppcoro/read_only_file.hpp
  include/cppcoro/write_only_file.hpp
  include/cppcoro/read_write_file.hpp
  include/cppcoro/file_read_operation.hpp
  include/cppcoro/file_write_operation.hpp

  include/cppcoro/net/ip_address.hpp
  include/cppcoro/net/ip_endpoint.hpp
  include/cppcoro/net/ipv4_address.hpp
  include/cppcoro/net/ipv4_endpoint.hpp
  include/cppcoro/net/ipv6_address.hpp
  include/cppcoro/net/ipv6_endpoint.hpp
  )

set(cppcoro_sources
  lib/async_auto_reset_event.cpp
  lib/async_manual_reset_event.cpp
  lib/async_mutex.cpp
  lib/cancellation_state.cpp
  lib/cancellation_token.cpp
  lib/cancellation_source.cpp
  lib/cancellation_registration.cpp
  lib/lightweight_manual_reset_event.cpp
  )

set(cppcoro_ioSources
  lib/ip_address.cpp
  lib/ip_endpoint.cpp
  lib/ipv4_address.cpp
  lib/ipv4_endpoint.cpp
  lib/ipv6_address.cpp
  lib/ipv6_endpoint.cpp
  )

if(WIN32)
set(cppcoro_includes ${cppcoro_includes}
  include/cppcoro/detail/win32.hpp
  include/cppcoro/detail/win32_overlapped_operation.hpp
  )
  
set(cppcoro_sources ${cppcoro_sources}
  lib/win32.cpp
  )

set(cppcoro_ioSources ${cppcoro_ioSources}
  lib/io_service.cpp
  lib/file.cpp
  lib/readable_file.cpp
  lib/writable_file.cpp
  lib/read_only_file.cpp
  lib/write_only_file.cpp
  lib/read_write_file.cpp
  lib/file_read_operation.cpp
  lib/file_write_operation.cpp
  )

endif() #WIN32

add_library(cppcoro ${cppcoro_sources} ${cppcoro_includes})
add_library(cppcoro::cppcoro ALIAS cppcoro)

if(CPPCORO_IO_ENABLED)
  target_sources(cppcoro PRIVATE ${cppcoro_ioSources} ${cppcoro_ioIncludes})
endif()

target_compile_features(cppcoro PUBLIC cxx_std_17)

target_include_directories(cppcoro PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(cppcoro PUBLIC /await)
  target_compile_definitions(cppcoro PUBLIC _SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(cppcoro PUBLIC -stdlib=libc++ -fcoroutines-ts)
  target_link_libraries(cppcoro c++ c++abi c++experimental)
endif()

set_target_properties(cppcoro PROPERTIES DEBUG_POSTFIX d)

##############################################
# Installation instructions

include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
install(TARGETS cppcoro
  EXPORT ${PROJECT_NAME}-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

#Export the targets to a script
install(EXPORT ${PROJECT_NAME}-targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${INSTALL_CONFIGDIR}
  )

#Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
  )

configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
  )

#Install the config, configversion
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  DESTINATION ${INSTALL_CONFIGDIR}
  )


##############################################
## Exporting from the build tree

export(EXPORT ${PROJECT_NAME}-targets FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake NAMESPACE ${PROJECT_NAME}::)

#Register package in user's package registry
export(PACKAGE ${PROJECT_NAME})

##############################################
## Tests

if(CPPCORO_TEST_ENABLED)

  set(cppcoro_test_headers
    test/counted.hpp
    test/io_service_fixture.hpp
    )

  set(cppcoro_test_sources
    test/main.cpp
    test/counted.cpp
    test/generator_tests.cpp
    test/recursive_generator_tests.cpp
    test/async_generator_tests.cpp
    test/async_auto_reset_event_tests.cpp
    test/async_manual_reset_event_tests.cpp
    test/async_mutex_tests.cpp
    test/async_latch_tests.cpp
    test/cancellation_token_tests.cpp
    test/task_tests.cpp
    test/shared_task_tests.cpp
    test/sync_wait_tests.cpp
    test/single_consumer_async_auto_reset_event_tests.cpp
    test/when_all_tests.cpp
    test/when_all_ready_tests.cpp
    )

  if(CPPCORO_IO_ENABLED)
    set(cppcoro_test_sources ${cppcoro_test_sources}
      test/ip_address_tests.cpp
      test/ip_endpoint_tests.cpp
      test/ipv4_address_tests.cpp
      test/ipv4_endpoint_tests.cpp
      test/ipv6_address_tests.cpp
      test/ipv6_endpoint_tests.cpp
      
      test/scheduling_operator_tests.cpp
      test/io_service_tests.cpp
      test/file_tests.cpp
      )
  endif() # WIN32

  enable_testing()

  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  
  add_executable(cppcoro_tests ${cppcoro_test_sources} ${cppcoro_test_headers})
  target_link_libraries(cppcoro_tests cppcoro::cppcoro Threads::Threads)
  
  add_test(NAME cppcoro_tests COMMAND cppcoro_tests)  
  
endif() # CPPCORO_TEST_ENABLED