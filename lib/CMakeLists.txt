set(includes
	awaitable_traits.hpp
	is_awaitable.hpp
	async_auto_reset_event.hpp
	async_manual_reset_event.hpp
	async_generator.hpp
	async_mutex.hpp
	async_latch.hpp
	async_scope.hpp
	broken_promise.hpp
	cancellation_registration.hpp
	cancellation_source.hpp
	cancellation_token.hpp
	task.hpp
	sequence_barrier.hpp
	sequence_traits.hpp
	single_producer_sequencer.hpp
	multi_producer_sequencer.hpp
	shared_task.hpp
	shared_task.hpp
	single_consumer_event.hpp
	single_consumer_async_auto_reset_event.hpp
	sync_wait.hpp
	task.hpp
	io_service.hpp
	config.hpp
	on_scope_exit.hpp
	file_share_mode.hpp
	file_open_mode.hpp
	file_buffering_mode.hpp
	file.hpp
	fmap.hpp
	when_all.hpp
	when_all_ready.hpp
	resume_on.hpp
	schedule_on.hpp
	generator.hpp
	readable_file.hpp
	recursive_generator.hpp
	writable_file.hpp
	read_only_file.hpp
	write_only_file.hpp
	read_write_file.hpp
	file_read_operation.hpp
	file_write_operation.hpp
	static_thread_pool.hpp
)
list(TRANSFORM includes PREPEND "${PROJECT_SOURCE_DIR}/include/cppcoro/")

set(netIncludes
	ip_address.hpp
	ip_endpoint.hpp
	ipv4_address.hpp
	ipv4_endpoint.hpp
	ipv6_address.hpp
	ipv6_endpoint.hpp
	socket.hpp
)
list(TRANSFORM netIncludes PREPEND "${PROJECT_SOURCE_DIR}/include/cppcoro/net/")

set(detailIncludes
	void_value.hpp
	when_all_ready_awaitable.hpp
	when_all_counter.hpp
	when_all_task.hpp
	get_awaiter.hpp
	is_awaiter.hpp
	any.hpp
	sync_wait_task.hpp
	unwrap_reference.hpp
	lightweight_manual_reset_event.hpp
)
list(TRANSFORM detailIncludes PREPEND "${PROJECT_SOURCE_DIR}/include/cppcoro/detail/")

set(privateHeaders
	cancellation_state.hpp
	socket_helpers.hpp
	auto_reset_event.hpp
	spin_wait.hpp
	spin_mutex.hpp
)

set(sources
	async_auto_reset_event.cpp
	async_manual_reset_event.cpp
	async_mutex.cpp
	cancellation_state.cpp
	cancellation_token.cpp
	cancellation_source.cpp
	cancellation_registration.cpp
	lightweight_manual_reset_event.cpp
	ip_address.cpp
	ip_endpoint.cpp
	ipv4_address.cpp
	ipv4_endpoint.cpp
	ipv6_address.cpp
	ipv6_endpoint.cpp
	static_thread_pool.cpp
	auto_reset_event.cpp
	spin_wait.cpp
	spin_mutex.cpp
)

if(WIN32)
	set(win32DetailIncludes
		win32.hpp
		win32_overlapped_operation.hpp
	)
    list(TRANSFORM win32DetailIncludes PREPEND "${PROJECT_SOURCE_DIR}/include/cppcoro/detail/")
    list(APPEND detailIncludes ${win32DetailIncludes})

    set(win32NetIncludes
        socket.hpp
        socket_accept_operation.hpp
        socket_connect_operation.hpp
        socket_disconnect_operation.hpp
        socket_recv_operation.hpp
        socket_recv_from_operation.hpp
        socket_send_operation.hpp
        socket_send_to_operation.hpp
    )
    list(TRANSFORM win32NetIncludes PREPEND "${PROJECT_SOURCE_DIR}/include/cppcoro/net/")
    list(APPEND netIncludes ${win32NetIncludes})

    set(win32Sources
        win32.cpp
        io_service.cpp
        file.cpp
        readable_file.cpp
        writable_file.cpp
        read_only_file.cpp
        write_only_file.cpp
        read_write_file.cpp
        file_read_operation.cpp
        file_write_operation.cpp
        socket_helpers.cpp
        socket.cpp
        socket_accept_operation.cpp
        socket_connect_operation.cpp
        socket_disconnect_operation.cpp
        socket_send_operation.cpp
        socket_send_to_operation.cpp
        socket_recv_operation.cpp
        socket_recv_from_operation.cpp
    )
    list(APPEND sources ${win32Sources})
endif()

add_library(cppcoro
	${includes}
	${netIncludes}
	${detailIncludes}
	${privateHeaders}
	${sources}
)

target_include_directories(cppcoro PUBLIC
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)
target_compile_features(cppcoro PUBLIC cxx_std_20)

find_package(Coroutines COMPONENTS Experimental Final REQUIRED)
target_link_libraries(cppcoro PUBLIC std::coroutines)

install(TARGETS cppcoro EXPORT cppcoroTargets
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin)
