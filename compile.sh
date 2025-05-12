#!/bin/bash

# Change to your project directory (adjust this if needed)
# cd "$(dirname "$0")"
POPPLER_PATH="/opt/poppler-qt6"
SDL_PATH="$(brew --prefix sdl2)/lib"
# Clean previous build
rm -rf build
mkdir build
cd build

# Run CMake (assumes Qt6 installed via Homebrew and Poppler too)
export PKG_CONFIG_PATH="${POPPLER_PATH}/lib/pkgconfig:$PKG_CONFIG_PATH"

cmake -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) -DCMAKE_BUILD_TYPE=Release ..

# Build the app
make -j$(sysctl -n hw.logicalcpu)

# Deploy the Qt dependencies (only works with tools like macdeployqt, optional)
# If installed: /opt/homebrew/opt/qt@6/bin/macdeployqt NoteApp.app

# Run the app
./SpeedyNote
cd ..
