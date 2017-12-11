##############################################################################
# cppcoro library
#
# This file defines extra command-line args specific to the cppcoro project.
##############################################################################
from cake.script import Script
import cake.system

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

if cake.system.isLinux() or cake.system.isDarwin():
  parser.add_option(
      None, "--clang-install-prefix",
      action="store_value",
      dest="clangInstallPrefix",
      type="string",
      metavar="PATH",
      default=None,
      help="Path where clang has been installed."
      )

  parser.add_option(
    None, "--clang-executable",
    action="store_value",
    dest="clangExecutable",
    type="string",
    metavar="FILE",
    default="clang",
    help="Name or full-path of clang executable to compile with")

  parser.add_option(
    None, "--libcxx-install-prefix",
    action="store_value",
    dest="libcxxInstallPrefix",
    type="string",
    metavar"PATH",
    default=None,
    help="Path where libc++ has been installed.\n"
         "Defaults to value of --clang-install-prefix")
