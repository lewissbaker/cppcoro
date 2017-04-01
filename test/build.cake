import cake.path

from cake.tools import script, env, compiler, project

script.include([
  env.expand('${CPPCORO}/lib/use.cake'),
])

sources = script.cwd([
  'main.cpp',
])

extras = script.cwd([
  'build.cake',
])

objects = compiler.objects(
  targetDir=env.expand('${CPPCORO_BUILD}/test'),
  sources=sources,
)

testExe = compiler.program(
  target=env.expand('${CPPCORO_BUILD}/test/run'),
  sources=objects,
)

vcproj = project.project(
  target=env.expand('${CPPCORO_PROJECT}/cppcoro_tests'),
  items={
    'Source': sources,
    '': extras,
  },
  output=testExe,
)

script.setResult(
  project=vcproj,
  test=testExe,
)
