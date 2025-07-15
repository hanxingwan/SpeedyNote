#!/bin/bash
set -e

# SpeedyNote Packaging Automation Script
# This script automates the entire process of creating an Arch Linux package

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
PKGNAME="speedynote"
PKGVER="0.6.1"
PKGREL="1"

echo -e "${BLUE}SpeedyNote Packaging Automation Script${NC}"
echo "=========================================="
echo

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if we're in the right directory
check_project_directory() {
    if [[ ! -f "CMakeLists.txt" ]] || [[ ! -f "Main.cpp" ]]; then
        echo -e "${RED}Error: This doesn't appear to be the SpeedyNote project directory${NC}"
        echo "Please run this script from the SpeedyNote project root directory"
        exit 1
    fi
}

# Function to check for required packaging tools
check_packaging_dependencies() {
    echo -e "${YELLOW}Checking packaging dependencies...${NC}"
    
    MISSING_DEPS=()
    
    if ! command_exists makepkg; then
        MISSING_DEPS+=("base-devel")
    fi
    
    if ! command_exists cmake; then
        MISSING_DEPS+=("cmake")
    fi
    
    if [[ ${#MISSING_DEPS[@]} -ne 0 ]]; then
        echo -e "${RED}Missing packaging dependencies:${NC}"
        for dep in "${MISSING_DEPS[@]}"; do
            echo "  - $dep"
        done
        echo
        echo -e "${YELLOW}Install with: sudo pacman -S ${MISSING_DEPS[*]}${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}All packaging dependencies are available!${NC}"
}

# Function to create/update PKGBUILD
create_pkgbuild() {
    echo -e "${YELLOW}Creating PKGBUILD...${NC}"
    
    cat > PKGBUILD << EOF
# Maintainer: SpeedyNote Team
pkgname=${PKGNAME}
pkgver=${PKGVER}
pkgrel=${PKGREL}
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
source=("\${pkgname}-\${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cd "\$srcdir"
    
    # Create build directory
    cmake -B build -S . \\
        -DCMAKE_BUILD_TYPE=Release \\
        -DCMAKE_INSTALL_PREFIX=/usr
    
    # Build the project
    cmake --build build --parallel
}

check() {
    cd "\$srcdir"
    
    # Basic check to ensure executable was built
    test -f "build/NoteApp"
}

package() {
    cd "\$srcdir"
    
    # Install the main executable
    install -Dm755 "build/NoteApp" "\$pkgdir/usr/bin/speedynote"
    
    # Install desktop file
    install -Dm644 /dev/stdin "\$pkgdir/usr/share/applications/speedynote.desktop" << EOFDESKTOP
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
    install -Dm644 "resources/icons/mainicon.png" "\$pkgdir/usr/share/pixmaps/speedynote.png"
    
    # Install translation files if they exist
    if [ -d "build" ] && [ "\$(ls -A build/*.qm 2>/dev/null)" ]; then
        install -dm755 "\$pkgdir/usr/share/speedynote/translations"
        install -m644 build/*.qm "\$pkgdir/usr/share/speedynote/translations/"
    fi
    
    # Install documentation
    install -Dm644 README.md "\$pkgdir/usr/share/doc/\$pkgname/README.md"
    if [ -f "README-Linux.md" ]; then
        install -Dm644 README-Linux.md "\$pkgdir/usr/share/doc/\$pkgname/README-Linux.md"
    fi
    install -Dm644 LICENSE "\$pkgdir/usr/share/licenses/\$pkgname/LICENSE"
}
EOF
    
    echo -e "${GREEN}PKGBUILD created successfully${NC}"
}

# Function to create source tarball
create_source_tarball() {
    echo -e "${YELLOW}Creating source tarball...${NC}"
    
    # Clean any existing build artifacts
    rm -rf build
    rm -f "${PKGNAME}-${PKGVER}.tar.gz"
    
    # Create the tarball excluding unnecessary files
    tar -czf "${PKGNAME}-${PKGVER}.tar.gz" \
        --exclude=build \
        --exclude=.git* \
        --exclude="*.tar.gz" \
        --exclude="*.pkg.tar.zst" \
        --exclude=pkg \
        --exclude=src \
        .
    
    echo -e "${GREEN}Source tarball created: ${PKGNAME}-${PKGVER}.tar.gz${NC}"
}

# Function to generate .SRCINFO
generate_srcinfo() {
    echo -e "${YELLOW}Generating .SRCINFO...${NC}"
    makepkg --printsrcinfo > .SRCINFO
    echo -e "${GREEN}.SRCINFO generated successfully${NC}"
}

# Function to build package
build_package() {
    echo -e "${YELLOW}Building package with makepkg...${NC}"
    
    # Build the package
    makepkg -f
    
    if [[ $? -eq 0 ]]; then
        echo -e "${GREEN}Package built successfully!${NC}"
        
        # Find the created package
        PACKAGE_FILE=$(ls ${PKGNAME}-${PKGVER}-${PKGREL}-*.pkg.tar.zst 2>/dev/null | head -1)
        if [[ -n "$PACKAGE_FILE" ]]; then
            echo -e "${CYAN}Package file: $PACKAGE_FILE${NC}"
            echo -e "${CYAN}Package size: $(du -h "$PACKAGE_FILE" | cut -f1)${NC}"
        fi
    else
        echo -e "${RED}Package build failed!${NC}"
        exit 1
    fi
}

# Function to test package installation
test_package() {
    PACKAGE_FILE=$(ls ${PKGNAME}-${PKGVER}-${PKGREL}-*.pkg.tar.zst 2>/dev/null | head -1)
    
    if [[ -z "$PACKAGE_FILE" ]]; then
        echo -e "${RED}No package file found to test${NC}"
        return 1
    fi
    
    echo -e "${YELLOW}Testing package installation...${NC}"
    echo -e "${CYAN}Package: $PACKAGE_FILE${NC}"
    
    # Check if package is already installed
    if pacman -Qi "$PKGNAME" >/dev/null 2>&1; then
        echo -e "${YELLOW}$PKGNAME is already installed. Remove it first? (y/N)${NC}"
        read -r response
        if [[ "$response" =~ ^[Yy]$ ]]; then
            sudo pacman -R "$PKGNAME"
        else
            echo -e "${YELLOW}Skipping installation test${NC}"
            return 0
        fi
    fi
    
    echo -e "${YELLOW}Installing package...${NC}"
    sudo pacman -U "$PACKAGE_FILE"
    
    if [[ $? -eq 0 ]]; then
        echo -e "${GREEN}Package installed successfully!${NC}"
        echo
        echo -e "${CYAN}Verifying installation:${NC}"
        echo -e "Executable: $(which speedynote 2>/dev/null || echo 'NOT FOUND')"
        echo -e "Desktop file: $([[ -f /usr/share/applications/speedynote.desktop ]] && echo 'FOUND' || echo 'NOT FOUND')"
        echo -e "Icon: $([[ -f /usr/share/pixmaps/speedynote.png ]] && echo 'FOUND' || echo 'NOT FOUND')"
    else
        echo -e "${RED}Package installation failed!${NC}"
        return 1
    fi
}

# Function to clean up
cleanup() {
    echo -e "${YELLOW}Cleaning up build artifacts...${NC}"
    rm -rf pkg src
    echo -e "${GREEN}Cleanup complete${NC}"
}

# Function to show package information
show_package_info() {
    echo
    echo -e "${CYAN}=== Package Information ===${NC}"
    echo -e "Package name: ${PKGNAME}"
    echo -e "Version: ${PKGVER}-${PKGREL}"
    echo -e "Architecture: x86_64"
    
    PACKAGE_FILE=$(ls ${PKGNAME}-${PKGVER}-${PKGREL}-*.pkg.tar.zst 2>/dev/null | head -1)
    if [[ -n "$PACKAGE_FILE" ]]; then
        echo -e "Package file: $PACKAGE_FILE"
        echo -e "Package size: $(du -h "$PACKAGE_FILE" | cut -f1)"
    fi
    
    echo
    echo -e "${CYAN}=== Installation Commands ===${NC}"
    echo -e "Install package: ${YELLOW}sudo pacman -U $PACKAGE_FILE${NC}"
    echo -e "Remove package: ${YELLOW}sudo pacman -R $PKGNAME${NC}"
    echo
    echo -e "${CYAN}=== For AUR Distribution ===${NC}"
    echo -e "1. Upload PKGBUILD and .SRCINFO to AUR repository"
    echo -e "2. Users can then install with: ${YELLOW}yay -S $PKGNAME${NC}"
}

# Main execution
main() {
    echo -e "${BLUE}Starting packaging process...${NC}"
    
    # Step 1: Verify environment
    check_project_directory
    check_packaging_dependencies
    
    # Step 2: Create packaging files
    create_pkgbuild
    create_source_tarball
    generate_srcinfo
    
    # Step 3: Build package
    build_package
    
    # Step 4: Ask about testing
    echo
    read -p "Do you want to test the package installation? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        test_package
    fi
    
    # Step 5: Cleanup
    cleanup
    
    # Step 6: Show final information
    show_package_info
    
    echo
    echo -e "${GREEN}Packaging process completed successfully!${NC}"
}

# Run main function
main "$@" 