#!/usr/bin/env bash
set -euo pipefail

: "${ANDROID_NDK_HOME:?}"
: "${ANDROID_DEPS_PREFIX:?}"
ABI=${ABI:-arm64-v8a}
API=${API:-28}
JOBS=${JOBS:-$(nproc)}
PY_VER=${PY_VER:-3.10.13}
BOOST_VER=${BOOST_VER:-1.82.0}

SRC=${ANDROID_DEPS_SRC:-$PWD/deps-src}
OUT=$ANDROID_DEPS_PREFIX
TC=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake
LLVM=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64
TRIPLE=aarch64-linux-android
CC=$LLVM/bin/${TRIPLE}${API}-clang
CXX=$LLVM/bin/${TRIPLE}${API}-clang++
AR=$LLVM/bin/llvm-ar
RANLIB=$LLVM/bin/llvm-ranlib
STRIP=$LLVM/bin/llvm-strip

mkdir -p "$SRC" "$OUT"
cd "$SRC"

fetch(){
  local name=$1 url=$2
  if [ ! -d "$name" ]; then
    mkdir -p "$name"
    curl -fsSL "$url" | tar xz -C "$name" --strip-components=1
  fi
}

cm(){
  local name=$1; shift
  cmake -S "$SRC/$name" -B "$SRC/$name/build-android" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TC" \
    -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_PREFIX_PATH="$OUT/zlib;$OUT/png;$OUT/brotli;$OUT/boost;$OUT/tbb" \
    -DCMAKE_INSTALL_PREFIX="$OUT/$name" \
    -DBUILD_SHARED_LIBS=OFF \
    "$@"
  cmake --build "$SRC/$name/build-android" -j"$JOBS"
  cmake --install "$SRC/$name/build-android"
}

done_flag(){ [ -f "$OUT/.done-$1" ]; }
mark(){ touch "$OUT/.done-$1"; }

if ! done_flag zlib; then
  fetch zlib https://github.com/madler/zlib/archive/refs/tags/v1.3.tar.gz
  cm zlib
  mark zlib
fi

if ! done_flag zstd; then
  fetch zstd https://github.com/facebook/zstd/archive/refs/tags/v1.5.5.tar.gz
  cmake -S zstd/build/cmake -B zstd/build-android -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TC" -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_STATIC=ON \
    -DCMAKE_INSTALL_PREFIX="$OUT/zstd"
  cmake --build zstd/build-android -j"$JOBS"
  cmake --install zstd/build-android
  mark zstd
fi

if ! done_flag png; then
  fetch png https://github.com/glennrp/libpng/archive/refs/tags/v1.6.40.tar.gz
  cm png -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_EXECUTABLES=OFF -DPNG_ARM_NEON=on -DZLIB_ROOT="$OUT/zlib"
  mark png
fi

if ! done_flag jpeg; then
  fetch jpeg https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.0.1.tar.gz
  cm jpeg -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF
  mark jpeg
fi

if ! done_flag brotli; then
  fetch brotli https://github.com/google/brotli/archive/refs/tags/v1.1.0.tar.gz
  cm brotli -DBROTLI_DISABLE_TESTS=ON
  mark brotli
fi

if ! done_flag freetype; then
  fetch freetype https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-2.tar.gz
  cm freetype -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_PNG=ON -DFT_REQUIRE_BROTLI=ON \
    -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_HARFBUZZ=ON \
    -DZLIB_ROOT="$OUT/zlib" -DPNG_ROOT="$OUT/png" -DBROTLIDEC_INCLUDE_DIRS="$OUT/brotli/include" \
    -DBROTLIDEC_LIBRARIES="$OUT/brotli/lib/libbrotlidec.a;$OUT/brotli/lib/libbrotlicommon.a"
  mark freetype
fi

if ! done_flag tbb; then
  fetch tbb https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.9.0.tar.gz
  cm tbb -DTBB_TEST=OFF -DTBB_STRICT=OFF -DTBB_EXAMPLES=OFF
  mark tbb
fi

if ! done_flag pugixml; then
  fetch pugixml https://github.com/zeux/pugixml/archive/refs/tags/v1.13.tar.gz
  cm pugixml
  mark pugixml
fi

