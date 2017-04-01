##############################################################################
# cppcoro library
#
# This file defines extra command-line args specific to the cppcoro project.
##############################################################################
from cake.script import Script

parser = Script.getCurrent().engine.parser

# Add a project generation option. It will be stored in 'engine.options' which
# can later be accessed in our config.cake.
parser.add_option(
  "-p", "--projects",
  action="store_true",
  dest="createProjects",
  help="Create projects instead of building a variant.",
  default=False,
  )
