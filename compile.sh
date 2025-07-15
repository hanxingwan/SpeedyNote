#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}SpeedyNote Linux Build Script${NC}"
echo "=================================="

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if a package is installed (for pacman)
package_installed() {
    pacman -Qi "$1" >/dev/null 2>&1
}

# Check for required dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"

MISSING_DEPS=()

# Check for basic build tools
if ! command_exists cmake; then
    MISSING_DEPS+=("cmake")
fi

if ! command_exists make; then
    MISSING_DEPS+=("make")
fi

if ! command_exists pkg-config; then
    MISSING_DEPS+=("pkgconf")
fi

# Check for Qt6 packages
if ! package_installed qt6-base; then
    MISSING_DEPS+=("qt6-base")
fi

if ! package_installed qt6-multimedia; then
    MISSING_DEPS+=("qt6-multimedia")
fi

if ! package_installed qt6-tools; then
    MISSING_DEPS+=("qt6-tools")
fi

# Check for other dependencies
if ! package_installed poppler-qt6; then
    MISSING_DEPS+=("poppler-qt6")
fi

if ! package_installed sdl2-compat; then
    MISSING_DEPS+=("sdl2-compat")
fi

# Report missing dependencies
if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${RED}Missing dependencies detected:${NC}"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo
    echo -e "${YELLOW}Please install missing dependencies with:${NC}"
    echo "sudo pacman -S ${MISSING_DEPS[*]}"
    echo
    read -p "Do you want to install them now? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo pacman -S "${MISSING_DEPS[@]}"
    else
        echo -e "${RED}Cannot continue without dependencies. Exiting.${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}All dependencies are available!${NC}"

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
echo -e "${YELLOW}Building SpeedyNote...${NC}"
cd build

# Configure with CMake
echo -e "${BLUE}Configuring with CMake...${NC}"
cmake ..

# Build with make using all available cores
echo -e "${BLUE}Compiling...${NC}"
NPROC=$(nproc)
make -j${NPROC}

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo
    
    # Check if the executable was created
    if [ -f "NoteApp" ]; then
        echo -e "${GREEN}SpeedyNote executable created successfully${NC}"
        echo "Executable location: $(pwd)/NoteApp"
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