if ! done_flag fmt; then
  fetch fmt https://github.com/fmtlib/fmt/archive/refs/tags/10.1.1.tar.gz
  cm fmt -DFMT_TEST=OFF -DFMT_DOC=OFF
  mark fmt
fi

if ! done_flag robin-map; then
  fetch robin-map https://github.com/Tessil/robin-map/archive/refs/tags/v1.2.1.tar.gz
  cm robin-map
  mark robin-map
fi

if ! done_flag epoxy; then
  fetch epoxy https://github.com/anholt/libepoxy/archive/refs/tags/1.5.10.tar.gz
  sed -i 's/libGL\.so\.1/libOSMesa.so/g; s/libGLESv2\.so\.2/libOSMesa.so/g' epoxy/src/dispatch_common.c epoxy/src/dispatch_common.h
  cat > epoxy-cross.txt <<CROSS
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'
[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
CROSS
  meson setup epoxy/build-android epoxy --cross-file epoxy-cross.txt \
    --prefix "$OUT/epoxy" --default-library static --buildtype release \
    -Dglx=no -Dx11=false -Degl=yes -Dtests=false
  ninja -C epoxy/build-android -j"$JOBS" install
  mark epoxy
fi

if ! done_flag boost; then
  fetch boost "https://archives.boost.io/release/$BOOST_VER/source/boost_${BOOST_VER//./_}.tar.gz"
  pushd boost
  ./bootstrap.sh --with-toolset=gcc >/dev/null
  cat > user-config.jam <<JAM
using clang : android : $CXX : <archiver>$AR <ranlib>$RANLIB <compileflags>-fPIC <compileflags>-DBOOST_ASIO_DISABLE_STD_ALIGNED_ALLOC ;
JAM
  ./b2 -j"$JOBS" --user-config=user-config.jam toolset=clang-android target-os=android \
    architecture=arm abi=aapcs address-model=64 binary-format=elf link=static runtime-link=shared \
    threading=multi variant=release --prefix="$OUT/boost" \
    --with-filesystem --with-thread --with-system --with-regex --with-date_time \
    --with-chrono --with-atomic --with-locale --with-serialization --with-iostreams install
  popd
  mark boost
fi

if ! done_flag python; then
  fetch python "https://www.python.org/ftp/python/$PY_VER/Python-$PY_VER.tgz"
  pushd python
  mkdir -p build-host && pushd build-host
  ../configure --prefix="$SRC/python-host" >/dev/null
  make -j"$JOBS" >/dev/null
  make install >/dev/null
  popd
  cat > config.site <<SITE
ac_cv_file__dev_ptmx=no
ac_cv_file__dev_ptc=no
ac_cv_buggy_getaddrinfo=no
ac_cv_func_wcsftime=no
ac_cv_func_ftime=no
ac_cv_func_faccessat=no
ac_cv_func_gethostbyname_r=no
ac_cv_func_sem_open=no
ac_cv_func_sem_timedwait=no
ac_cv_func_sem_getvalue=no
ac_cv_func_sem_unlink=no
ac_cv_func_shm_open=no
ac_cv_func_shm_unlink=no
ac_cv_func_linkat=no
ac_cv_func_lchmod=no
SITE
  mkdir -p build-android && pushd build-android
  CONFIG_SITE=../config.site \
  PATH="$SRC/python-host/bin:$PATH" \
  CC=$CC CXX=$CXX AR=$AR RANLIB=$RANLIB READELF=$LLVM/bin/llvm-readelf \
  CFLAGS="-fPIC -I$OUT/zlib/include" LDFLAGS="-L$OUT/zlib/lib" \
  ../configure --host=$TRIPLE --build="$(../config.guess)" \
    --prefix="$OUT/python" --with-build-python="$SRC/python-host/bin/python3.10" \
    --enable-shared --without-ensurepip --disable-ipv6 --with-openssl=no
  make -j"$JOBS"
  make install
  popd
  popd
  mark python
fi

if ! done_flag openimageio; then
  fetch openimageio https://github.com/AcademySoftwareFoundation/OpenImageIO/archive/refs/tags/v2.4.13.0.tar.gz
  cm openimageio -DBUILD_TESTING=OFF -DOIIO_BUILD_TESTS=OFF -DOIIO_BUILD_TOOLS=OFF \
    -DUSE_PYTHON=OFF -DUSE_QT=OFF -DUSE_OPENCOLORIO=OFF -DUSE_OPENEXR=OFF -DUSE_OPENCV=OFF \
    -DUSE_FFMPEG=OFF -DUSE_FREETYPE=OFF -DUSE_GIF=OFF -DUSE_LIBRAW=OFF -DUSE_OPENJPEG=OFF \
    -DUSE_PTEX=OFF -DUSE_WEBP=OFF -DUSE_TBB=OFF -DUSE_DCMTK=OFF -DUSE_NUKE=OFF -DUSE_BZIP2=OFF \
    -DUSE_JPEGTURBO=ON -DLINKSTATIC=ON -DBoost_USE_STATIC_LIBS=ON \
    -DBOOST_ROOT="$OUT/boost" -DBoost_NO_SYSTEM_PATHS=ON \
    -DJPEG_ROOT="$OUT/jpeg" -DPNG_ROOT="$OUT/png" -DZLIB_ROOT="$OUT/zlib" \
    -DPugixml_ROOT="$OUT/pugixml" -Dfmt_ROOT="$OUT/fmt" -DRobinmap_ROOT="$OUT/robin-map" \
    -DCMAKE_PREFIX_PATH="$OUT/zlib;$OUT/png;$OUT/jpeg;$OUT/boost;$OUT/pugixml;$OUT/fmt;$OUT/robin-map"
  mark openimageio
fi

if ! done_flag expat; then
  fetch expat https://github.com/libexpat/libexpat/archive/refs/tags/R_2_5_0.tar.gz
  cmake -S expat/expat -B expat/build-android -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TC" -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_TESTS=OFF \
    -DEXPAT_BUILD_DOCS=OFF -DEXPAT_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX="$OUT/expat"
  cmake --build expat/build-android -j"$JOBS"
  cmake --install expat/build-android
  mark expat
fi

if ! done_flag mesa; then
  MESA_VER=${MESA_VER:-23.3.6}
  fetch mesa "https://archive.mesa3d.org/mesa-$MESA_VER.tar.xz"
  sed -i 's/"libvulkan\.so\.1"/"libvulkan.so"/g' mesa/src/gallium/drivers/zink/zink_screen.c
  cat > mesa-cross.txt <<CROSS
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'
pkg-config = 'pkg-config'
[built-in options]
c_args = ['-fPIC', '-I$OUT/zlib/include', '-I$OUT/expat/include', '-I$OUT/zstd/include']
cpp_args = ['-fPIC', '-I$OUT/zlib/include', '-I$OUT/expat/include', '-I$OUT/zstd/include']
c_link_args = ['-L$OUT/zlib/lib', '-L$OUT/expat/lib', '-L$OUT/zstd/lib']
cpp_link_args = ['-L$OUT/zlib/lib', '-L$OUT/expat/lib', '-L$OUT/zstd/lib', '-static-libstdc++']
[properties]
needs_exe_wrapper = true
[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
CROSS
  export PKG_CONFIG_PATH="$OUT/zlib/lib/pkgconfig:$OUT/expat/lib/pkgconfig:$OUT/zstd/lib/pkgconfig"
  meson setup mesa/build-android mesa --cross-file mesa-cross.txt \
    --prefix "$OUT/mesa" --buildtype release \
    -Dplatforms= -Dgallium-drivers=swrast,zink -Dvulkan-drivers= \
    -Dllvm=disabled -Dosmesa=true -Dglx=disabled -Degl=disabled -Dgbm=disabled \
    -Dgles1=disabled -Dgles2=disabled -Dopengl=true -Dshared-glapi=enabled \
    -Dlibunwind=disabled -Dlmsensors=disabled -Dbuild-tests=false -Dzstd=enabled \
    -Dvalgrind=disabled -Dshader-cache=disabled -Dgallium-nine=false -Dgallium-xa=disabled \
    -Dgallium-vdpau=disabled -Dgallium-va=disabled -Dvideo-codecs= -Dandroid-libbacktrace=disabled
  ninja -C mesa/build-android -j"$JOBS" install
  mark mesa
fi
