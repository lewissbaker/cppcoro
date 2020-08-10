include(ExternalProject)

set(LIBURING_TMP_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/liburing)
externalproject_add(_fetch_uring
  GIT_REPOSITORY https://github.com/axboe/liburing.git
  GIT_TAG master
  CONFIGURE_COMMAND ./configure --prefix=${LIBURING_TMP_INSTALL_DIR}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
  BUILD_IN_SOURCE ON
  )
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${LIBURING_TMP_INSTALL_DIR}/include)
add_library(liburing INTERFACE IMPORTED GLOBAL)
target_link_libraries(liburing INTERFACE ${LIBURING_TMP_INSTALL_DIR}/lib/liburing.a)
target_include_directories(liburing INTERFACE ${LIBURING_TMP_INSTALL_DIR}/include)
add_dependencies(liburing _fetch_uring)
add_library(uring::uring ALIAS liburing)

install(FILES ${LIBURING_TMP_INSTALL_DIR}/lib/liburing.a DESTINATION lib)
install(DIRECTORY ${LIBURING_TMP_INSTALL_DIR}/include/ DESTINATION include)
