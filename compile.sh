#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
DISTRO=""
AUTO_DETECT=true

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  --deb, -deb       Build for Debian/Ubuntu"
    echo "  --rpm, -rpm       Build for Red Hat/Fedora/SUSE"
    echo "  --arch, -arch     Build for Arch Linux"
    echo "  --apk, -apk       Build for Alpine Linux"
    echo "  --help, -h        Show this help message"
    echo
    echo "If no option is specified, the script will auto-detect the distribution."
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --deb|-deb)
            DISTRO="debian"
            AUTO_DETECT=false
            shift
            ;;
        --rpm|-rpm)
            DISTRO="rpm"
            AUTO_DETECT=false
            shift
            ;;
        --arch|-arch)
            DISTRO="arch"
            AUTO_DETECT=false
            shift
            ;;
        --apk|-apk)
            DISTRO="alpine"
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

echo -e "${BLUE}SpeedyNote Multi-Distribution Build Script${NC}"
echo "=========================================="

# Function to detect distribution
detect_distribution() {
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        case $ID in
            ubuntu|debian|linuxmint|pop)
                echo "debian"
                ;;
            fedora|rhel|centos|rocky|almalinux|opensuse*|sles)
                echo "rpm"
                ;;
            arch|manjaro|endeavouros|garuda)
                echo "arch"
                ;;
            alpine)
                echo "alpine"
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
    DISTRO=$(detect_distribution)
    if [[ $DISTRO == "unknown" ]]; then
        echo -e "${RED}Unable to detect distribution. Please specify manually.${NC}"
        show_usage
        exit 1
    fi
    echo -e "${YELLOW}Auto-detected distribution: $DISTRO${NC}"
else
    echo -e "${YELLOW}Target distribution: $DISTRO${NC}"
fi

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Distribution-specific package checking functions
package_installed_debian() {
    dpkg -l "$1" 2>/dev/null | grep -q "^ii"
}

package_installed_rpm() {
    rpm -q "$1" >/dev/null 2>&1
}

package_installed_arch() {
    pacman -Qi "$1" >/dev/null 2>&1
}

package_installed_alpine() {
    apk info -e "$1" >/dev/null 2>&1
}

# Function to check if a package is installed
package_installed() {
    case $DISTRO in
        debian)
            package_installed_debian "$1"
            ;;
        rpm)
            package_installed_rpm "$1"
            ;;
        arch)
            package_installed_arch "$1"
            ;;
        alpine)
            package_installed_alpine "$1"
            ;;
    esac
}

# Function to get package manager install command
get_install_command() {
    case $DISTRO in
        debian)
            echo "sudo apt install -y"
            ;;
        rpm)
            if command_exists dnf; then
                echo "sudo dnf install -y"
            elif command_exists yum; then
                echo "sudo yum install -y"
            elif command_exists zypper; then
                echo "sudo zypper install -y"
            else
                echo "sudo yum install -y"
            fi
            ;;
        arch)
            echo "sudo pacman -S --needed"
            ;;
        alpine)
            echo "sudo apk add"
            ;;
    esac
}

# Function to map dependencies to distribution-specific package names
get_dependency_mapping() {
    case $DISTRO in
        debian)
            echo "cmake make pkg-config qt6-base-dev libqt6gui6t64 libqt6widgets6t64 qt6-multimedia-dev qt6-tools-dev libpoppler-qt6-dev libsdl2-dev"
            ;;
        rpm)
            echo "cmake make pkgconf qt6-qtbase-devel qt6-qtmultimedia-devel qt6-qttools-devel poppler-qt6-devel SDL2-devel"
            ;;
        arch)
            echo "cmake make pkgconf qt6-base qt6-multimedia qt6-tools poppler-qt6 sdl2-compat"
            ;;
        alpine)
            echo "cmake make pkgconf qt6-qtbase-dev qt6-qtmultimedia-dev qt6-qttools-dev poppler-qt5-dev sdl2-dev"
            ;;
    esac
}

# Check for required dependencies
echo -e "${YELLOW}Checking dependencies for $DISTRO...${NC}"

MISSING_DEPS=()
EXPECTED_DEPS=$(get_dependency_mapping)

# Check basic build tools
if ! command_exists cmake; then
    case $DISTRO in
        debian|rpm|arch|alpine) MISSING_DEPS+=("cmake") ;;
    esac
fi

if ! command_exists make; then
    case $DISTRO in
        debian|rpm|arch|alpine) MISSING_DEPS+=("make") ;;
    esac
fi

if ! command_exists pkg-config; then
    case $DISTRO in
        debian) MISSING_DEPS+=("pkg-config") ;;
        rpm) MISSING_DEPS+=("pkgconf") ;;
        arch) MISSING_DEPS+=("pkgconf") ;;
        alpine) MISSING_DEPS+=("pkgconf") ;;
    esac
