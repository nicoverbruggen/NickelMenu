# A cross toolchain for Kobo Qt6 firmware. It is the Qt 6
# counterpart of NickelTC (https://github.com/pgaskin/NickelTC), which covers
# firmware 3.x and 4.x with Qt 5.2.1.
#
# Kobo built firmware 5.18 with GCC 11.5.0 and glibc 2.35, a Yocto kirkstone
# set, and its Qt 6.5.2 links system ICU 70, glib 2.72, fontconfig, FreeType,
# HarfBuzz, xkbcommon, D-Bus and OpenGL ES 2. Ubuntu 22.04 ships the same
# glibc, GCC 11.4 and the same library generation for armhf, so this image
# uses Ubuntu's cross compiler and armhf multiarch packages as the sysroot
# instead of a crosstool-ng build. GCC 11.4 and 11.5 share the C++ ABI
# (GLIBCXX_3.4.29); the firmware's libstdc++ is 6.0.29 as well.
#
# Qt itself is built from Kobo's published 6.5.2 qtbase source, configured
# with the same system libraries and OpenGL ES 2 as the firmware, so the
# headers describe the ABI Nickel was built against. Only qtbase is built;
# other Qt modules are not included in this toolchain.
#
# Layout of the result, under /tc/arm-kobo-linux-gnueabihf:
#   bin/arm-kobo-linux-gnueabihf-*  compiler and binutils; gcc and g++ carry
#                                   the Kobo CPU flags, so no flags are needed
#   bin/qmake6, moc, uic, rcc, ...  Qt tools for the host, targeting the device
#   bin/arm-kobo-linux-gnueabihf-pkg-config
#   qt6/                            Kobo's Qt 6.5.2 built for the device
#   host/                           the host build of the Qt tools
#   kobo.cmake                      the compiler toolchain file
#   toolchain.cmake                 Qt's toolchain file; chainloads kobo.cmake
# The C library and system libraries stay where Ubuntu puts cross packages:
# /usr/arm-linux-gnueabihf and /usr/lib/arm-linux-gnueabihf.

# The digest is the multi-architecture index, so amd64 and arm64 hosts build
# the same image.
FROM docker.io/library/ubuntu:22.04@sha256:2edbbc5dc405e9612ba3584ce95480277e3eb374407b5505fe26f17df77c7dbc AS base
ARG DEBIAN_FRONTEND=noninteractive

# The armhf packages come from ports.ubuntu.com on every host architecture.
# The stock sources.list must be restricted to the native architecture first,
# or apt looks for armhf packages on archive.ubuntu.com and fails.
RUN set -eu; \
    arch="$(dpkg --print-architecture)"; \
    sed -i "s|^deb |deb [arch=$arch] |" /etc/apt/sources.list; \
    dpkg --add-architecture armhf; \
    for pocket in jammy jammy-updates jammy-security; do \
        echo "deb [arch=armhf] http://ports.ubuntu.com/ubuntu-ports $pocket main universe"; \
    done > /etc/apt/sources.list.d/armhf.list; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        bash bzip2 ca-certificates cmake coreutils curl file g++ gcc git jq \
        libdbus-1-dev make ninja-build patch pkg-config python3 unzip xz-utils zip \
        gcc-11-arm-linux-gnueabihf g++-11-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
        libc6-dev:armhf libstdc++-11-dev:armhf \
        libatspi2.0-dev:armhf libdbus-1-dev:armhf libdrm-dev:armhf libegl-dev:armhf libevdev-dev:armhf \
        libfontconfig-dev:armhf libfreetype-dev:armhf libgbm-dev:armhf libgles-dev:armhf \
        libglib2.0-dev:armhf libharfbuzz-dev:armhf libicu-dev:armhf libinput-dev:armhf \
        libmtdev-dev:armhf libpcre2-dev:armhf libpng-dev:armhf libsqlite3-dev:armhf \
        libssl-dev:armhf libudev-dev:armhf libxkbcommon-dev:armhf libzstd-dev:armhf \
        zlib1g-dev:armhf; \
    rm -rf /var/lib/apt/lists/*

FROM base AS build

COPY source /tc/source
RUN python3 /tc/source/prepare.py /tc/src/qtbase

COPY wrappers.sh kobo.cmake build-qt.sh /tc/src/
COPY mkspecs /tc/src/mkspecs
RUN /tc/src/wrappers.sh /tc/arm-kobo-linux-gnueabihf

# The Qt build takes tens of minutes. It runs at its final path because the
# generated toolchain files and qmake wrappers record absolute paths.
RUN /tc/src/build-qt.sh /tc/arm-kobo-linux-gnueabihf /tc/src

FROM base AS toolchain
COPY --from=build /tc/arm-kobo-linux-gnueabihf /tc/arm-kobo-linux-gnueabihf
COPY check.sh /tc/arm-kobo-linux-gnueabihf/check.sh
# CMake 3.21 and later read CMAKE_TOOLCHAIN_FILE from the environment, so a
# plain cmake call cross-compiles here.
ENV PATH=/tc/arm-kobo-linux-gnueabihf/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    CROSS_TC=arm-kobo-linux-gnueabihf \
    CMAKE_TOOLCHAIN_FILE=/tc/arm-kobo-linux-gnueabihf/toolchain.cmake \
    PKG_CONFIG=arm-kobo-linux-gnueabihf-pkg-config \
    LC_ALL=C.UTF-8
RUN /tc/arm-kobo-linux-gnueabihf/check.sh && rm -rf /tmp/*
CMD ["/bin/bash"]
