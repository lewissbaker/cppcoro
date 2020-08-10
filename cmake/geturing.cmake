include(ExternalProject)

externalproject_add(_fetch_uring
  GIT_REPOSITORY https://github.com/axboe/liburing.git
  GIT_TAG master
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_INSTALL_PREFIX}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
  BUILD_IN_SOURCE ON
  )
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_INSTALL_PREFIX}/include)
add_library(liburing INTERFACE IMPORTED GLOBAL)
target_link_libraries(liburing INTERFACE ${CMAKE_INSTALL_PREFIX}/lib/liburing.a)
target_include_directories(liburing INTERFACE ${CMAKE_INSTALL_PREFIX}/include)
add_dependencies(liburing _fetch_uring)
add_library(uring::uring ALIAS liburing)
