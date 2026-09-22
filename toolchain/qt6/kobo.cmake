# CMake toolchain file for the Kobo Qt6 firmware cross compiler.
#
# Qt's generated toolchain.cmake next to this file chainloads it and adds the
# Qt prefix, so most projects use that one. Use this file directly for a
# project without Qt.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

get_filename_component(KOBO_TOOLCHAIN_ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(KOBO_CROSS_PREFIX "${KOBO_TOOLCHAIN_ROOT}/bin/arm-kobo-linux-gnueabihf-")

set(CMAKE_C_COMPILER "${KOBO_CROSS_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${KOBO_CROSS_PREFIX}g++")
set(CMAKE_ASM_COMPILER "${KOBO_CROSS_PREFIX}gcc")
set(CMAKE_AR "${KOBO_CROSS_PREFIX}gcc-ar" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "${KOBO_CROSS_PREFIX}gcc-ranlib" CACHE FILEPATH "Ranlib")
set(CMAKE_NM "${KOBO_CROSS_PREFIX}gcc-nm" CACHE FILEPATH "Nm")
set(CMAKE_STRIP "${KOBO_CROSS_PREFIX}strip" CACHE FILEPATH "Strip")
set(CMAKE_OBJCOPY "${KOBO_CROSS_PREFIX}objcopy" CACHE FILEPATH "Objcopy")
set(CMAKE_OBJDUMP "${KOBO_CROSS_PREFIX}objdump" CACHE FILEPATH "Objdump")
set(PKG_CONFIG_EXECUTABLE "${KOBO_CROSS_PREFIX}pkg-config" CACHE FILEPATH "pkg-config for the device")

# The sysroot is the image itself, laid out the Debian multiarch way: the
# device's C library in /usr/arm-linux-gnueabihf, its other libraries in
# /usr/lib/arm-linux-gnueabihf. The compiler and linker already search both,
# so CMake only needs the architecture directory name to find libraries and
# the Qt prefix to find packages. Programs are always host programs.
set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)
list(APPEND CMAKE_FIND_ROOT_PATH "${KOBO_TOOLCHAIN_ROOT}/qt6" /usr/arm-linux-gnueabihf)
list(APPEND CMAKE_PREFIX_PATH "${KOBO_TOOLCHAIN_ROOT}/qt6")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
