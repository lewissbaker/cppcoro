cmake_minimum_required(VERSION 3.16.3)

### Compiler(s): only clang for linux. Other compilers
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  set(CMAKE_CXX_COMPILER clang++)
  add_compile_options(-stdlib=libc++)
  add_compile_options(-Werror -Wall -Wextra -Wpedantic)
endif()

project(cppcoro VERSION 1.0.0 LANGUAGES CXX)

### Configuration
set(API_VERSION "1.0" CACHE STRING "api version")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)

### Packages
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)
find_package(PkgConfig MODULE REQUIRED)

include(CTest)
include(GNUInstallDirs)

add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(test)

# installation stuff, mostly boilerpalte:

install(TARGETS
    ${PROJECT_NAME} ${PROJECT_NAME}_api
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)

install(EXPORT ${PROJECT_NAME}Targets
  NAMESPACE coroutines::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  ${PROJECT_NAME}ConfigVersion.cmake
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(Config.cmake.in
  ${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

install(FILES
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
