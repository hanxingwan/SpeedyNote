#!/bin/bash
set -e

# SpeedyNote Multi-Distribution Packaging Script
# This script automates the process of creating packages for multiple Linux distributions

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
PKGNAME="speedynote"
PKGVER="0.6.2"
PKGREL="1"
MAINTAINER="SpeedyNote Team"
DESCRIPTION="A fast note-taking application with PDF annotation support and controller input"
URL="https://github.com/alpha-liu-01/SpeedyNote"
LICENSE="MIT"

# Default values
DISTRO=""
PACKAGE_FORMATS=()
AUTO_DETECT=true

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  --deb, -deb       Create .deb package for Debian/Ubuntu"
    echo "  --rpm, -rpm       Create .rpm package for Red Hat/Fedora/SUSE"
    echo "  --arch, -arch     Create .pkg.tar.zst package for Arch Linux"
    echo "  --apk, -apk       Create .apk package for Alpine Linux"
    echo "  --all             Create packages for all supported distributions"
    echo "  --help, -h        Show this help message"
    echo
    echo "You can specify multiple formats: $0 --deb --rpm --arch"
    echo "If no option is specified, the script will auto-detect the distribution."
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --deb|-deb)
            PACKAGE_FORMATS+=("deb")
            AUTO_DETECT=false
            shift
            ;;
        --rpm|-rpm)
            PACKAGE_FORMATS+=("rpm")
            AUTO_DETECT=false
            shift
            ;;
        --arch|-arch)
            PACKAGE_FORMATS+=("arch")
            AUTO_DETECT=false
            shift
            ;;
        --apk|-apk)
            PACKAGE_FORMATS+=("apk")
            AUTO_DETECT=false
            shift
            ;;
        --all)
            PACKAGE_FORMATS=("deb" "rpm" "arch" "apk")
            AUTO_DETECT=false
            shift
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

echo -e "${BLUE}SpeedyNote Multi-Distribution Packaging Script${NC}"
echo "=============================================="
echo

# Function to detect distribution
detect_distribution() {
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        case $ID in
            ubuntu|debian|linuxmint|pop)
                echo "deb"
                ;;
            fedora|rhel|centos|rocky|almalinux)
                echo "rpm"
                ;;
            opensuse*|sles)
                echo "rpm"
                ;;
            arch|manjaro|endeavouros|garuda)
                echo "arch"
                ;;
            alpine)
                echo "apk"
                ;;
            *)
                echo "unknown"
                ;;
        esac
    else
        echo "unknown"
    fi
}

# Auto-detect distribution if not specified
if [[ $AUTO_DETECT == true ]]; then
    DETECTED_DISTRO=$(detect_distribution)
    if [[ $DETECTED_DISTRO == "unknown" ]]; then
        echo -e "${RED}Unable to detect distribution. Please specify manually.${NC}"
        show_usage
        exit 1
    fi
    PACKAGE_FORMATS=("$DETECTED_DISTRO")
    echo -e "${YELLOW}Auto-detected distribution: $DETECTED_DISTRO${NC}"
else
    echo -e "${YELLOW}Target package formats: ${PACKAGE_FORMATS[*]}${NC}"
fi

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

# Function to get dependencies for each distribution
get_dependencies() {
    local format=$1
    case $format in
        deb)
            echo "libqt6core6t64, libqt6gui6t64, libqt6widgets6t64, libqt6multimedia6, libpoppler-qt6-3t64, libsdl2-2.0-0"
            ;;
        rpm)
            echo "qt6-qtbase, qt6-qtmultimedia, poppler-qt6, SDL2"
            ;;
        arch)
            echo "qt6-base, qt6-multimedia, poppler-qt6, sdl2-compat"
            ;;
        apk)
            echo "qt6-qtbase, qt6-qtmultimedia, poppler-qt6, sdl2"
            ;;
    esac
}

