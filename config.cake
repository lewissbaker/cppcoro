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
      from cake.library.compilers.msvc import getVisualStudio2017Compiler, findMsvc2017InstallDir
      if nugetPath:
        vcInstallDir = cake.path.join(nugetPath, 'lib', 'native')
      else:
        vcInstallDir = str(findMsvc2017InstallDir(targetArchitecture=arch, allowPreRelease=True))
      compiler = getVisualStudio2017Compiler(
        configuration,
        targetArchitecture=arch,
        vcInstallDir=vcInstallDir,
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
      compiler.warningLevel = '4'
      compiler.warningsAsErrors = True

      # Enable experimental C++ coroutines via command-line flag.
      compiler.addCppFlag('/await')

      # Enable static analysis warnings (not as errors)
      compiler.addCppFlag('/analyze:WX-')
      compiler.addCppFlag('/analyze:max_paths')
      compiler.addCppFlag('512')

      # Enable C++17 features like std::optional<>
      compiler.addCppFlag('/std:c++latest')

      compiler.addProgramFlag('/nodefaultlib')
      compiler.addModuleFlag('/nodefaultlib')

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
      compiler.addLibrary('msvcrtd')
      compiler.addLibrary('msvcprtd')
      compiler.addLibrary('msvcurtd')
      compiler.addLibrary('ucrtd')
      compiler.addLibrary('oldnames')

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
      compiler.addLibrary('msvcrt')
      compiler.addLibrary('msvcprt')
      compiler.addLibrary('msvcurt')
      compiler.addLibrary('ucrt')
      compiler.addLibrary('oldnames')

      # Enable compiler to optimise-out heap allocations for coroutine frames
      # Only supported on X64
      if arch == 'x64':
        compiler.addCppFlag('/await:heapelide')

      compiler.addDefine('NDEBUG')

      project = msvcOptVariant.tools["project"]
      project.projectConfigName = "Windows (" + arch + ") Msvc (Optimised)"
      project.solutionConfigName = "Msvc Optimised"
      configuration.addVariant(msvcOptVariant)

    except CompilerNotFoundError, e:
      print str(e)

elif cake.system.isLinux():

  from cake.library.compilers.clang import ClangCompiler

  clangVariant = baseVariant.clone(compiler='clang',
                                   platform='linux',
                                   architecture='x64')

  # If you have built your own version of Clang, you can modify
  # this variable to point to the CMAKE_INSTALL_PREFIX for
  # where you have installed your clang/libcxx build.
  clangInstallPrefix = '/usr'

  # Set this to the install-prefix of where libc++ is installed.
  # You only need to set this if it is not installed at the same
  # location as clangInstallPrefix.
  libCxxInstallPrefix = None # '/path/to/install'

  clangBinPath = cake.path.join(clangInstallPrefix, 'bin')

  compiler = ClangCompiler(
    configuration=configuration,
    clangExe=cake.path.join(clangBinPath, 'clang'),
    llvmArExe=cake.path.join(clangBinPath, 'llvm-ar'),
    binPaths=[clangBinPath])

  compiler.addCppFlag('-std=c++1z')
  compiler.addCppFlag('-fcoroutines-ts')
  compiler.addCppFlag('-m64')

  compiler.addModuleFlag('-fuse-ld=lld')
  compiler.addProgramFlag('-fuse-ld=lld')
  
  if libCxxInstallPrefix:
    compiler.addCppFlag('-nostdinc++')
    compiler.addIncludePath(cake.path.join(
      libCxxInstallPrefix, 'include', 'c++', 'v1'))
    compiler.addLibraryPath(cake.path.join(
      libCxxInstallPrefix, 'lib'))
  else:
    compiler.addCppFlag('-stdlib=libc++')

  compiler.addLibrary('c++')
  compiler.addLibrary('c++abi')
  compiler.addLibrary('c')
  compiler.addLibrary('pthread')

  #compiler.addProgramFlag('-Wl,--trace')
  #compiler.addProgramFlag('-Wl,-v')

  clangVariant.tools['compiler'] = compiler

  env = clangVariant.tools["env"]
  env["COMPILER"] = "clang"
  env["COMPILER_VERSION"] = "5.0"
  env["PLATFORM"] = "linux"
  env["ARCHITECTURE"] = "x64"

  clangDebugVariant = clangVariant.clone(release='debug')
  clangDebugVariant.tools["env"]["RELEASE"] = 'debug'

  # Configure debug-specific settings here
  compiler = clangDebugVariant.tools["compiler"]
  compiler.addCppFlag('-O0')
  compiler.addCppFlag('-g')

  configuration.addVariant(clangDebugVariant)

  clangOptimisedVariant = clangVariant.clone(release='optimised')
  clangOptimisedVariant.tools["env"]["RELEASE"] = 'optimised'

  # Configure optimised-specific settings here
  compiler = clangOptimisedVariant.tools["compiler"]
  compiler.addCppFlag('-O3')
  compiler.addCppFlag('-g')
  compiler.addCppFlag('-flto')
  compiler.addProgramFlag('-flto')
  compiler.addModuleFlag('-flto')

  configuration.addVariant(clangOptimisedVariant)
