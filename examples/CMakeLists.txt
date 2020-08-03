link_libraries(cppcoro::coro)

add_executable(read_file
        read_file.cpp
        )

add_executable(hello_udp
        hello_udp.cpp
        )

add_executable(hello_tcp
        hello_tcp.cpp
        )
