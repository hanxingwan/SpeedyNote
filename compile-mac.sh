#!/bin/bash
set -e

# SpeedyNote macOS Compilation Script

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if we're in the right directory
check_project_directory() {
    if [[ ! -f "CMakeLists.txt" ]]; then
        echo -e "${RED}Error: This doesn't appear to be the SpeedyNote project directory${NC}"
        echo "Please run this script from the SpeedyNote project root directory"
        exit 1
    fi
}

# Function to detect architecture
detect_architecture() {
    local arch=$(uname -m)
    case $arch in
        x86_64|amd64)
            echo "Intel x86-64"
            ;;
        arm64|aarch64)
            echo "Apple Silicon (ARM64)"
            ;;
        *)
            echo "Unknown ($arch)"
            ;;
    esac
}

# Function to check Homebrew installation
check_homebrew() {
    echo -e "${YELLOW}Checking Homebrew installation...${NC}"
    
    if ! command_exists brew; then
        echo -e "${RED}Error: Homebrew is not installed${NC}"
        echo "Please install Homebrew from https://brew.sh"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Homebrew found${NC}"
}

# Function to check and install dependencies
check_dependencies() {
    echo -e "${YELLOW}Checking required dependencies...${NC}"
    
    local missing_deps=()
    
    # Check for required packages
    if ! brew list qt@6 &>/dev/null; then
        missing_deps+=("qt@6")
    fi
    
    if ! brew list poppler &>/dev/null; then
        missing_deps+=("poppler")
    fi
    
    if ! brew list sdl2 &>/dev/null; then
        missing_deps+=("sdl2")
    fi
    
    if ! brew list cmake &>/dev/null; then
        missing_deps+=("cmake")
    fi
    
    if ! brew list pkg-config &>/dev/null; then
        missing_deps+=("pkg-config")
    fi
    
    if [ ${#missing_deps[@]} -eq 0 ]; then
        echo -e "${GREEN}✓ All dependencies are installed${NC}"
    else
        echo -e "${YELLOW}Missing dependencies: ${missing_deps[*]}${NC}"
        echo -e "${CYAN}Would you like to install them now? (y/n)${NC}"
        read -r response
        if [[ "$response" =~ ^[Yy]$ ]]; then
            echo -e "${YELLOW}Installing missing dependencies...${NC}"
            brew install "${missing_deps[@]}"
            echo -e "${GREEN}✓ Dependencies installed${NC}"
        else
            echo -e "${RED}Cannot proceed without required dependencies${NC}"
            exit 1
        fi
    fi
}

# Function to set up Qt paths
setup_qt_paths() {
    local arch=$(uname -m)
    
    if [[ "$arch" == "arm64" ]] || [[ "$arch" == "aarch64" ]]; then
        export HOMEBREW_PREFIX="/opt/homebrew"
    else
        export HOMEBREW_PREFIX="/usr/local"
    fi
    
    # Add Qt binaries to PATH for lrelease
    export PATH="${HOMEBREW_PREFIX}/opt/qt@6/bin:$PATH"
    
    echo -e "${CYAN}Using Homebrew prefix: ${HOMEBREW_PREFIX}${NC}"
}

# Function to build the project
build_project() {
    echo -e "${YELLOW}Building SpeedyNote...${NC}"
    
    # Detect and display architecture
    local arch_type=$(detect_architecture)
    echo -e "${CYAN}Detected architecture: ${arch_type}${NC}"
    
    case $arch_type in
        "Apple Silicon (ARM64)")
            echo -e "${MAGENTA}🍎 Optimization target: Apple Silicon (M1/M2/M3/M4)${NC}"
            ;;
        "Intel x86-64")
            echo -e "${CYAN}🍎 Optimization target: Intel Mac (Nehalem/Core i series)${NC}"
            ;;
        *)
            echo -e "${YELLOW}Using generic optimizations${NC}"
            ;;
    esac
    
    # Clean and create build directory
    rm -rf build
    mkdir -p build
    
    # Compile translations if lrelease is available
    if command_exists lrelease; then
        echo -e "${YELLOW}Compiling translation files...${NC}"
        lrelease ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts 2>/dev/null || true
        cp resources/translations/*.qm build/ 2>/dev/null || true
    fi
    
    cd build
    
    # Configure and build with optimizations
    echo -e "${YELLOW}Configuring build with maximum performance optimizations...${NC}"
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
    
    # Get CPU count for parallel compilation
    local cpu_count=$(sysctl -n hw.ncpu)
    echo -e "${YELLOW}Compiling with ${cpu_count} parallel jobs...${NC}"
    make -j${cpu_count}
    
    if [[ ! -f "NoteApp" ]]; then
        echo -e "${RED}Build failed: NoteApp executable not found${NC}"
        exit 1
    fi
    
    cd ..
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${CYAN}Executable location: build/NoteApp${NC}"
}

# Function to create app bundle (optional)
create_app_bundle() {
    echo
    echo -e "${CYAN}Would you like to create a macOS .app bundle? (y/n)${NC}"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Creating SpeedyNote.app bundle...${NC}"
        
        # Create app bundle structure
        mkdir -p "SpeedyNote.app/Contents/MacOS"
        mkdir -p "SpeedyNote.app/Contents/Resources"
        
        # Copy executable
        cp build/NoteApp "SpeedyNote.app/Contents/MacOS/"
        
        # Copy icon if available
        if [[ -f "resources/icons/mainicon.png" ]]; then
            cp "resources/icons/mainicon.png" "SpeedyNote.app/Contents/Resources/"
        fi
        
        # Create Info.plist
        cat > "SpeedyNote.app/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>NoteApp</string>
    <key>CFBundleIdentifier</key>
    <string>com.github.alpha-liu-01.SpeedyNote</string>
    <key>CFBundleName</key>
    <string>SpeedyNote</string>
    <key>CFBundleDisplayName</key>
    <string>SpeedyNote</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF
        
        # Use macdeployqt if available to bundle Qt frameworks
        if command_exists macdeployqt; then
            echo -e "${YELLOW}Running macdeployqt to bundle Qt frameworks...${NC}"
            macdeployqt "SpeedyNote.app"
        else
            echo -e "${YELLOW}Note: macdeployqt not found. The app bundle may need Qt frameworks installed separately.${NC}"
        fi
        
        echo -e "${GREEN}✓ SpeedyNote.app bundle created${NC}"
    fi
}

# Main execution
main() {
    echo -e "${BLUE}═══════════════════════════════════════════════${NC}"
    echo -e "${BLUE}   SpeedyNote macOS Compilation Script${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════${NC}"
    echo
    
    # Step 1: Verify environment
    check_project_directory
    
    # Step 2: Check Homebrew
    check_homebrew
    
    # Step 3: Check dependencies
    check_dependencies
    
    # Step 4: Set up Qt paths
    setup_qt_paths
    
    # Step 5: Build project
    build_project
    
    # Step 6: Optional app bundle
    create_app_bundle
    
    echo
    echo -e "${GREEN}═══════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  SpeedyNote compilation completed successfully!${NC}"
    echo -e "${GREEN}═══════════════════════════════════════════════${NC}"
    echo
    echo -e "${CYAN}To run SpeedyNote:${NC}"
    echo -e "  ${YELLOW}./build/NoteApp${NC}"
    if [[ -d "SpeedyNote.app" ]]; then
        echo -e "  ${YELLOW}or open SpeedyNote.app${NC}"
    fi
    echo
}

# Run main function
main "$@"