# Function to get build dependencies for each distribution
get_build_dependencies() {
    local format=$1
    case $format in
        deb)
            echo "cmake, make, pkg-config, qt6-base-dev, libqt6gui6t64, libqt6widgets6t64, qt6-multimedia-dev, qt6-tools-dev, libpoppler-qt6-dev, libsdl2-dev"
            ;;
        rpm)
            echo "cmake, make, pkgconf, qt6-qtbase-devel, qt6-qtmultimedia-devel, qt6-qttools-devel, poppler-qt6-devel, SDL2-devel"
            ;;
        arch)
            echo "cmake, make, pkgconf, qt6-base, qt6-multimedia, qt6-tools, poppler-qt6, sdl2-compat"
            ;;
        apk)
            echo "cmake, make, pkgconf, qt6-qtbase-dev, qt6-qtmultimedia-dev, qt6-qttools-dev, poppler-qt5-dev, sdl2-dev"
            ;;
    esac
}

# Function to check packaging dependencies
check_packaging_dependencies() {
    local format=$1
    echo -e "${YELLOW}Checking packaging dependencies for $format...${NC}"
    
    MISSING_DEPS=()
    
    case $format in
        deb)
            if ! command_exists dpkg-deb; then
                MISSING_DEPS+=("dpkg-dev")
            fi
            if ! command_exists debuild; then
                MISSING_DEPS+=("devscripts")
            fi
            ;;
        rpm)
            if ! command_exists rpmbuild; then
                MISSING_DEPS+=("rpm-build")
            fi
            if ! command_exists rpmspec; then
                MISSING_DEPS+=("rpm-devel")
            fi
            ;;
        arch)
            if ! command_exists makepkg; then
                MISSING_DEPS+=("base-devel")
            fi
            ;;
        apk)
            if ! command_exists abuild; then
                MISSING_DEPS+=("alpine-sdk")
            fi
            if ! command_exists abuild-sign; then
                MISSING_DEPS+=("abuild")
            fi
            ;;
    esac
    
    if [[ ${#MISSING_DEPS[@]} -ne 0 ]]; then
        echo -e "${RED}Missing packaging dependencies for $format:${NC}"
        for dep in "${MISSING_DEPS[@]}"; do
            echo "  - $dep"
        done
        echo
        case $format in
            deb)
                echo -e "${YELLOW}Install with: sudo apt-get install ${MISSING_DEPS[*]}${NC}"
                ;;
            rpm)
                echo -e "${YELLOW}Install with: sudo dnf install ${MISSING_DEPS[*]}${NC}"
                ;;
            arch)
                echo -e "${YELLOW}Install with: sudo pacman -S ${MISSING_DEPS[*]}${NC}"
                ;;
            apk)
                echo -e "${YELLOW}Install with: sudo apk add ${MISSING_DEPS[*]}${NC}"
                ;;
        esac
        return 1
    fi
    
    echo -e "${GREEN}All packaging dependencies are available for $format!${NC}"
    return 0
}

