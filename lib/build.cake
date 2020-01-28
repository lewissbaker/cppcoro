###############################################################################
# Copyright Lewis Baker
# Licenced under MIT license. See LICENSE.txt for details.
###############################################################################

import cake.path

from cake.tools import compiler, script, env, project, variant

includes = cake.path.join(env.expand('${CPPCORO}'), 'include', 'cppcoro', [
  'awaitable_traits.hpp',
  'is_awaitable.hpp',
  'async_auto_reset_event.hpp',
  'async_manual_reset_event.hpp',
  'async_generator.hpp',
  'async_mutex.hpp',
  'async_latch.hpp',
  'async_scope.hpp',
  'broken_promise.hpp',
  'cancellation_registration.hpp',
  'cancellation_source.hpp',
  'cancellation_token.hpp',
  'task.hpp',
  'sequence_barrier.hpp',
  'sequence_traits.hpp',
  'single_producer_sequencer.hpp',
  'multi_producer_sequencer.hpp',
  'shared_task.hpp',
  'single_consumer_event.hpp',
  'single_consumer_async_auto_reset_event.hpp',
  'sync_wait.hpp',
  'task.hpp',
  'io_service.hpp',
  'config.hpp',
  'on_scope_exit.hpp',
  'file_share_mode.hpp',
  'file_open_mode.hpp',
  'file_buffering_mode.hpp',
  'file.hpp',
  'fmap.hpp',
  'when_all.hpp',
  'when_all_ready.hpp',
  'resume_on.hpp',
  'schedule_on.hpp',
  'generator.hpp',
  'readable_file.hpp',
  'recursive_generator.hpp',
  'writable_file.hpp',
  'read_only_file.hpp',
  'write_only_file.hpp',
  'read_write_file.hpp',
  'file_read_operation.hpp',
  'file_write_operation.hpp',
  'static_thread_pool.hpp',
  ])

netIncludes = cake.path.join(env.expand('${CPPCORO}'), 'include', 'cppcoro', 'net', [
  'ip_address.hpp',
  'ip_endpoint.hpp',
  'ipv4_address.hpp',
  'ipv4_endpoint.hpp',
  'ipv6_address.hpp',
  'ipv6_endpoint.hpp',
  'socket.hpp',
])

detailIncludes = cake.path.join(env.expand('${CPPCORO}'), 'include', 'cppcoro', 'detail', [
  'void_value.hpp',
  'when_all_ready_awaitable.hpp',
  'when_all_counter.hpp',
  'when_all_task.hpp',
  'get_awaiter.hpp',
  'is_awaiter.hpp',
  'any.hpp',
  'sync_wait_task.hpp',
  'unwrap_reference.hpp',
  'lightweight_manual_reset_event.hpp',
  ])

privateHeaders = script.cwd([
  'cancellation_state.hpp',
  'socket_helpers.hpp',
  'auto_reset_event.hpp',
  'spin_wait.hpp',
  'spin_mutex.hpp',
  ])

sources = script.cwd([
  'async_auto_reset_event.cpp',
  'async_manual_reset_event.cpp',
  'async_mutex.cpp',
  'cancellation_state.cpp',
  'cancellation_token.cpp',
  'cancellation_source.cpp',
  'cancellation_registration.cpp',
  'lightweight_manual_reset_event.cpp',
  'ip_address.cpp',
  'ip_endpoint.cpp',
  'ipv4_address.cpp',
  'ipv4_endpoint.cpp',
  'ipv6_address.cpp',
  'ipv6_endpoint.cpp',
  'static_thread_pool.cpp',
  'auto_reset_event.cpp',
  'spin_wait.cpp',
  'spin_mutex.cpp',
  ])

extras = script.cwd([
  'build.cake',
  'use.cake',
  ])

if variant.platform == "windows":
  detailIncludes.extend(cake.path.join(env.expand('${CPPCORO}'), 'include', 'cppcoro', 'detail', [
    'win32.hpp',
    'win32_overlapped_operation.hpp',
    ]))
  netIncludes.extend(cake.path.join(env.expand('${CPPCORO}'), 'include', 'cppcoro', 'net', [
    'socket.hpp',
    'socket_accept_operation.hpp',
    'socket_connect_operation.hpp',
    'socket_disconnect_operation.hpp',
    'socket_recv_operation.hpp',
    'socket_recv_from_operation.hpp',
    'socket_send_operation.hpp',
    'socket_send_to_operation.hpp',
  ]))
  sources.extend(script.cwd([
    'win32.cpp',
    'io_service.cpp',
    'file.cpp',
    'readable_file.cpp',
    'writable_file.cpp',
    'read_only_file.cpp',
    'write_only_file.cpp',
    'read_write_file.cpp',
    'file_read_operation.cpp',
    'file_write_operation.cpp',
    'socket_helpers.cpp',
    'socket.cpp',
    'socket_accept_operation.cpp',
    'socket_connect_operation.cpp',
    'socket_disconnect_operation.cpp',
    'socket_send_operation.cpp',
    'socket_send_to_operation.cpp',
    'socket_recv_operation.cpp',
    'socket_recv_from_operation.cpp',
    ]))

buildDir = env.expand('${CPPCORO_BUILD}')

compiler.addIncludePath(env.expand('${CPPCORO}/include'))

objects = compiler.objects(
  targetDir=env.expand('${CPPCORO_BUILD}/obj'),
  sources=sources,
  )

lib = compiler.library(
  target=env.expand('${CPPCORO_LIB}/cppcoro'),
  sources=objects,
  )

vcproj = project.project(
  target=env.expand('${CPPCORO_PROJECT}/cppcoro'),
  items={
    'Include': {
      'Detail': detailIncludes,
      'Net': netIncludes,
      '': includes,
      },
    'Source': sources + privateHeaders,
    '': extras
  },
  output=lib,
  )

script.setResult(
  project=vcproj,
  library=lib,
  )