fi

# Check distribution-specific packages
case $DISTRO in
    debian)
        if ! package_installed qt6-base-dev; then
            MISSING_DEPS+=("qt6-base-dev" "libqt6gui6t64" "libqt6widgets6t64")
        fi
        if ! package_installed qt6-multimedia-dev; then
            MISSING_DEPS+=("qt6-multimedia-dev")
        fi
        if ! package_installed qt6-tools-dev; then
            MISSING_DEPS+=("qt6-tools-dev")
        fi
        if ! package_installed libpoppler-qt6-dev; then
            MISSING_DEPS+=("libpoppler-qt6-dev")
        fi
        if ! package_installed libsdl2-dev; then
            MISSING_DEPS+=("libsdl2-dev")
        fi
        ;;
    rpm)
        if ! package_installed qt6-qtbase-devel; then
            MISSING_DEPS+=("qt6-qtbase-devel")
        fi
        if ! package_installed qt6-qtmultimedia-devel; then
            MISSING_DEPS+=("qt6-qtmultimedia-devel")
        fi
        if ! package_installed qt6-qttools-devel; then
            MISSING_DEPS+=("qt6-qttools-devel")
        fi
        if ! package_installed poppler-qt6-devel; then
            MISSING_DEPS+=("poppler-qt6-devel")
        fi
        if ! package_installed SDL2-devel; then
            MISSING_DEPS+=("SDL2-devel")
        fi
        ;;
    arch)
        if ! package_installed qt6-base; then
            MISSING_DEPS+=("qt6-base")
        fi
        if ! package_installed qt6-multimedia; then
            MISSING_DEPS+=("qt6-multimedia")
        fi
        if ! package_installed qt6-tools; then
            MISSING_DEPS+=("qt6-tools")
        fi
        if ! package_installed poppler-qt6; then
            MISSING_DEPS+=("poppler-qt6")
        fi
        if ! package_installed sdl2-compat; then
            MISSING_DEPS+=("sdl2-compat")
        fi
        ;;
    alpine)
        if ! package_installed qt6-qtbase-dev; then
            MISSING_DEPS+=("qt6-qtbase-dev")
        fi
        if ! package_installed qt6-qtmultimedia-dev; then
            MISSING_DEPS+=("qt6-qtmultimedia-dev")
        fi
        if ! package_installed qt6-qttools-dev; then
            MISSING_DEPS+=("qt6-qttools-dev")
        fi
        if ! package_installed poppler-qt5-dev; then
            MISSING_DEPS+=("poppler-qt5-dev")
        fi
        if ! package_installed sdl2-dev; then
            MISSING_DEPS+=("sdl2-dev")
        fi
        ;;
esac

# Report missing dependencies
if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${RED}Missing dependencies detected:${NC}"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo
    INSTALL_CMD=$(get_install_command)
    echo -e "${YELLOW}Please install missing dependencies with:${NC}"
    echo "$INSTALL_CMD ${MISSING_DEPS[*]}"
    echo
    read -p "Do you want to install them now? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        $INSTALL_CMD "${MISSING_DEPS[@]}"
    else
        echo -e "${RED}Cannot continue without dependencies. Exiting.${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}All dependencies are available for $DISTRO!${NC}"

# Clean and create build directory
echo -e "${YELLOW}Setting up build directory...${NC}"
rm -rf build
mkdir -p build

# Compile .ts files to .qm using lrelease
echo -e "${YELLOW}Compiling translation files...${NC}"
if command_exists lrelease; then
    lrelease ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts
    
    # Copy .qm files to build directory
    cp resources/translations/*.qm build/ 2>/dev/null || true
else
    echo -e "${YELLOW}Warning: lrelease not found, translations will not be available${NC}"
fi

# Build the project
echo -e "${YELLOW}Building SpeedyNote for $DISTRO...${NC}"
cd build

# Configure with CMake
echo -e "${BLUE}Configuring with CMake...${NC}"
cmake ..

# Build with make using all available cores
echo -e "${BLUE}Compiling...${NC}"
NPROC=$(nproc)
make -j${NPROC}

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build successful for $DISTRO!${NC}"
    echo
    
    # Check if the executable was created
    if [ -f "NoteApp" ]; then
        echo -e "${GREEN}SpeedyNote executable created successfully${NC}"
        echo "Executable location: $(pwd)/NoteApp"
        echo -e "${BLUE}Distribution: $DISTRO${NC}"
        echo
        
        # Ask if user wants to run the application
        read -p "Do you want to run SpeedyNote now? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo -e "${BLUE}Starting SpeedyNote...${NC}"
            ./NoteApp
        fi
    else
        echo -e "${RED}Error: NoteApp executable not found after build${NC}"
        exit 1
    fi
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi 
