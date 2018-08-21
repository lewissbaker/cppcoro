###############################################################################
# Copyright (c) Lewis Baker
# Licenced under MIT license. See LICENSE.txt for details.
###############################################################################

import cake.path

from cake.tools import script, env, compiler, project, variant, test

script.include([
  env.expand('${CPPCORO}/lib/use.cake'),
])

headers = script.cwd([
  "counted.hpp",
  "io_service_fixture.hpp",
  ])

sources = script.cwd([
  'main.cpp',
  'counted.cpp',
  'generator_tests.cpp',
  'recursive_generator_tests.cpp',
  'async_generator_tests.cpp',
  'async_auto_reset_event_tests.cpp',
  'async_manual_reset_event_tests.cpp',
  'async_mutex_tests.cpp',
  'async_latch_tests.cpp',
  'cancellation_token_tests.cpp',
  'task_tests.cpp',
  'shared_task_tests.cpp',
  'sync_wait_tests.cpp',
  'single_consumer_async_auto_reset_event_tests.cpp',
  'when_all_tests.cpp',
  'when_all_ready_tests.cpp',
  'ip_address_tests.cpp',
  'ip_endpoint_tests.cpp',
  'ipv4_address_tests.cpp',
  'ipv4_endpoint_tests.cpp',
  'ipv6_address_tests.cpp',
  'ipv6_endpoint_tests.cpp',
  'static_thread_pool_tests.cpp',
  ])

if variant.platform == 'windows':
  sources += script.cwd([
    'scheduling_operator_tests.cpp',
    'io_service_tests.cpp',
    'file_tests.cpp',
    'socket_tests.cpp',
    ])

extras = script.cwd([
  'build.cake',
])

intermediateBuildDir = cake.path.join(env.expand('${CPPCORO_BUILD}'), 'test', 'obj')

compiler.addDefine('CPPCORO_RELEASE_' + variant.release.upper())

objects = compiler.objects(
  targetDir=intermediateBuildDir,
  sources=sources,
)

testExe = compiler.program(
  target=env.expand('${CPPCORO_BUILD}/test/run'),
  sources=objects,
)

test.alwaysRun = True
testResult = test.run(
  program=testExe,
  results=env.expand('${CPPCORO_BUILD}/test/run.results'),
  )
script.addTarget('testresult', testResult)

vcproj = project.project(
  target=env.expand('${CPPCORO_PROJECT}/cppcoro_tests'),
  items={
    'Source': sources + headers,
    '': extras,
  },
  output=testExe,
)

script.setResult(
  project=vcproj,
  test=testExe,
)
