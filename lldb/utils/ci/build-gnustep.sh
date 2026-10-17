#!/usr/bin/env bash
#===-- build-gnustep.sh - Build a GNUstep stack for LLDB's tests ---------===//
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===//
#
# Builds the GNUstep libobjc2 runtime and gnustep-base (Foundation) into one
# install prefix, for configuring LLDB with
#
#   -DLLDB_TEST_OBJC_GNUSTEP=On
#   -DLLDB_TEST_OBJC_GNUSTEP_DIR=<prefix>
#
# Distribution packages cannot be used: Debian and Ubuntu build gnustep-base
# against GCC's Objective-C runtime, not libobjc2.
#
# The versions are pinned so that a change in GNUstep does not show up as an
# LLDB failure. gnustep-base is built with debug info, so that a test can
# step into it.
#
# Besides a compiler, CMake, Ninja and git, gnustep-base needs the
# development files for libcurl, libffi, GnuTLS, ICU, libxml2 and libxslt.
#
# Usage: build-gnustep.sh <prefix> [<work-dir>]
#   CC and CXX select the compiler (default: clang, clang++).
#   JOBS sets the build parallelism (default: nproc).
#
#===----------------------------------------------------------------------===//

set -euo pipefail

PREFIX=${1:?usage: build-gnustep.sh <prefix> [<work-dir>]}
WORK=${2:-$(mktemp -d)}
CC=${CC:-clang}
CXX=${CXX:-clang++}
JOBS=${JOBS:-$(nproc)}

# libobjc2 v2.3, tools-make make-2_9_3, libs-base base-1_31_1.
LIBOBJC2_COMMIT=e877e782fb965c4e870d2ecf6aff58dddc6290ae
TOOLS_MAKE_COMMIT=214a57eb7a6d2e0590a8aece4970a72561beb8d5
LIBS_BASE_COMMIT=6307e474dd39e36eb2c5f6ee15ade8b51e337970

# The ABI gnustep-base is compiled for. gnustep-make bakes it into every
# compile line, so it has to match the -fobjc-runtime of any test inferior
# that links against gnustep-base.
RUNTIME_ABI=gnustep-2.2

mkdir -p "$PREFIX" "$WORK"
PREFIX=$(cd "$PREFIX" && pwd)
WORK=$(cd "$WORK" && pwd)

fetch() { # <repo> <commit> <dir>
  rm -rf "$3"
  git init -q "$3"
  git -C "$3" fetch -q --depth 1 "https://github.com/gnustep/$1.git" "$2"
  git -C "$3" checkout -q FETCH_HEAD
}

echo "== libobjc2"
fetch libobjc2 "$LIBOBJC2_COMMIT" "$WORK/libobjc2"
cmake -G Ninja -S "$WORK/libobjc2" -B "$WORK/libobjc2/build" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DGNUSTEP_INSTALL_TYPE=NONE \
  -DTESTS=OFF
ninja -C "$WORK/libobjc2/build" -j "$JOBS" install

echo "== tools-make"
fetch tools-make "$TOOLS_MAKE_COMMIT" "$WORK/tools-make"
(cd "$WORK/tools-make" &&
  CC="$CC" CXX="$CXX" ./configure --prefix="$PREFIX" \
    --with-layout=fhs --with-libdir=lib \
    --with-library-combo=ng-gnu-gnu --with-runtime-abi="$RUNTIME_ABI" &&
  make -j "$JOBS" && make install)

echo "== libs-base"
fetch libs-base "$LIBS_BASE_COMMIT" "$WORK/libs-base"
# This release declares an instance variable __block, which clang rejects
# whenever blocks are enabled. The qualifier means nothing on an ivar.
HEADER="$WORK/libs-base/Headers/Foundation/NSBackgroundActivityScheduler.h"
sed -i 's/^  BLOCK_SCOPE GSScheduledBlock _block;$/  GSScheduledBlock _block;/' \
  "$HEADER"
if grep -q 'BLOCK_SCOPE GSScheduledBlock' "$HEADER"; then
  echo "error: the NSBackgroundActivityScheduler.h workaround did not apply" >&2
  exit 1
fi
# Recent clang makes several legacy C diagnostics errors by default; the
# feature tests in this release's configure predate that.
LEGACY_C="-Wno-error=incompatible-pointer-types"
LEGACY_C+=" -Wno-error=implicit-function-declaration"
LEGACY_C+=" -Wno-error=implicit-int -Wno-error=int-conversion"
(
  cd "$WORK/libs-base"
  # GNUstep.sh is written for neither errexit nor nounset.
  set +eu
  . "$PREFIX/share/GNUstep/Makefiles/GNUstep.sh"
  set -eu
  export CC="$CC" CXX="$CXX" OBJC="$CC" OBJCXX="$CXX" LDCC="$CC"
  # These reach configure's tests. The build's own flags, -fobjc-runtime
  # included, come from gnustep-make. Every Objective-C object file
  # references __objc_load, configure's test programs included, so the
  # runtime has to be linked throughout.
  export CFLAGS="$LEGACY_C" OBJCFLAGS="$LEGACY_C"
  export CPPFLAGS="-I$PREFIX/include"
  export LDFLAGS="-L$PREFIX/lib -Wl,-rpath,$PREFIX/lib -lobjc"
  ./configure --enable-libffi
  make -j "$JOBS" debug=yes strip=no
  make install debug=yes strip=no
)

# The tests find Foundation by its header and the runtime by its loader entry
# point; check both rather than discover a broken prefix as skipped tests.
# Check for gnustep-base's debug info too, since a test that steps into it
# would otherwise quietly step over instead.
test -f "$PREFIX/include/Foundation/Foundation.h"
nm -D --defined-only "$PREFIX/lib/libobjc.so" | grep -w __objc_load > /dev/null
readelf -S --wide "$PREFIX/lib/libgnustep-base.so" | grep -F .debug_info > /dev/null
echo "== GNUstep installed in $PREFIX"
