#!/bin/bash
# Smoke test for the toolchain: the compiler, pkg-config, qmake6 and CMake
# each produce ARM hard-float code linked against Kobo's Qt. The image build
# runs it last; run it again inside the image when something looks wrong.
set -euo pipefail
tc=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

echo "== compiler"
echo 'int main(void) { return 0; }' | arm-kobo-linux-gnueabihf-gcc -x c - -o "$work/c"
file "$work/c" | grep -q 'ARM, EABI5'
arm-kobo-linux-gnueabihf-readelf -A "$work/c" | grep -q 'Tag_ABI_VFP_args: VFP registers'
arm-kobo-linux-gnueabihf-readelf -A "$work/c" | grep -q 'Tag_Advanced_SIMD_arch: NEONv1'

echo "== headers match the firmware configuration"
gui=$tc/qt6/include/QtGui
core=$tc/qt6/include/QtCore
for feature in opengles2 egl fontconfig; do
    grep -q "^#define QT_FEATURE_$feature 1" "$gui/qtgui-config.h"
done
grep -q '^#define QT_FEATURE_xkbcommon 1' "$gui/6.5.2/QtGui/private/qtgui-config_p.h"
grep -q '^#define QT_FEATURE_glib 1' "$core/qtcore-config.h"
grep -q '^#define QT_FEATURE_icu 1' "$core/6.5.2/QtCore/private/qtcore-config_p.h"

echo "== pkg-config"
arm-kobo-linux-gnueabihf-pkg-config --cflags --libs Qt6Widgets | grep -q -- "-lQt6Widgets"
printf '#include <QString>\n#include <cstdio>\nint main() { QString s("test"); printf("%%s", qPrintable(s)); }\n' > "$work/pc.cpp"
arm-kobo-linux-gnueabihf-g++ -fPIC "$work/pc.cpp" -o "$work/pc" $(arm-kobo-linux-gnueabihf-pkg-config --cflags --libs Qt6Core)
arm-kobo-linux-gnueabihf-readelf -d "$work/pc" | grep -q 'libQt6Core.so.6'

echo "== qmake6"
mkdir "$work/qmake"
cp "$work/pc.cpp" "$work/qmake/main.cpp"
printf 'QT = core\nTARGET = q\nSOURCES = main.cpp\n' > "$work/qmake/q.pro"
(cd "$work/qmake" && qmake6 q.pro && make -s)
arm-kobo-linux-gnueabihf-readelf -d "$work/qmake/q" | grep -q 'libQt6Core.so.6'
file "$work/qmake/q" | grep -q 'ARM, EABI5'

echo "== cmake with a plugin that needs moc and private headers"
mkdir "$work/cmake"
cat > "$work/cmake/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(check LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
add_library(checkplugin MODULE plugin.cpp)
target_link_libraries(checkplugin PRIVATE Qt6::Widgets Qt6::GuiPrivate)
EOF
cat > "$work/cmake/plugin.cpp" <<'EOF'
#include <QtGui/qpa/qplatformintegrationplugin.h>
#include <QtWidgets/QWidget>
class CheckPlugin : public QPlatformIntegrationPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid)
public:
    // Widgets stays in NEEDED only when something uses it; the linker
    // drops unused libraries by default.
    QPlatformIntegration *create(const QString &, const QStringList &) override { return QWidget::mouseGrabber() ? nullptr : nullptr; }
};
#include "plugin.moc"
EOF
cmake -S "$work/cmake" -B "$work/cmake/build" -G Ninja >/dev/null
cmake --build "$work/cmake/build" >/dev/null
arm-kobo-linux-gnueabihf-readelf -d "$work/cmake/build/libcheckplugin.so" | grep -q 'libQt6Widgets.so.6'
file "$work/cmake/build/libcheckplugin.so" | grep -q 'ARM, EABI5'
echo "toolchain ok"
