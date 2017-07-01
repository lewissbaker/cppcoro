##############################################################################
# Copyright (c) Lewis Baker
# Licenced under MIT license. See LICENSE.txt for details.
#
# cppcoro library
#
# This file defines the set of build variants available to be built and
# their corresponding tools and settings.
#
##############################################################################

import os
import sys

import cake.path
import cake.system

from cake.engine import Variant
from cake.script import Script

configuration = Script.getCurrent().configuration
engine = configuration.engine

# Add path containing custom cake extensions to Python's import path.
toolsDir = configuration.abspath(cake.path.join('tools', 'cake_extensions'))
sys.path.insert(0, toolsDir)

hostPlatform = cake.system.platform().lower()
hostArchitecture = cake.system.architecture().lower()

baseVariant = Variant()

from cake.library.script import ScriptTool
from cake.library.variant import VariantTool
from cake.library.env import EnvironmentTool
from cake.library.project import ProjectTool
from cake.library.compilers import CompilerNotFoundError
from testtool import UnitTestTool

baseVariant.tools["script"] = script = ScriptTool(configuration=configuration)
baseVariant.tools["variant"] = variant = VariantTool(configuration=configuration)
baseVariant.tools["test"] = test = UnitTestTool(configuration=configuration)

baseVariant.tools["env"] = env = EnvironmentTool(configuration=configuration)

env["VARIANT"] = "${PLATFORM}_${ARCHITECTURE}_${COMPILER}${COMPILER_VERSION}_${RELEASE}"
env["BUILD"] = "build/${VARIANT}"

env["CPPCORO"] = "."
env["CPPCORO_BUILD"] = "${BUILD}"
env["CPPCORO_PROJECT"] = "build/project"
env["CPPCORO_BIN"] = "${CPPCORO_BUILD}/bin"
env["CPPCORO_LIB"] = "${CPPCORO_BUILD}/lib"

baseVariant.tools["project"] = project = ProjectTool(configuration=configuration)
project.product = project.VS2015
project.enabled = engine.options.createProjects
if project.enabled:
  test.enabled = False
  engine.addBuildSuccessCallback(project.build)

if cake.system.isWindows() or cake.system.isCygwin():
  # Uncomment the next line to use an experimental MSVC compiler version
  # downloaded from the https://vcppdogfooding.azurewebsites.net/ NuGet repository.
  # Unzip the .nuget file to a folder and specify the path here.
  nugetPath = None #r'C:\Path\To\VisualCppTools.Community.VS2017Layout.14.11.25415-Pre'

  for arch in ("x64", "x86"):
    try:
      from cake.library.compilers.msvc import getVisualStudio2017Compiler
      compiler = getVisualStudio2017Compiler(
        configuration,
        targetArchitecture=arch,
        vcInstallDir=cake.path.join(nugetPath, 'lib', 'native') if nugetPath else None,
        )

      msvcVariant = baseVariant.clone(
        platform="windows",
        compiler="msvc",
        architecture=arch,
        )

      msvcVariant.tools["compiler"] = compiler

      project = msvcVariant.tools["project"]
      project.solutionPlatformName = "Windows " + arch
      if arch == "x86":
        project.projectPlatformName = "Win32"
      elif arch == "x64":
        project.projectPlatformName = "x64"

      if engine.options.createProjects:
        compiler.enabled = False

      compiler.enableRtti = True
      compiler.enableExceptions = True
      compiler.outputMapFile = True
      compiler.outputFullPath = True
      compiler.messageStyle = compiler.MSVS_CLICKABLE
      compiler.warningLevel = '3'
      compiler.warningsAsErrors = True

      # Enable experimental C++ coroutines via command-line flag.
      compiler.addCppFlag('/await')

      # Enable C++17 features like std::optional<>
      compiler.addCppFlag('/std:c++latest')

      env = msvcVariant.tools["env"]
      env["COMPILER"] = "msvc"
      env["COMPILER_VERSION"] = "15"
      env["PLATFORM"] = "windows"
      env["ARCHITECTURE"] = arch

      # Visual Studio - Debug
      msvcDebugVariant = msvcVariant.clone(release="debug")
      msvcDebugVariant.tools["env"]["RELEASE"] = "debug"
      compiler = msvcDebugVariant.tools["compiler"]
      compiler.debugSymbols = True
      compiler.useIncrementalLinking = True
      compiler.optimisation = compiler.NO_OPTIMISATION
      compiler.runtimeLibraries = 'debug-dll'
      project = msvcDebugVariant.tools["project"]
      project.projectConfigName = "Windows (" + arch + ") Msvc (Debug)"
      project.solutionConfigName = "Msvc Debug"
      configuration.addVariant(msvcDebugVariant)

      # Visual Studio - Optimised
      msvcOptVariant = msvcVariant.clone(release="optimised")
      msvcOptVariant.tools["env"]["RELEASE"] = "optimised"
      compiler = msvcOptVariant.tools["compiler"]
      compiler.debugSymbols = True
      compiler.useIncrementalLinking = False
      compiler.useFunctionLevelLinking = True
      compiler.optimisation = compiler.FULL_OPTIMISATION
      compiler.runtimeLibraries = 'release-dll'
      compiler.addDefine('NDEBUG')
      project = msvcOptVariant.tools["project"]
      project.projectConfigName = "Windows (" + arch + ") Msvc (Optimised)"
      project.solutionConfigName = "Msvc Optimised"
      configuration.addVariant(msvcOptVariant)

    except CompilerNotFoundError, e:
      print str(e)
