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

      engine.logger.outputInfo("MSVC (" + arch + "): " + vcInstallDir + "\n")

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

      compiler.addDefine('_SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING')

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
      compiler.addLibrary('vcruntimed')
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
      compiler.addLibrary('vcruntime')
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

elif cake.system.isLinux() or cake.system.isDarwin():

  def firstExisting(paths, default=None):
    for p in paths:
      if os.path.exists(p):
        return p
    else:
      return default

      
      
  if cake.system.isLinux():
    defaultInstallPaths = ["/usr"]
  elif cake.system.isDarwin():
    defaultInstallPaths = ["/usr/local/opt/llvm", "/usr"]
      
  clangInstallPrefix = engine.options.clangInstallPrefix
  clangExe = engine.options.clangExecutable
  
  if clangInstallPrefix is None:
    # If no clang install prefix was specified then try to deduce it
    # from the --clang-executable path.
    if os.path.isabs(clangExe):
      clangBinPath = cake.path.dirName(clangExe)
      if cake.path.baseName(clangBinPath) == 'bin':
        clangInstallPrefix = cake.path.dirName(clangBinPath)

    # Use sensible system defaults if neither --clang-executable or
    # --clang-install-prefix command-line arguments are specified.
    if clangInstallPrefix is None:
      clangInstallPrefix = firstExisting(defaultInstallPaths, default="/usr")

  libcxxInstallPrefix = engine.options.libcxxInstallPrefix
  if libcxxInstallPrefix is None:
    libcxxInstallPrefix = clangInstallPrefix

  # If the user didn't specify an absolute path to clang then look for it in
  # the installed clang bin directory.
  if not os.path.isabs(clangExe):
    clangExe = cake.path.join(clangInstallPrefix, 'bin', clangExe)

  # Extract the clang version suffix from the file-name
  # We'll try and use other LLVM tools with the same suffix if they exist.
  clangName = cake.path.baseName(clangExe)
  versionSuffix = clangName[5:] if clangName.startswith('clang-') else ""

  clangExeDir = cake.path.dirName(clangExe)
  clangBinPath = cake.path.join(clangInstallPrefix, 'bin')

  binPaths = [clangExeDir]
  if clangBinPath not in binPaths:
    binPaths.append(clangBinPath)

  llvmArExe = firstExisting(
    cake.path.join(binPaths, ['llvm-ar' + versionSuffix, 'llvm-ar', 'ar']),
    default="/usr/bin/ar")

  # Prefer lld if available, otherwise fall back to system ld
  lldExe = firstExisting(
    cake.path.join(binPaths, ['ld.lld' + versionSuffix, 'ld.lld']),
    )
    
  engine.logger.outputInfo(
    "clang: " + clangExe + "\n" +
    "llvm-ar: " + llvmArExe + "\n")
    
  from cake.library.compilers.clang import ClangCompiler

  if cake.system.isLinux():
    platform = 'linux'
  elif cake.system.isDarwin():
    platform = 'darwin'
  
  clangVariant = baseVariant.clone(compiler='clang',
                                   platform=platform,
                                   architecture='x64')

  # If libc++ is installed in a non-standard location, add the path to libc++.so
  # to the library search path by adding libc++'s /lib directory to LD_LIBRARY_PATH
  if libcxxInstallPrefix not in defaultInstallPaths:
    libcxxLibPath = os.path.abspath(cake.path.join(libcxxInstallPrefix, 'lib'))
    ldPaths = [libcxxLibPath]
    test = clangVariant.tools["test"]
    oldLdPath = test.env.get('LD_LIBRARY_PATH', None)
    if oldLdPath:
      ldPaths.append(oldLdPath)
    test.env['LD_LIBRARY_PATH'] = os.path.pathsep.join(ldPaths)

  compiler = ClangCompiler(
    configuration=configuration,
    clangExe=clangExe,
    llvmArExe=llvmArExe,
    binPaths=[clangBinPath])

  compiler.addCppFlag('-std=c++1z')
  compiler.addCppFlag('-fcoroutines-ts')
  compiler.addCppFlag('-m64')

  if lldExe:
    lldExeAbspath = configuration.abspath(lldExe)
    compiler.addModuleFlag('-fuse-ld=' + lldExeAbspath)
    compiler.addProgramFlag('-fuse-ld=' + lldExeAbspath)
  
  if libcxxInstallPrefix != clangInstallPrefix:
    compiler.addCppFlag('-nostdinc++')
    compiler.addIncludePath(cake.path.join(
      libcxxInstallPrefix, 'include', 'c++', 'v1'))
    compiler.addLibraryPath(cake.path.join(
      libcxxInstallPrefix, 'lib'))
  else:
    compiler.addCppFlag('-stdlib=libc++')

  compiler.addLibrary('c++')
  compiler.addLibrary('c')
  compiler.addLibrary('pthread')

  #compiler.addProgramFlag('-Wl,--trace')
  #compiler.addProgramFlag('-Wl,-v')

  clangVariant.tools['compiler'] = compiler

  env = clangVariant.tools["env"]
  env["COMPILER"] = "clang"
  env["COMPILER_VERSION"] = compiler.version
  env["PLATFORM"] = platform
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
  
  # Only use link-time optimisation if we're using LLD
  if lldExe:
    compiler.addCppFlag('-flto')
    compiler.addProgramFlag('-flto')
    compiler.addModuleFlag('-flto')

  configuration.addVariant(clangOptimisedVariant)
