import cake.path

from cake.tools import script, env, compiler

compiler.addIncludePath(env.expand('${CPPCORO}/include'))

buildScript = script.get(script.cwd('build.cake'))
compiler.addLibrary(buildScript.getResult('library'))
