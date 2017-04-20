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

import cake.path
import cake.system

from cake.engine import Variant
from cake.script import Script

configuration = Script.getCurrent().configuration
engine = configuration.engine

hostPlatform = cake.system.platform().lower()
hostArchitecture = cake.system.architecture().lower()

baseVariant = Variant()

from cake.library.script import ScriptTool
from cake.library.variant import VariantTool
from cake.library.env import EnvironmentTool
from cake.library.project import ProjectTool
from cake.library.compilers import CompilerNotFoundError

baseVariant.tools["script"] = script = ScriptTool(configuration=configuration)
baseVariant.tools["variant"] = variant = VariantTool(configuration=configuration)

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
  engine.addBuildSuccessCallback(project.build)

def getVisualStudio2015Compiler(targetArchitecture, ucrtVersion, windows10SdkVersion, vcInstallDir=None, windows10SdkBaseDir=None):

  from cake.msvs import getWindowsKitsDir, getMsvcProductDir
  from cake.library.compilers.msvc import MsvcCompiler

  if vcInstallDir is None:
    vcInstallDir = getMsvcProductDir("VisualStudio\\14.0")

  if windows10SdkBaseDir is None:
    windows10SdkBaseDir = getWindowsKitsDir(version="10")

  vcIncludeDir = cake.path.join(vcInstallDir, "include")

  windows10SdkIncludeDirs = cake.path.join(windows10SdkBaseDir, "Include", windows10SdkVersion, ["um", "shared"])

  ucrtIncludeDir = cake.path.join(windows10SdkBaseDir, "Include", ucrtVersion, "ucrt")

  if cake.system.isWindows64():
    vcNativeBinDir = cake.path.join(vcInstallDir, "bin", "amd64")
  else:
    vcNativeBinDir = cake.path.join(vcInstallDir, "bin")

  if targetArchitecture == "x64":
    if cake.system.isWindows64():
      vcBinDir = vcNativeBinDir
    else:
      vcBinDir = cake.path.join(vcInstallDir, "bin", "x86_amd64")
    vcLibDir = cake.path.join(vcInstallDir, "lib", "amd64")
    windows10SdkBinDir = cake.path.join(windows10SdkBaseDir, "bin", "x64")
    windows10SdkLibDir = cake.path.join(windows10SdkBaseDir, "Lib", windows10SdkVersion, "um", "x64")
    ucrtLibDir = cake.path.join(windows10SdkBaseDir, "Lib", ucrtVersion, "ucrt", "x64")
  elif targetArchitecture == "x86":
    if cake.system.isWindows64():
      vcBinDir = cake.path.join(vcInstallDir, "bin", "amd64_x86")
    else:
      vcBinDir = vcNativeBinDir
    vcLibDir = cake.path.join(vcInstallDir, "lib")
    windows10SdkBinDir = cake.path.join(windows10SdkBaseDir, "bin", "x86")
    windows10SdkLibDir = cake.path.join(windows10SdkBaseDir, "Lib", windows10SdkVersion, "um", "x86")
    ucrtLibDir = cake.path.join(windows10SdkBaseDir, "Lib", ucrtVersion, "ucrt", "x86")

  vcBinPaths = [vcBinDir]
  if vcNativeBinDir != vcBinDir:
    vcBinPaths.append(vcNativeBinDir)

  return MsvcCompiler(
    configuration=configuration,
    clExe=cake.path.join(vcBinDir, "cl.exe"),
    libExe=cake.path.join(vcBinDir, "lib.exe"),
    linkExe=cake.path.join(vcBinDir, "link.exe"),
    rcExe=cake.path.join(windows10SdkBinDir, "rc.exe"),
    mtExe=cake.path.join(windows10SdkBinDir, "mt.exe"),
    bscExe=None,
    binPaths=vcBinPaths + [windows10SdkBinDir],
    includePaths=[vcIncludeDir, ucrtIncludeDir] + windows10SdkIncludeDirs,
    libraryPaths=[vcLibDir, ucrtLibDir, windows10SdkLibDir],
    architecture=targetArchitecture,
    )

def findMsvc2017InstallDir(targetArchitecture):
  """Find the location of the MSVC 2017 install directory.

  Returns path of the latest VC install directory that contains a compiler
  for the specified target architecture. Throws CompilerNotFoundError if
  couldn't find any MSVC 2017 version.
  """
  # TODO: We are supposed to use the new COM API to lookup the installed
  # locations of the VS 2017 build tools but I haven't figured out how to
  # do this successfully from Python yet.
  # We'll just use some simple heuristics to try and find it in the mean-time.
  if cake.system.isWindows64():
    programFiles = os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')
  else:
    programFiles = os.environ.get('ProgramFiles', r'C:\Program Files')

  vs2017BasePath = cake.path.join(programFiles, 'Microsoft Visual Studio', '2017')

  for edition in ('Enterprise', 'Professional', 'Community'):
    msvcBasePath = cake.path.join(vs2017BasePath, edition, r'VC\Tools\MSVC')
    if os.path.isdir(msvcBasePath):
      versions = [v for v in os.listdir(msvcBasePath) if v.startswith('14.10.')]
      versions.sort(key=lambda x: int(x[len('14.10.'):]), reverse=True)
      for version in versions:
        msvcPath = cake.path.join(msvcBasePath, version)
        if cake.system.isWindows64():
          if os.path.isdir(cake.path.join(msvcPath, 'bin', 'HostX64', targetArchitecture)):
            return msvcPath
        else:
          if os.path.isdir(cake.path.join(msvcPath, 'bin', 'HostX86', targetArchitecture)):
            return msvcPath
  else:
    raise CompilerNotFoundError()