# Function to build the project
build_project() {
    echo -e "${YELLOW}Building SpeedyNote...${NC}"
    
    # Clean and create build directory
    rm -rf build
    mkdir -p build
    
    # Compile translations if lrelease is available
    if command_exists lrelease; then
        echo -e "${YELLOW}Compiling translation files...${NC}"
        lrelease ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts
        cp resources/translations/*.qm build/ 2>/dev/null || true
    fi
    
    cd build
    
    # Configure and build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
    make -j$(nproc)
    
    if [[ ! -f "NoteApp" ]]; then
        echo -e "${RED}Build failed: NoteApp executable not found${NC}"
        exit 1
    fi
    
    cd ..
    echo -e "${GREEN}Build successful!${NC}"
}

# Function to create desktop file with PDF and SPN MIME type association
create_desktop_file() {
    local desktop_file="$1"
    cat > "$desktop_file" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=SpeedyNote
Comment=$DESCRIPTION
Exec=speedynote %F
Icon=speedynote
Terminal=false
StartupNotify=true
Categories=Office;Education;
Keywords=notes;pdf;annotation;writing;package;
MimeType=application/pdf;application/x-speedynote-package;
EOF
}

# Function to create MIME type definition for .spn files
create_mime_xml() {
    local mime_file="$1"
    cat > "$mime_file" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
    <mime-type type="application/x-speedynote-package">
        <comment>SpeedyNote Package</comment>
        <comment xml:lang="es">Paquete de SpeedyNote</comment>
        <comment xml:lang="fr">Package SpeedyNote</comment>
        <comment xml:lang="zh">SpeedyNote 包</comment>
        <icon name="speedynote"/>
        <glob pattern="*.spn"/>
        <magic priority="50">
            <match type="string" offset="0" value="Contents"/>
        </magic>
    </mime-type>
</mime-info>
EOF
}

# Function to create DEB package
create_deb_package() {
    echo -e "${YELLOW}Creating DEB package...${NC}"
    
    PKG_DIR="debian-pkg"
    rm -rf "$PKG_DIR"
    mkdir -p "$PKG_DIR/DEBIAN"
    mkdir -p "$PKG_DIR/usr/bin"
    mkdir -p "$PKG_DIR/usr/share/applications"
    mkdir -p "$PKG_DIR/usr/share/pixmaps"
    mkdir -p "$PKG_DIR/usr/share/doc/$PKGNAME"
    mkdir -p "$PKG_DIR/usr/share/mime/packages"
    
    # Create control file
    cat > "$PKG_DIR/DEBIAN/control" << EOF
Package: $PKGNAME
Version: $PKGVER-$PKGREL
Architecture: amd64
Maintainer: $MAINTAINER
Depends: $(get_dependencies deb)
Section: editors
Priority: optional
Homepage: $URL
Description: $DESCRIPTION
 SpeedyNote is a fast and efficient note-taking application with PDF annotation
 support and controller input capabilities.
EOF
    
    # Create postinst script for MIME database update
    cat > "$PKG_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/bash
set -e

# Update MIME database
if [ -x /usr/bin/update-mime-database ]; then
    update-mime-database /usr/share/mime
fi

# Update desktop database
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q /usr/share/applications
fi

exit 0
EOF
    
    # Create postrm script for cleanup
    cat > "$PKG_DIR/DEBIAN/postrm" << 'EOF'
#!/bin/bash
set -e

if [ "$1" = "remove" ]; then
    # Update MIME database
    if [ -x /usr/bin/update-mime-database ]; then
        update-mime-database /usr/share/mime
    fi
    
    # Update desktop database
    if [ -x /usr/bin/update-desktop-database ]; then
        update-desktop-database -q /usr/share/applications
    fi
fi

exit 0
EOF
    
    chmod 755 "$PKG_DIR/DEBIAN/postinst"
    chmod 755 "$PKG_DIR/DEBIAN/postrm"
    
    # Install files
    cp build/NoteApp "$PKG_DIR/usr/bin/speedynote"
    cp resources/icons/mainicon.png "$PKG_DIR/usr/share/pixmaps/speedynote.png"
    cp README.md "$PKG_DIR/usr/share/doc/$PKGNAME/"
    
    # Create desktop file with PDF association
    create_desktop_file "$PKG_DIR/usr/share/applications/speedynote.desktop"
    
    # Create MIME type definition for .spn files
    create_mime_xml "$PKG_DIR/usr/share/mime/packages/application-x-speedynote-package.xml"
    
    # Build package
    dpkg-deb --build "$PKG_DIR" "${PKGNAME}_${PKGVER}-${PKGREL}_amd64.deb"
    
    echo -e "${GREEN}DEB package created: ${PKGNAME}_${PKGVER}-${PKGREL}_amd64.deb${NC}"
}

# Function to create RPM package
create_rpm_package() {
    echo -e "${YELLOW}Creating RPM package...${NC}"
    
    # Setup RPM build environment
    mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
    
    # Create source tarball with proper directory structure
    CURRENT_DIR=$(basename "$PWD")
    cd ..
    tar -czf ~/rpmbuild/SOURCES/${PKGNAME}-${PKGVER}.tar.gz \
        --exclude=build \
        --exclude=.git* \
        --exclude="*.rpm" \
        --exclude="*.deb" \
        --exclude="*.pkg.tar.zst" \
        --exclude="*.apk" \
        --transform "s|^${CURRENT_DIR}|${PKGNAME}-${PKGVER}|" \
        "${CURRENT_DIR}/"
    cd "${CURRENT_DIR}"
    
    # Create spec file
    cat > ~/rpmbuild/SPECS/${PKGNAME}.spec << EOF
Name:           $PKGNAME
Version:        $PKGVER
Release:        $PKGREL%{?dist}
Summary:        $DESCRIPTION
License:        $LICENSE
URL:            $URL
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  $(get_build_dependencies rpm)
Requires:       $(get_dependencies rpm)

%description
SpeedyNote is a fast and efficient note-taking application with PDF annotation
support and controller input capabilities.

%prep
%setup -q

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/share/applications
mkdir -p %{buildroot}/usr/share/pixmaps
mkdir -p %{buildroot}/usr/share/doc/%{name}
mkdir -p %{buildroot}/usr/share/mime/packages

install -m755 %{_vpath_builddir}/NoteApp %{buildroot}/usr/bin/speedynote
install -m644 resources/icons/mainicon.png %{buildroot}/usr/share/pixmaps/speedynote.png
install -m644 README.md %{buildroot}/usr/share/doc/%{name}/

cat > %{buildroot}/usr/share/applications/speedynote.desktop << EOFDESKTOP
[Desktop Entry]
Version=1.0
Type=Application
Name=SpeedyNote
Comment=$DESCRIPTION
Exec=speedynote %F
Icon=speedynote
Terminal=false
StartupNotify=true
Categories=Office;Education;
Keywords=notes;pdf;annotation;writing;package;
MimeType=application/pdf;application/x-speedynote-package;
EOFDESKTOP

cat > %{buildroot}/usr/share/mime/packages/application-x-speedynote-package.xml << EOFMIME
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
    <mime-type type="application/x-speedynote-package">
        <comment>SpeedyNote Package</comment>
        <comment xml:lang="es">Paquete de SpeedyNote</comment>
        <comment xml:lang="fr">Package SpeedyNote</comment>
        <comment xml:lang="zh">SpeedyNote 包</comment>
        <icon name="speedynote"/>
        <glob pattern="*.spn"/>
        <magic priority="50">
            <match type="string" offset="0" value="Contents"/>
        </magic>
    </mime-type>
</mime-info>
EOFMIME

%post
/usr/bin/update-desktop-database -q /usr/share/applications || :
/usr/bin/update-mime-database /usr/share/mime &> /dev/null || :

%postun
/usr/bin/update-desktop-database -q /usr/share/applications || :
/usr/bin/update-mime-database /usr/share/mime &> /dev/null || :

%files
/usr/bin/speedynote
/usr/share/applications/speedynote.desktop
/usr/share/pixmaps/speedynote.png
/usr/share/doc/%{name}/README.md
/usr/share/mime/packages/application-x-speedynote-package.xml

%changelog
* $(date '+%a %b %d %Y') $MAINTAINER - $PKGVER-$PKGREL
- Initial package with PDF file association support
EOF
    
    # Build RPM
    rpmbuild -ba ~/rpmbuild/SPECS/${PKGNAME}.spec
    
    # Copy to current directory
    cp ~/rpmbuild/RPMS/x86_64/${PKGNAME}-${PKGVER}-${PKGREL}.*.rpm .
    
    echo -e "${GREEN}RPM package created: ${PKGNAME}-${PKGVER}-${PKGREL}.*.rpm${NC}"
}

# Function to create Arch package
create_arch_package() {
    echo -e "${YELLOW}Creating Arch package...${NC}"
    
    # Create source tarball
    tar -czf "${PKGNAME}-${PKGVER}.tar.gz" \
        --exclude=build \
        --exclude=.git* \
        --exclude="*.tar.gz" \
        --exclude="*.pkg.tar.zst" \
        --exclude=pkg \
        --exclude=src \
        .
    
    # Create PKGBUILD
    cat > PKGBUILD << EOF
# Maintainer: $MAINTAINER
pkgname=$PKGNAME
pkgver=$PKGVER
pkgrel=$PKGREL
pkgdesc="$DESCRIPTION"
arch=('x86_64')
url="$URL"
license=('MIT')
depends=($(get_dependencies arch | tr ',' ' '))
makedepends=($(get_build_dependencies arch | tr ',' ' '))
source=("\${pkgname}-\${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cd "\$srcdir"
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build --parallel
}

package() {
    cd "\$srcdir"
    install -Dm755 "build/NoteApp" "\$pkgdir/usr/bin/speedynote"
    install -Dm644 "resources/icons/mainicon.png" "\$pkgdir/usr/share/pixmaps/speedynote.png"
    install -Dm644 README.md "\$pkgdir/usr/share/doc/\$pkgname/README.md"
    
    install -Dm644 /dev/stdin "\$pkgdir/usr/share/applications/speedynote.desktop" << EOFDESKTOP
[Desktop Entry]
Version=1.0
Type=Application
Name=SpeedyNote
Comment=$DESCRIPTION
Exec=speedynote %F
Icon=speedynote
Terminal=false
StartupNotify=true
Categories=Office;Education;
Keywords=notes;pdf;annotation;writing;package;
MimeType=application/pdf;application/x-speedynote-package;
EOFDESKTOP

    install -Dm644 /dev/stdin "\$pkgdir/usr/share/mime/packages/application-x-speedynote-package.xml" << EOFMIME
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
    <mime-type type="application/x-speedynote-package">
        <comment>SpeedyNote Package</comment>
        <comment xml:lang="es">Paquete de SpeedyNote</comment>
        <comment xml:lang="fr">Package SpeedyNote</comment>
        <comment xml:lang="zh">SpeedyNote 包</comment>
        <icon name="speedynote"/>
        <glob pattern="*.spn"/>
        <magic priority="50">
            <match type="string" offset="0" value="Contents"/>
        </magic>
    </mime-type>
</mime-info>
EOFMIME
}

post_install() {
    update-desktop-database -q
    update-mime-database /usr/share/mime
}

post_upgrade() {
    update-desktop-database -q
    update-mime-database /usr/share/mime
}

post_remove() {
    update-desktop-database -q
    update-mime-database /usr/share/mime
}
EOF
    
    # Build package
    makepkg -f
    
    echo -e "${GREEN}Arch package created: ${PKGNAME}-${PKGVER}-${PKGREL}-x86_64.pkg.tar.zst${NC}"
}

# Function to create Alpine package
create_apk_package() {
    echo -e "${YELLOW}Creating Alpine package...${NC}"
    
    # Create Alpine package structure
    mkdir -p alpine-pkg
    cd alpine-pkg
    
    # Create APKBUILD
    cat > APKBUILD << EOF
# Maintainer: $MAINTAINER
pkgname=$PKGNAME
pkgver=$PKGVER
pkgrel=$PKGREL
pkgdesc="$DESCRIPTION"
url="$URL"
arch="aarch64"
license="MIT"
depends="$(get_dependencies apk)"
makedepends="$(get_build_dependencies apk)"
source="\$pkgname-\$pkgver.tar.gz"
builddir="\$srcdir"
install="\$pkgname.post-install"

build() {
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build --parallel
}

package() {
    install -Dm755 "build/NoteApp" "\$pkgdir/usr/bin/speedynote"
    install -Dm644 "resources/icons/mainicon.png" "\$pkgdir/usr/share/pixmaps/speedynote.png"
    install -Dm644 README.md "\$pkgdir/usr/share/doc/\$pkgname/README.md"
    
    install -Dm644 /dev/stdin "\$pkgdir/usr/share/applications/speedynote.desktop" << EOFDESKTOP
[Desktop Entry]
Version=1.0
Type=Application
Name=SpeedyNote
Comment=$DESCRIPTION
Exec=speedynote %F
Icon=speedynote
Terminal=false
StartupNotify=true
Categories=Office;Education;
Keywords=notes;pdf;annotation;writing;package;
MimeType=application/pdf;application/x-speedynote-package;
EOFDESKTOP

    install -Dm644 /dev/stdin "\$pkgdir/usr/share/mime/packages/application-x-speedynote-package.xml" << EOFMIME
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
    <mime-type type="application/x-speedynote-package">
        <comment>SpeedyNote Package</comment>
        <comment xml:lang="es">Paquete de SpeedyNote</comment>
        <comment xml:lang="fr">Package SpeedyNote</comment>
        <comment xml:lang="zh">SpeedyNote 包</comment>
        <icon name="speedynote"/>
        <glob pattern="*.spn"/>
        <magic priority="50">
            <match type="string" offset="0" value="Contents"/>
        </magic>
    </mime-type>
</mime-info>
EOFMIME
}
EOF
    
    # Create post-install script
    cat > "${PKGNAME}.post-install" << 'EOF'
#!/bin/sh

# Update desktop and MIME databases
update-desktop-database -q /usr/share/applications 2>/dev/null || true
update-mime-database /usr/share/mime 2>/dev/null || true

exit 0
EOF
    
    # Create source tarball
    cd ..
    tar -czf "alpine-pkg/${PKGNAME}-${PKGVER}.tar.gz" \
        --exclude=build \
        --exclude=.git* \
        --exclude=alpine-pkg \
        .
    
    cd alpine-pkg
    
    # Build package
    abuild -r
    
    echo -e "${GREEN}Alpine package created in ~/packages/alpine-pkg/${PKGNAME}/ for .apk file"
}

# Function to clean up
cleanup() {
    echo -e "${YELLOW}Cleaning up build artifacts...${NC}"
    rm -rf build debian-pkg alpine-pkg
    rm -f "${PKGNAME}-${PKGVER}.tar.gz"
    echo -e "${GREEN}Cleanup complete${NC}"
}

# Function to show package information
show_package_info() {
    echo
    echo -e "${CYAN}=== Package Information ===${NC}"
    echo -e "Package name: ${PKGNAME}"
    echo -e "Version: ${PKGVER}-${PKGREL}"
    echo -e "Formats created: ${PACKAGE_FORMATS[*]}"
    echo -e "PDF file association: Enabled"
    echo
    
    echo -e "${CYAN}=== Created Packages ===${NC}"
    for format in "${PACKAGE_FORMATS[@]}"; do
        case $format in
            deb)
                if [[ -f "${PKGNAME}_${PKGVER}-${PKGREL}_amd64.deb" ]]; then
                    echo -e "DEB: ${PKGNAME}_${PKGVER}-${PKGREL}_amd64.deb ($(du -h "${PKGNAME}_${PKGVER}-${PKGREL}_amd64.deb" | cut -f1))"
                fi
                ;;
            rpm)
                RPM_FILE=$(ls ${PKGNAME}-${PKGVER}-${PKGREL}.*.rpm 2>/dev/null | head -1)
                if [[ -n "$RPM_FILE" ]]; then
                    echo -e "RPM: $RPM_FILE ($(du -h "$RPM_FILE" | cut -f1))"
                fi
                ;;
            arch)
                if [[ -f "${PKGNAME}-${PKGVER}-${PKGREL}-x86_64.pkg.tar.zst" ]]; then
                    echo -e "Arch: ${PKGNAME}-${PKGVER}-${PKGREL}-x86_64.pkg.tar.zst ($(du -h "${PKGNAME}-${PKGVER}-${PKGREL}-x86_64.pkg.tar.zst" | cut -f1))"
                fi
                ;;
            apk)
                echo -e "Alpine: Check ~/packages/alpine-pkg/${PKGNAME}/ for .apk file"
                ;;
        esac
    done
    
    echo
    echo -e "${CYAN}=== PDF File Association ===${NC}"
    echo -e "SpeedyNote will be available in the 'Open with' menu for PDF files"
    echo -e "Users can set SpeedyNote as the default PDF application through their desktop environment"
}

# Main execution
main() {
    echo -e "${BLUE}Starting multi-distribution packaging process...${NC}"
    
    # Step 1: Verify environment
    check_project_directory
    
    # Step 2: Check packaging dependencies for each format
    FAILED_FORMATS=()
    for format in "${PACKAGE_FORMATS[@]}"; do
        if ! check_packaging_dependencies "$format"; then
            FAILED_FORMATS+=("$format")
        fi
    done
    
    if [[ ${#FAILED_FORMATS[@]} -gt 0 ]]; then
        echo -e "${RED}Cannot continue with formats: ${FAILED_FORMATS[*]}${NC}"
        echo -e "${YELLOW}Please install missing dependencies and try again.${NC}"
        exit 1
    fi
    
    # Step 3: Build project
    build_project
    
    # Step 4: Create packages
    for format in "${PACKAGE_FORMATS[@]}"; do
        case $format in
            deb)
                create_deb_package
                ;;
            rpm)
                create_rpm_package
                ;;
            arch)
                create_arch_package
                ;;
            apk)
                create_apk_package
                ;;
        esac
    done
    
    # Step 5: Cleanup
    cleanup
    
    # Step 6: Show final information
    show_package_info
    
    echo
    echo -e "${GREEN}Multi-distribution packaging process completed successfully!${NC}"
}

# Run main function
main "$@" 