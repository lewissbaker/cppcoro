#!/bin/sh
set -e
cd $(dirname $(dirname $(dirname $(readlink -f $0))))

if [ -z "${CC}" ]; then
  CC=clang
fi

if [ -z "${CXX}" ]; then
  CXX=clang++
fi

if [ -z "${ARCH}" ]; then
  ARCH=x64
fi

if [ -z "${CLANG_VERSION}" ]; then
  CLANG_VERSION=7
fi

if [ -z "${LLVM_INSTALL_PREFIX}" ]; then
  LLVM_INSTALL_PREFIX="`pwd`/build/tools/llvm"
fi

CMAKE_LLVM_INSTALL_PREFIX=
if [ -d "${LLVM_INSTALL_PREFIX}" ]; then
  CMAKE_LLVM_INSTALL_PREFIX="-DLLVM_INSTALL_PREFIX:PATH=${LLVM_INSTALL_PREFIX}"
fi

CMAKE_BUILD_TARGET=
if [ "${1}" = "run" ]; then
  CMAKE_BUILD_TARGET="--target run_tests"
fi

export CC="${CC}-${CLANG_VERSION}"
export CXX="${CXX}-${CLANG_VERSION}"

build() {
  mkdir -p build/llvm-${ARCH}/${1} && cd build/llvm-${ARCH}/${1}
  if [ ! -f build.ninja ]; then
    cmake -GNinja -DCMAKE_BUILD_TYPE="${1}" ${CMAKE_LLVM_INSTALL_PREFIX} \
      -DCPPCORO_EXTRA_WARNINGS:BOOL=ON \
      -DCPPCORO_BUILD_TESTS:BOOL=ON \
      ../../..
  fi
  cmake --build . ${CMAKE_BUILD_TARGET}
  cd ../../..
}

case "${RELEASE}" in
debug)
  build Debug
  ;;
optimised)
  build RelWithDebInfo
  ;;
*)
  build Debug
  build RelWithDebInfo
  ;;
esac
