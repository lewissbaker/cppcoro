#!/bin/bash
#
# Builds libc++ from source

export WORKSPACE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LLVM_PROJECT="${WORKSPACE_ROOT}/tools/llvm-project"

: "${LLVM_REPO:=https://github.com/llvm-mirror/llvm.git}"
: "${LLVM_REF:=master}"

: "${LIBCXX_REPO:=https://github.com/llvm-mirror/libcxx.git}"
: "${LIBCXX_REF:=master}"

: "${LLVM_INSTALL_PREFIX:=$WORKSPACE_ROOT/build/llvm-install}"

: "${CXX:=clang++}"
: "${CC:=clang}"

if [ ! -d "${LLVM_PROJECT}" ]; then
  mkdir -p "${LLVM_PROJECT}" || exit
fi

cd "${LLVM_PROJECT}" || exit

if [ ! -d llvm/.git ]; then
  git clone --depth=1 -b "$LLVM_REF" -- "$LLVM_REPO" llvm || exit
else
  (cd llvm &&
   git remote set-url origin "$LLVM_REPO" &&
   git fetch origin "$LLVM_REF" &&
   git checkout FETCH_HEAD) || exit
fi

if [ ! -d llvm/projects/libcxx.git ]; then
  git clone --depth=1 -b "$LIBCXX_REF" -- "$LIBCXX_REPO" llvm/projects/libcxx || exit
  ln -s llvm/projects/libcxx libcxx || exit
else
  (cd llvm/projects/libcxx &&
   git remote set-url origin "$LIBCXX_REPO" &&
   git fetch --depth=1 origin "$LIBCXX_REF" &&
   git checkout FETCH_HEAD) || exit
fi

cd "$WORKSPACE_ROOT" || exit

(mkdir -p build/libcxx && \
 cd build/libcxx && \
 cmake -GNinja \
       -DCMAKE_CXX_COMPILER="${CXX}" \
       -DCMAKE_C_COMPILER="${CC}" \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_INSTALL_PREFIX="${LLVM_INSTALL_PREFIX}" \
       -DLLVM_PATH="${LLVM_PROJECT}/llvm" \
       -DLIBCXX_CXX_ABI=libstdc++ \
       "${LLVM_PROJECT}/libcxx" && \
  ninja install) || exit
