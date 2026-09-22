#!/bin/bash
# Create the prefixed tools in <toolchain>/bin around Ubuntu's cross compiler.
set -euo pipefail
tc=$1
bin=$tc/bin
mkdir -p "$bin"

# The CPU flags Kobo builds with. They come first so a caller's own flags win.
# Kobo's own qmake spec used -mcpu=cortex-a9 for its i.MX6 devices; armv7-a
# with NEON covers those and the MediaTek devices running Qt6 firmware.
flags='-march=armv7-a -mfpu=neon -mfloat-abi=hard -mthumb'
for tool in gcc g++ cpp; do
    cat > "$bin/arm-kobo-linux-gnueabihf-$tool" <<EOF
#!/bin/sh
exec /usr/bin/arm-linux-gnueabihf-$tool-11 $flags "\$@"
EOF
    chmod 755 "$bin/arm-kobo-linux-gnueabihf-$tool"
done
ln -sf arm-kobo-linux-gnueabihf-gcc "$bin/arm-kobo-linux-gnueabihf-cc"
ln -sf arm-kobo-linux-gnueabihf-g++ "$bin/arm-kobo-linux-gnueabihf-c++"

for tool in gcc-ar gcc-nm gcc-ranlib gcov gcov-dump gcov-tool lto-dump; do
    ln -sf "/usr/bin/arm-linux-gnueabihf-$tool-11" "$bin/arm-kobo-linux-gnueabihf-$tool"
done
for tool in addr2line ar as c++filt dwp elfedit gprof ld ld.bfd ld.gold nm objcopy objdump ranlib readelf size strings strip; do
    ln -sf "/usr/bin/arm-linux-gnueabihf-$tool" "$bin/arm-kobo-linux-gnueabihf-$tool"
done

# Qt's .pc files come first so its Qt6*.pc win over anything else; the
# multiarch directory holds the system libraries the firmware links.
cat > "$bin/arm-kobo-linux-gnueabihf-pkg-config" <<EOF
#!/bin/sh
export PKG_CONFIG_PATH=
export PKG_CONFIG_LIBDIR=$tc/qt6/lib/pkgconfig:/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig
exec /usr/bin/pkg-config "\$@"
EOF
chmod 755 "$bin/arm-kobo-linux-gnueabihf-pkg-config"
