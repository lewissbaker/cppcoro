cmake_minimum_required(VERSION 3.12)
project(cppcoro LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

add_subdirectory(lib)

enable_testing()
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
install(DIRECTORY include/cppcoro
	    DESTINATION include
	    COMPONENT Devel)
install(FILES cmake/FindCoroutines.cmake
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
