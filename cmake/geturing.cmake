find_package(liburing)
if(NOT liburing_FOUND)
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
    add_library(_liburing INTERFACE IMPORTED GLOBAL)
    target_link_libraries(_liburing INTERFACE ${LIBURING_TMP_INSTALL_DIR}/lib/liburing.a)
    target_include_directories(_liburing INTERFACE ${LIBURING_TMP_INSTALL_DIR}/include)
    add_dependencies(_liburing _fetch_uring)
    add_library(liburing::liburing ALIAS _liburing)

    install(FILES ${LIBURING_TMP_INSTALL_DIR}/lib/liburing.a DESTINATION lib)
    install(DIRECTORY ${LIBURING_TMP_INSTALL_DIR}/include/ DESTINATION include)
endif()