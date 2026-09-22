#!/bin/bash
# Build Kobo's qtbase 6.5.2 twice: the tools for the host, then the libraries
# for the device. Both install under the toolchain directory.
set -euo pipefail
tc=$1
src=$2
jobs=$(nproc)

cp "$src/kobo.cmake" "$tc/kobo.cmake"
cp -r "$src/mkspecs/linux-arm-kobo-gnueabihf-g++" "$src/qtbase/mkspecs/"

# The host build only needs QtCore for moc, rcc, uic, qmake and qlalr, and
# QtDBus so the device build finds the D-Bus code generators.
cmake -S "$src/qtbase" -B "$src/host" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$tc/host" \
    -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
    -DFEATURE_gui=OFF -DFEATURE_widgets=OFF -DFEATURE_network=OFF -DFEATURE_sql=OFF \
    -DFEATURE_testlib=OFF -DFEATURE_xml=OFF -DFEATURE_concurrent=OFF -DFEATURE_printsupport=OFF \
    -DFEATURE_dbus=ON -DFEATURE_icu=OFF -DFEATURE_glib=OFF -DINPUT_opengl=no
cmake --build "$src/host" --parallel "$jobs"
cmake --install "$src/host"
"$tc/host/libexec/moc" --version

# The device build follows the firmware's libQt6Core and libQt6Gui link
# dependencies: system ICU, glib, zstd, PCRE2, zlib, fontconfig, FreeType,
# HarfBuzz, libpng, xkbcommon, D-Bus, EGL and OpenGL ES 2. The firmware's Gui
# does not link libjpeg, brotli or AT-SPI, so those stay bundled or off.
# OpenGL ES 2 matters for the ABI: with it off, QPlatformIntegration and
# other QPA classes lose virtual methods Nickel's Qt has.
# Kobo's Gui compiles its AT-SPI sources without the AT-SPI bridge feature,
# so the headers must be on the include path even though nothing links them.
cmake -S "$src/qtbase" -B "$src/target" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$tc/qt6" \
    -DCMAKE_TOOLCHAIN_FILE="$tc/kobo.cmake" -DQT_HOST_PATH="$tc/host" \
    -DQT_QMAKE_TARGET_MKSPEC=linux-arm-kobo-gnueabihf-g++ \
    -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
    -DQT_EXTRA_INCLUDEPATHS=/usr/include/at-spi-2.0 \
    -DINPUT_opengl=es2 -DFEATURE_opengles2=ON -DFEATURE_egl=ON -DFEATURE_eglfs=ON \
    -DFEATURE_xcb=OFF -DFEATURE_xlib=OFF -DFEATURE_vulkan=OFF -DFEATURE_brotli=OFF \
    -DFEATURE_system_zlib=ON -DFEATURE_system_pcre2=ON -DFEATURE_icu=ON -DFEATURE_glib=ON \
    -DFEATURE_zstd=ON -DFEATURE_fontconfig=ON -DFEATURE_system_freetype=ON \
    -DFEATURE_system_harfbuzz=ON -DFEATURE_system_libpng=ON -DFEATURE_system_libjpeg=OFF \
    -DFEATURE_xkbcommon=ON -DFEATURE_dbus=ON -DFEATURE_dbus_linked=ON \
    -DFEATURE_openssl=ON -DFEATURE_openssl_linked=OFF
cmake --build "$src/target" --parallel "$jobs"
cmake --install "$src/target"
cp "$src/target/config.summary" "$tc/qt6/config.summary"

# One place to find every tool. qmake6, qtpaths6 and qt-cmake in qt6/bin are
# Qt's wrappers around the host tools with the device configuration. They find
# that configuration next to their own path, so a link would break them; a
# wrapper that runs them in place does not. The rest are host programs that
# are the same for either build.
ln -sf qt6/lib/cmake/Qt6/qt.toolchain.cmake "$tc/toolchain.cmake"
for tool in qmake6 qtpaths6 qt-cmake; do
    printf '#!/bin/sh\nexec "%s/qt6/bin/%s" "$@"\n' "$tc" "$tool" > "$tc/bin/$tool"
    chmod 755 "$tc/bin/$tool"
done
ln -sf qmake6 "$tc/bin/qmake"
for tool in moc uic rcc qlalr tracegen tracepointgen cmake_automoc_parser syncqt; do
    ln -sf "../host/libexec/$tool" "$tc/bin/$tool"
done
for tool in qdbuscpp2xml qdbusxml2cpp qvkgen; do
    ln -sf "../host/bin/$tool" "$tc/bin/$tool"
done

# Record what went into the image next to the tools.
{
    echo "qtbase 6.5.2 with local patches from kobolabs/Kobo-Reader packages-v5"
    arm-linux-gnueabihf-gcc-11 --version | head -1
    dpkg-query -W -f '${Package} ${Version}\n' '*-cross' '*:armhf' cmake ninja-build 2>/dev/null | sort
} > "$tc/manifest.txt"
