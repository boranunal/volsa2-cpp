#!/usr/bin/env bash
# ==============================================================================
# VolSa 2 - Linux Release Build & Packaging Script
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$ROOT_DIR"

echo "=== [1/6] Configuring Release Build ==="
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local/Qt-6.9.1

echo "=== [2/6] Compiling Optimized Binaries ==="
cmake --build build -j"$(nproc)"

echo "=== [3/6] Running Test Suite ==="
ctest --test-dir build --output-on-failure

echo "=== [4/6] Generating CPack Distribution Packages ==="
cpack --config build/CPackConfig.cmake

echo "=== [5/6] Building Standalone AppImage ==="
mkdir -p build/AppDir/usr/bin build/AppDir/usr/lib build/AppDir/usr/plugins/platforms

# Copy & strip binaries
cp build/gui/volsa2-gui build/AppDir/usr/bin/
cp build/volsa2-cli build/AppDir/usr/bin/
strip build/AppDir/usr/bin/volsa2-gui build/AppDir/usr/bin/volsa2-cli

# Copy Qt core libraries
cp -d /usr/local/Qt-6.9.1/lib/libQt6Core.so* build/AppDir/usr/lib/
cp -d /usr/local/Qt-6.9.1/lib/libQt6Gui.so* build/AppDir/usr/lib/
cp -d /usr/local/Qt-6.9.1/lib/libQt6Widgets.so* build/AppDir/usr/lib/
cp -d /usr/local/Qt-6.9.1/lib/libQt6DBus.so* build/AppDir/usr/lib/

# Copy Qt platform plugins (Wayland, EGLFS, XCB)
cp -r /usr/local/Qt-6.9.1/plugins/platforms/* build/AppDir/usr/plugins/platforms/ 2>/dev/null || true
cp -d /usr/lib/x86_64-linux-gnu/libQt6XcbQpa.so* build/AppDir/usr/lib/ 2>/dev/null || true
cp /usr/lib/x86_64-linux-gnu/qt6/plugins/platforms/libqxcb.so build/AppDir/usr/plugins/platforms/ 2>/dev/null || true

# Setup AppRun launcher
cat << 'EOF' > build/AppDir/AppRun
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=$(dirname "$SELF")
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HERE/usr/plugins"

if [ "$1" = "cli" ] || [ "$(basename "$0")" = "volsa2-cli" ]; then
    if [ "$1" = "cli" ]; then shift; fi
    exec "$HERE/usr/bin/volsa2-cli" "$@"
fi

exec "$HERE/usr/bin/volsa2-gui" "$@"
EOF
chmod +x build/AppDir/AppRun

# Setup Desktop Entry & Icon
cat << 'EOF' > build/AppDir/volsa2.desktop
[Desktop Entry]
Type=Application
Name=VolSa 2
GenericName=Sample Librarian & Sequencer Manager
Comment=Digital sample librarian for KORG Volca Sample 2
Exec=volsa2-gui %F
Icon=volsa2
Categories=AudioVideo;Audio;Sequencer;
Terminal=false
EOF
cp volsa2.png build/AppDir/
[ -f volsa2-icon.png ] && cp volsa2-icon.png build/AppDir/volsa2.png
cp build/AppDir/volsa2.desktop build/AppDir/
cp build/AppDir/volsa2.png build/AppDir/.DirIcon

APPIMAGETOOL="/tmp/appimagetool"
if [ ! -f "$APPIMAGETOOL" ]; then
    curl -L -o "$APPIMAGETOOL" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x "$APPIMAGETOOL"
fi

ARCH=x86_64 "$APPIMAGETOOL" build/AppDir VolSa2-2.0.0-x86_64.AppImage

echo "=== [6/6] Assembling Release Artifacts ==="
mkdir -p release/bin
cp build/gui/volsa2-gui release/bin/
cp build/volsa2-cli release/bin/
strip release/bin/volsa2-gui release/bin/volsa2-cli

cp VolSa2-2.0.0-x86_64.AppImage release/
cp volsa2-2.0.0-linux-x86_64.sh release/
cp volsa2-2.0.0-linux-x86_64.tar.gz release/
cp build/AppDir/volsa2.desktop release/
cp volsa2.png release/
[ -f volsa2-icon.png ] && cp volsa2-icon.png release/
cp install.sh release/

cat << 'EOF' > release/99-korg-volca.rules
# KORG Volca Sample 2 USB MIDI Interface
SUBSYSTEM=="sound", ATTRS{idVendor}=="0944", MODE="0666", GROUP="audio"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0944", MODE="0666", GROUP="audio"
EOF

cd release
sha256sum VolSa2-2.0.0-x86_64.AppImage volsa2-2.0.0-linux-x86_64.sh volsa2-2.0.0-linux-x86_64.tar.gz bin/volsa2-gui bin/volsa2-cli install.sh > SHA256SUMS
cd ..

echo "=== Build & Packaging Complete! ==="
ls -lh release/
