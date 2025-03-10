#!/bin/bash
set -e
set -x

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR=$SCRIPT_DIR/../../
cd $REPO_DIR

mkdir -p build
cd build
# Boost unit tests don't like the static linking
# /usr/local/bin/gcc + g++ is 9.2.0 version
cmake -E env LDFLAGS="-Wl,--exclude-libs,ALL -static-libgcc -static-libstdc++" cmake .. -DCMAKE_BUILD_TYPE=Release -DWARNINGS=Off -DBUILD_JAVA=On -DBUILD_DOCS=Off -DBUILD_FLATBUFFERS=On -DVW_BUILD_CSV=On\
 -DBUILD_PYTHON=Off -DSTATIC_LINK_VW_JAVA=On -DCMAKE_C_COMPILER=/usr/local/bin/gcc -DCMAKE_CXX_COMPILER=/usr/local/bin/g++ \
 -DBUILD_TESTING=Off -DVW_ZLIB_SYS_DEP=Off -DBUILD_SHARED_LIBS=Off
NUM_PROCESSORS=$(nproc)
make vw_jni -j ${NUM_PROCESSORS}
