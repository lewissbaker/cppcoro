###############################################################################
# Copyright (c) Lewis Baker
# Licenced under MIT license. See LICENSE.txt for details.
###############################################################################

import cake.path

from cake.tools import script, env, compiler, variant

compiler.addIncludePath(env.expand('${CPPCORO}/include'))

buildScript = script.get(script.cwd('build.cake'))
compiler.addLibrary(buildScript.getResult('library'))

if variant.platform == "windows":
  compiler.addLibrary("kernel32")