def getVisualStudio2017Compiler(targetArchitecture, ucrtVersion, windows10SdkVersion, vcInstallDir=None, windows10SdkBaseDir=None):

  from cake.msvs import getWindowsKitsDir
  from cake.library.compilers.msvc import MsvcCompiler

  if vcInstallDir is None:
    vcInstallDir = findMsvc2017InstallDir(targetArchitecture)

  if windows10SdkBaseDir is None:
    windows10SdkBaseDir = getWindowsKitsDir(version="10")

  vcIncludeDir = cake.path.join(vcInstallDir, "include")

  windows10SdkIncludeDirs = cake.path.join(windows10SdkBaseDir, "Include", windows10SdkVersion, ["um", "shared"])

  ucrtIncludeDir = cake.path.join(windows10SdkBaseDir, "Include", ucrtVersion, "ucrt")

  if cake.system.isWindows64():
    vcNativeBinDir = cake.path.join(vcInstallDir, "bin", "HostX64", "x64")
  else:
    vcNativeBinDir = cake.path.join(vcInstallDir, "bin", "HostX86", "x86")

  if targetArchitecture == "x64":
    if cake.system.isWindows64():
      vcBinDir = vcNativeBinDir
    else:
      vcBinDir = cake.path.join(vcInstallDir, "bin", "HostX86", "x64")
    vcLibDir = cake.path.join(vcInstallDir, "lib", "x64")
    windows10SdkBinDir = cake.path.join(windows10SdkBaseDir, "bin", "x64")
    windows10SdkLibDir = cake.path.join(windows10SdkBaseDir, "Lib", windows10SdkVersion, "um", "x64")
    ucrtLibDir = cake.path.join(windows10SdkBaseDir, "Lib", ucrtVersion, "ucrt", "x64")
  elif targetArchitecture == "x86":
    if cake.system.isWindows64():
      vcBinDir = cake.path.join(vcInstallDir, "bin", "HostX64", "x86")
    else:
      vcBinDir = vcNativeBinDir
    vcLibDir = cake.path.join(vcInstallDir, "lib", "x86")
    windows10SdkBinDir = cake.path.join(windows10SdkBaseDir, "bin", "x86")
    windows10SdkLibDir = cake.path.join(windows10SdkBaseDir, "Lib", windows10SdkVersion, "um", "x86")
    ucrtLibDir = cake.path.join(windows10SdkBaseDir, "Lib", ucrtVersion, "ucrt", "x86")

  vcBinPaths = [vcBinDir]
  if vcNativeBinDir != vcBinDir:
    vcBinPaths.append(vcNativeBinDir)

  return MsvcCompiler(
    configuration=configuration,
    clExe=cake.path.join(vcBinDir, "cl.exe"),
    libExe=cake.path.join(vcBinDir, "lib.exe"),
    linkExe=cake.path.join(vcBinDir, "link.exe"),
    rcExe=cake.path.join(windows10SdkBinDir, "rc.exe"),
    mtExe=cake.path.join(windows10SdkBinDir, "mt.exe"),
    bscExe=None,
    binPaths=vcBinPaths + [windows10SdkBinDir],
    includePaths=[vcIncludeDir, ucrtIncludeDir] + windows10SdkIncludeDirs,
    libraryPaths=[vcLibDir, ucrtLibDir, windows10SdkLibDir],
    architecture=targetArchitecture,
    )

if cake.system.isWindows() or cake.system.isCygwin():
  for msvcVer in ("14.10", "14.0"):
    for arch in ("x64", "x86"):
      try:
        msvcVariant = baseVariant.clone(
          platform="windows",
          compiler="msvc" + msvcVer,
          compilerFamily="msvc",
          architecture=arch,
          )

        project = msvcVariant.tools["project"]
        project.solutionPlatformName = "Windows " + arch + " (Msvc " + msvcVer + ")"
        if arch == "x86":
          project.projectPlatformName = "Win32"
        elif arch == "x64":
          project.projectPlatformName = "x64"

        windows10SdkVersion = "10.0.10586.0"
        ucrtVersion = windows10SdkVersion

        if msvcVer == "14.0":
          msvcVariant.tools["compiler"] = compiler = getVisualStudio2015Compiler(
            targetArchitecture=arch,
            ucrtVersion=ucrtVersion,
            windows10SdkVersion=windows10SdkVersion,
            vcInstallDir=None, # Or path to unzipped .nuget package
            windows10SdkBaseDir=None, # Or path to unzipped Windows 10 SDK
            )
        elif msvcVer == "14.10":
          msvcVariant.tools["compiler"] = compiler = getVisualStudio2017Compiler(
            targetArchitecture=arch,
            ucrtVersion=ucrtVersion,
            windows10SdkVersion=windows10SdkVersion,
            vcInstallDir=None, # Or path to unzipped .nuget package
            windows10SdkBaseDir=None, # Or path to unzipped Windows 10 SDK
            )

        if engine.options.createProjects:
          compiler.enabled = False

        compiler.enableRtti = True
        compiler.enableExceptions = True
        compiler.outputMapFile = True
        compiler.messageStyle = compiler.MSVS_CLICKABLE
        compiler.warningLevel = '3'
        compiler.warningsAsErrors = True

        # Enable experimental C++ coroutines via command-line flag.
        compiler.addCppFlag('/await')

        env = msvcVariant.tools["env"]
        env["COMPILER"] = "msvc"
        env["COMPILER_VERSION"] = msvcVer
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
        project.projectConfigName = "Windows (" + arch + ") Msvc " + msvcVer + " (Debug)"
        project.solutionConfigName = "Debug"
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
        project.projectConfigName = "Windows (" + arch + ") Msvc " + msvcVer + " (Optimised)"
        project.solutionConfigName = "Optimised"
        configuration.addVariant(msvcOptVariant)

      except CompilerNotFoundError:
        pass
