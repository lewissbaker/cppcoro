from cake.tools import script, project, env

libScript = script.get(script.cwd('lib/build.cake'))
script.addTarget('objects', libScript.getTarget('objects'))
script.addTarget('libs', libScript.getTarget('libs'))
script.addDefaultTarget(libScript.getDefaultTarget())
libScript.execute()

testScript = script.get(script.cwd('test/build.cake'))
script.addTarget('objects', testScript.getTarget('objects'))
script.addDefaultTarget(testScript.getDefaultTarget())
testScript.execute()

projects = [
  libScript.getResult('project'),
  testScript.getResult('project'),
]


sln = project.solution(
  target=env.expand('${CPPCORO_PROJECT}/cppcoro'),
  projects=projects,
)

script.addTargets('projects', projects)
script.addTarget('projects', sln)

script.setResult(
  projects=projects,
)
