cmake_minimum_required(VERSION 3.13)
project(cppcoro LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CTest)

add_subdirectory(lib)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()

export(EXPORT cppcoroTargets
  FILE "${PROJECT_BINARY_DIR}/cppcoro/cppcoroTargets.cmake"
  NAMESPACE cppcoro::)
configure_file(cmake/cppcoroConfig.cmake
    "${PROJECT_BINARY_DIR}/cppcoro/cppcoroConfig.cmake"
    COPYONLY)

set(config_package_location lib/cmake/cppcoro)
install(DIRECTORY include
	    DESTINATION .
	    COMPONENT Devel)
install(FILES cmake/FindCppcoroCoroutines.cmake
	DESTINATION ${config_package_location}
	COMPONENT Devel)
install(EXPORT cppcoroTargets
  FILE cppcoroTargets.cmake
  NAMESPACE cppcoro::
  DESTINATION ${config_package_location})
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/cppcoro/cppcoroConfig.cmake
  DESTINATION ${config_package_location}
  COMPONENT Devel)
