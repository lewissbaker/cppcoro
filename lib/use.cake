###############################################################################
# Copyright (c) Lewis Baker
# Licenced under MIT license. See LICENSE.txt for details.
###############################################################################

import cake.path

from cake.tools import script, env, compiler

compiler.addIncludePath(env.expand('${CPPCORO}/include'))

# TODO: Uncomment these once we actually have some source
# files to compile for the cppcoro library.
#buildScript = script.get(script.cwd('build.cake'))
#compiler.addLibrary(buildScript.getResult('library'))
