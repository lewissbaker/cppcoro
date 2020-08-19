find_path(LIBURING_INCLUDE_DIR liburing.h
        PATH_SUFFIXES liburing)

find_library(LIBURING_LIBRARY NAMES uring)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(liburing DEFAULT_MSG
        LIBURING_LIBRARY LIBURING_INCLUDE_DIR)

mark_as_advanced(LIBURING_LIBRARY LIBURING_INCLUDE_DIR)

add_library(liburing::liburing INTERFACE IMPORTED GLOBAL)
target_link_libraries(liburing::liburing INTERFACE ${LIBURING_LIBRARY})
target_include_directories(liburing::liburing INTERFACE ${LIBURING_INCLUDE_DIR})
