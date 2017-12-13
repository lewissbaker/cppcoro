###############################################################################
# Kt C++ Library
# Copyright (c) 2005-2010  Lewis Baker
###############################################################################

import subprocess
import os

import cake.filesys
import cake.path

from cake.async import waitForAsyncResult, flatten
from cake.target import getPath, getPaths, getTask, Target, FileTarget
from cake.library import Tool
from cake.script import Script

class UnitTestTool(Tool):

  enabled = True
  alwaysRun = None

  def __init__(self, configuration):
    Tool.__init__(self, configuration)
    self.dependencies = []
    self.extraArgs = []
    self.env = dict(os.environ)

  def run(self, program, results=None, cwd=None, extraArgs=[], dependencies=[]):

    tool = self.clone()
    if dependencies:
      tool.dependencies.extend(dependencies)
    if extraArgs:
      tool.extraArgs.extend(extraArgs)

    return tool._run(program, results, cwd)

  def _run(self, program, results, cwd):

    @waitForAsyncResult
    def _start(program, results, dependencies):
      if self.alwaysRun is False:
        return

      task = self.engine.createTask(
        lambda: self._launchTest(program, results, cwd, flatten(dependencies)))
      task.parent.completeAfter(task)
      task.startAfter(getTask([program, dependencies]))

    @waitForAsyncResult
    def _makeTarget(results):
      if self.enabled:
        task = self.engine.createTask(lambda: _start(program, results, self.dependencies))
        task.lazyStart()
      else:
        task = None

      if results:
        target = FileTarget(path=results, task=task)
      else:
        target = Target(task=task)

      Script.getCurrent().getDefaultTarget().addTarget(target)
      Script.getCurrent().getTarget('testresults').addTarget(target)

      return target

    return _makeTarget(results)

  def _launchTest(self, program, results, cwd, dependencies):

    programPath = getPath(program)

    programArgs = [programPath] + self.extraArgs

    dependencyArgs = programArgs + dependencies

    if self.alwaysRun is None and results:
      try:
        _, reason = self.configuration.checkDependencyInfo(results, dependencyArgs)
        if reason is None:
          # Up to date
          return
          
        self.engine.logger.outputDebug(
          "reason",
          "Building '%s' because '%s'\n" % (results, reason),
          )
      except EnvironmentError:
        pass

    self.engine.logger.outputInfo("Testing %s\n" % programPath)

    self.engine.logger.outputDebug("run", " ".join(programArgs))

    if results:
      resultsAbsPath = self.configuration.abspath(results)
      cake.filesys.makeDirs(cake.path.dirName(resultsAbsPath))
      stdout = open(resultsAbsPath, "w+t")
    else:
      stdout = tempfile.TemporaryFile(mode="w+t")

    try:
      p = subprocess.Popen(
        args=programArgs,
        executable=self.configuration.abspath(programPath),
        stdin=subprocess.PIPE,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        cwd=cwd,
        env=self.env
        )
    except EnvironmentError, e:
      msg = "cake: failed to launch %s: %s\n" % (programPath, str(e))
      self.engine.raiseError(msg, targets=targets)

    p.stdin.close()
    exitCode = p.wait()

    stdout.seek(0)
    testOutput = stdout.read()
    stdout.close()

    self.engine.logger.outputInfo(testOutput)

    if exitCode != 0:
      msg = "test %s failed with exit code %i (0x%08x)\n" % (programPath, exitCode, exitCode)
      self.engine.raiseError(msg, targets=[results])

    if results:
      newDependencyInfo = self.configuration.createDependencyInfo(
        targets=[results],
        args=dependencyArgs,
        dependencies=[programPath] + getPaths(dependencies),
        )
      self.configuration.storeDependencyInfo(newDependencyInfo)
