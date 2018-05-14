@echo off
setlocal enableextensions enabledelayedexpansion

cd %~dp0\..\..

if "%PLATFORM%" == "" (
  set GENERATOR="Visual Studio 15 2017";"Visual Studio 15 2017 Win64"
) else (
  if "%PLATFORM%" == "x86" (
    set GENERATOR="Visual Studio 15 2017"
  ) else (
    set GENERATOR="Visual Studio 15 2017 Win64"
  )
)

if "%CONFIGURATION%" == "" (
  set CONFIG="Debug";"RelWithDebInfo"
) else (
  if "%CONFIGURATION%" == "debug" (
    set CONFIG="Debug"
  ) else (
    set CONFIG="RelWithDebInfo"
  )
)

set CMAKE_BUILD_TARGET=
if "%1" == "run" set CMAKE_BUILD_TARGET=--target %1

for %%g in (%GENERATOR%) do (
  if %%g == "Visual Studio 15 2017" (
    md build\msvc-x86 2>nul
    cd build\msvc-x86
  ) else (
    md build\msvc-x64 2>nul
    cd build\msvc-x64
  )
  if not exist cppcoro.sln (
    cmake -G %%g -DCMAKE_CONFIGURATION_TYPES="Debug;RelWithDebInfo" ^
      -DCPPCORO_EXTRA_WARNINGS:BOOL=ON ^
      -DCPPCORO_BUILD_TESTS:BOOL=ON ^
      ..\..
  )
  for %%c in (%CONFIG%) do (
    cmake --build . %CMAKE_BUILD_TARGET%
  ) 
  cd ..\..
)
