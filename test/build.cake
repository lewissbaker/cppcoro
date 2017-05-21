###############################################################################
# Copyright (c) Lewis Baker
# Licenced under MIT license. See LICENSE.txt for details.
###############################################################################

import cake.path

from cake.tools import script, env, compiler, project, variant

script.include([
  env.expand('${CPPCORO}/lib/use.cake'),
])

headers = script.cwd([
  "counted.hpp",
  ])

sources = script.cwd([
  'main.cpp',
  'counted.cpp',
  'async_generator_tests.cpp',
  'async_mutex_tests.cpp',
  'cancellation_token_tests.cpp',
  'lazy_task_tests.cpp',
  'shared_lazy_task_tests.cpp',
  'shared_task_tests.cpp',
  'task_tests.cpp',
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
