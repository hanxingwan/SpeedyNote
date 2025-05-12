#!/bin/bash

set -e

echo "📦 Installing dependencies via Homebrew..."
brew install cmake ninja pkg-config qt6 boost

echo "📥 Cloning Poppler source..."
git clone https://gitlab.freedesktop.org/poppler/poppler.git
cd poppler
mkdir -p build && cd build

echo "🔧 Configuring build with Qt6 support..."
cmake .. -G Ninja \
    -DCMAKE_INSTALL_PREFIX=/opt/poppler-qt6 \
    -DENABLE_QT6=ON \
    -DENABLE_QT5=OFF \
    -DENABLE_GLIB=OFF \
    -DENABLE_UNSTABLE_API_ABI_HEADERS=OFF

echo "⚙️ Building..."
ninja

echo "📂 Installing to /opt/poppler-qt6 (requires sudo)..."
sudo ninja install

echo "✅ Poppler with Qt6 support installed at /opt/poppler-qt6"

echo "📁 Headers: /opt/poppler-qt6/include/poppler"
echo "📚 Libraries: /opt/poppler-qt6/lib"
