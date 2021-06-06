add_library(${PROJECT_NAME}
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

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME} PRIVATE
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
      socket_recv_from_operation.cpp
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  ${PROJECT_NAME}_api
)

set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${API_VERSION})

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
    VERSION="${PROJECT_VERSION}"
)