#!/bin/bash
set -e

APP_NAME="SpeedyNote"
APP_BUNDLE="build/${APP_NAME}.app"
QT_PATH="$(brew --prefix qt6)"
POPPLER_PATH="/opt/poppler-qt6"
SDL_PATH="$(brew --prefix sdl2)/lib"

echo "📦 Cleaning build directory..."
rm -rf build
mkdir build
cd build

echo "🔨 Running CMake..."
export PKG_CONFIG_PATH="${POPPLER_PATH}/lib/pkgconfig:$PKG_CONFIG_PATH"
cmake -DCMAKE_PREFIX_PATH="${QT_PATH}" -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.logicalcpu)

cd ..
echo "📁 Preparing .app bundle..."
rm -rf "${APP_BUNDLE}/Contents/Frameworks"
"${QT_PATH}/bin/macdeployqt6" "${APP_BUNDLE}" -verbose=3 -executable="${APP_BUNDLE}/Contents/MacOS/${APP_NAME}" || true

echo "📎 Copying Poppler and SDL2..."
cp "${POPPLER_PATH}"/lib/libpoppler*.dylib "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || echo "⚠️ Poppler not found?"
cp "${SDL_PATH}/libSDL2.dylib" "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || echo "⚠️ SDL2 not found?"

echo "🔧 Adding missing QtDBus.framework..."
QT_DBUS_FRAMEWORK="${QT_PATH}/lib/QtDBus.framework"
if [ -d "$QT_DBUS_FRAMEWORK" ]; then
    cp -R "$QT_DBUS_FRAMEWORK" "${APP_BUNDLE}/Contents/Frameworks/"
    install_name_tool -id "@rpath/QtDBus.framework/Versions/A/QtDBus" "${APP_BUNDLE}/Contents/Frameworks/QtDBus.framework/Versions/A/QtDBus"
else
    echo "⚠️ QtDBus.framework not found!"
fi

echo "➕ Ensuring @rpath is set for Qt frameworks..."
install_name_tool -add_rpath "@loader_path/../Frameworks" "${APP_BUNDLE}/Contents/MacOS/${APP_NAME}" || true

echo "🧹 Cleaning stale Qt rpaths..."
install_name_tool -delete_rpath "${QT_PATH}/lib" "${APP_BUNDLE}/Contents/MacOS/${APP_NAME}" || true

echo "✅ Done. Launch with:"
echo "env -u DYLD_LIBRARY_PATH -u DYLD_FRAMEWORK_PATH ${APP_BUNDLE}/Contents/MacOS/${APP_NAME}"
