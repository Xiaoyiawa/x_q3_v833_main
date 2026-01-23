export TOOLCHAIN=/home/xiaoyi/gcc
export PATH=$PATH:$TOOLCHAIN/bin
export CC=$TOOLCHAIN/bin/arm-unknown-linux-musleabihf-gcc
export CXX=$TOOLCHAIN/bin/arm-unknown-linux-musleabihf-g++
export AR=$TOOLCHAIN/bin/arm-unknown-linux-musleabihf-ar
export RANLIB=$TOOLCHAIN/bin/arm-unknown-linux-musleabihf-ranlib
export STRIP=$TOOLCHAIN/bin/arm-unknown-linux-musleabihf-strip

export STAGING_DIR=$TOOLCHAIN/bin

make -j${nproc}
