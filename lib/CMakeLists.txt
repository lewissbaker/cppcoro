file(GLOB_RECURSE headers ../include/cppcoro/*.hpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../include/cppcoro PREFIX cppcoro FILES ${headers})

set(sources
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
  ipv6_endpoint.cpp)

if(WIN32)
  set(win32_sources
    win32.cpp
    io_service.cpp
    file.cpp
    readable_file.cpp
    writable_file.cpp
    read_only_file.cpp
    write_only_file.cpp
    read_write_file.cpp
    file_read_operation.cpp
    file_write_operation.cpp)
  list(APPEND sources ${win32_sources})
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX lib FILES ${sources})

add_library(cppcoro STATIC ${headers} ${sources})
set_target_properties(cppcoro PROPERTIES CXX_EXTENSIONS OFF)
target_compile_features(cppcoro PUBLIC cxx_std_17)

target_include_directories(cppcoro PRIVATE lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:include>)

if(WIN32)
  target_link_libraries(cppcoro PUBLIC ws2_32 mswsock synchronization)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(cppcoro PUBLIC Threads::Threads)
endif()

install(TARGETS cppcoro EXPORT cppcoro LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
