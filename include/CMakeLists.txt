add_library(${PROJECT_NAME}_api INTERFACE
)

# this is interface only library
target_include_directories(${PROJECT_NAME}_api INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

include(FindPkgConfig)

if(PKG_CONFIG_FOUND)
    configure_file(cppcoro.pc.in ${PROJECT_BINARY_DIR}/cppcoro.pc @ONLY)
    install(FILES ${PROJECT_BINARY_DIR}/cppcoro.pc DESTINATION lib/pkgconfig)
endif()

# the proper way to use library is to make it visible for others i.e. library is the only responsible to install itself
install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/cppcoro
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)