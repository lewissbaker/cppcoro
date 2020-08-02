add_executable(read_file
        read_file.cpp
        )
target_link_libraries(read_file PUBLIC cppcoro::coro)

add_executable(echo_udp
        echo_udp.cpp
        )
target_link_libraries(echo_udp PUBLIC cppcoro::coro)
