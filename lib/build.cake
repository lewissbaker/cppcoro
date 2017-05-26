###############################################################################
# Copyright Lewis Baker
# Licenced under MIT license. See LICENSE.txt for details.
###############################################################################

import cake.path

from cake.tools import compiler, script, env, project

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
  'on_scope_exit.hpp',
  ])

privateHeaders = script.cwd([
  'cancellation_state.hpp',
  ])

sources = script.cwd([
  'async_mutex.cpp',
  'cancellation_state.cpp',
  'cancellation_token.cpp',
  'cancellation_source.cpp',
  'cancellation_registration.cpp',
  ])

extras = script.cwd([
  'build.cake',
  'use.cake',
  ])

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
    'Include': includes,
    'Source': sources + privateHeaders,
    '': extras
  },
  output=lib,
  )

script.setResult(
  project=vcproj,
  library=lib,
  )
