include(ExternalProject)

externalproject_add(_fetch_uring
  GIT_REPOSITORY https://github.com/axboe/liburing.git
  GIT_TAG master
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}
  BUILD_COMMAND make -j4
  BUILD_IN_SOURCE ON
  ALWAYS OFF
  UPDATE_DISCONNECTED ON
  )
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include)
add_library(liburing INTERFACE IMPORTED GLOBAL)
target_link_libraries(liburing INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/lib/liburing.a)
target_include_directories(liburing INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include)
add_dependencies(liburing _fetch_uring)
add_library(uring::uring ALIAS liburing)
