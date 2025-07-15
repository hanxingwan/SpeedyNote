# Maintainer: SpeedyNote Team
pkgname=speedynote
pkgver=0.6.1
pkgrel=1
pkgdesc="A fast note-taking application with PDF annotation support and controller input"
arch=('x86_64')
url="https://github.com/speedynote/speedynote"
license=('GPL3')
depends=(
    'qt6-base'
    'qt6-multimedia'
    'poppler-qt6'
    'sdl2-compat'
)
makedepends=(
    'cmake'
    'qt6-tools'
    'pkgconf'
)
optdepends=(
    'qt6-wayland: Wayland support'
    'qt6-imageformats: Additional image format support'
)
source=("${pkgname}-${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$srcdir"
    
    # Create build directory
    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    
    # Build the project
    cmake --build build --parallel
}

check() {
    cd "$srcdir"
    
    # Basic check to ensure executable was built
    test -f "build/NoteApp"
}

package() {
    cd "$srcdir"
    
    # Install the main executable
    install -Dm755 "build/NoteApp" "$pkgdir/usr/bin/speedynote"
    
    # Install desktop file
    install -Dm644 /dev/stdin "$pkgdir/usr/share/applications/speedynote.desktop" << EOFDESKTOP
[Desktop Entry]
Version=1.0
Type=Application
Name=SpeedyNote
Comment=Fast note-taking application with PDF annotation support
Exec=speedynote
Icon=speedynote
Terminal=false
StartupNotify=true
Categories=Office;Education;
Keywords=notes;pdf;annotation;writing;
EOFDESKTOP

    # Install icon (using the main icon from resources)
    install -Dm644 "resources/icons/mainicon.png" "$pkgdir/usr/share/pixmaps/speedynote.png"
    
    # Install translation files if they exist
    if [ -d "build" ] && [ "$(ls -A build/*.qm 2>/dev/null)" ]; then
        install -dm755 "$pkgdir/usr/share/speedynote/translations"
        install -m644 build/*.qm "$pkgdir/usr/share/speedynote/translations/"
    fi
    
    # Install documentation
    install -Dm644 README.md "$pkgdir/usr/share/doc/$pkgname/README.md"
    if [ -f "README-Linux.md" ]; then
        install -Dm644 README-Linux.md "$pkgdir/usr/share/doc/$pkgname/README-Linux.md"
    fi
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
