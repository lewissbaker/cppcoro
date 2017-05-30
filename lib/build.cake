###############################################################################
# Copyright Lewis Baker
# Licenced under MIT license. See LICENSE.txt for details.
###############################################################################

import cake.path

from cake.tools import compiler, script, env, project, variant

includes = cake.path.join(env.expand('${CPPCORO}'), 'include', 'cppcoro', [
  'async_mutex.hpp',
  'broken_promise.hpp',
  'cancellation_registration.hpp',
  'cancellation_source.hpp',
  'cancellation_token.hpp',
  'lazy_task.hpp',
  'shared_lazy_task.hpp',
  'shared_task.hpp',
  'single_consumer_event.hpp',
  'task.hpp',
  'io_service.hpp',
  'config.hpp',
  'on_scope_exit.hpp',
  'file_share_mode.hpp',
  'file_open_mode.hpp',
  'file_buffering_mode.hpp',
  'file.hpp',
  'readable_file.hpp',
  'writable_file.hpp',
  'read_only_file.hpp',
  'write_only_file.hpp',
  'read_write_file.hpp',
  'file_read_operation.hpp',
  'file_write_operation.hpp',
  ])

detailIncludes = []

privateHeaders = script.cwd([
  'cancellation_state.hpp',
  ])

sources = script.cwd([
  'async_mutex.cpp',
  'cancellation_state.cpp',
  'cancellation_token.cpp',
  'cancellation_source.cpp',
  'cancellation_registration.cpp',
  'io_service.cpp',
  'file.cpp',
  'readable_file.cpp',
  'writable_file.cpp',
  'read_only_file.cpp',
  'write_only_file.cpp',
  'read_write_file.cpp',
  'file_read_operation.cpp',
  'file_write_operation.cpp',
  ])

extras = script.cwd([
  'build.cake',
  'use.cake',
  ])

if variant.platform == "windows":
  detailIncludes.extend(script.cwd([
    'win32.hpp',
    ]))
  sources.extend(script.cwd([
    'win32.cpp',
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