#!/bin/bash
set -e

# SpeedyNote Flatpak Build Script
# Automates the entire Flatpak packaging process with all fixes applied

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
APP_ID="com.github.alpha_liu_01.SpeedyNote"
MANIFEST_FILE="${APP_ID}.yml"
REPO_NAME="repo"
BUILD_DIR="build-dir"
REMOTE_NAME="local-speedynote-repo"

echo -e "${BLUE}SpeedyNote Flatpak Build Script${NC}"
echo "=================================="
echo

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check Flatpak installation and setup
check_flatpak_setup() {
    echo -e "${YELLOW}Checking Flatpak setup...${NC}"
    
    if ! command_exists flatpak; then
        echo -e "${RED}Flatpak is not installed!${NC}"
        echo "Install with: sudo pacman -S flatpak"
        exit 1
    fi
    
    if ! command_exists flatpak-builder; then
        echo -e "${RED}flatpak-builder is not installed!${NC}"
        echo "Install with: sudo pacman -S flatpak-builder"
        exit 1
    fi
    
    # Check if Flathub is added
    if ! flatpak remote-list --user | grep -q flathub; then
        echo -e "${YELLOW}Adding Flathub remote...${NC}"
        flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
    fi
    
    # Check if KDE runtime is available
    if ! flatpak info --user org.kde.Platform//6.8 >/dev/null 2>&1; then
        echo -e "${YELLOW}KDE Platform runtime not found. Installing...${NC}"
        flatpak install --user flathub org.kde.Platform//6.8 org.kde.Sdk//6.8 -y
    fi
    
    echo -e "${GREEN}Flatpak setup is ready!${NC}"
}

# Function to verify we're in the right directory
check_project_directory() {
    if [[ ! -f "CMakeLists.txt" ]] || [[ ! -f "Main.cpp" ]]; then
        echo -e "${RED}Error: This doesn't appear to be the SpeedyNote project directory${NC}"
        echo "Please run this script from the SpeedyNote project root directory"
        exit 1
    fi
    
    if [[ ! -f "$MANIFEST_FILE" ]]; then
        echo -e "${RED}Error: Flatpak manifest $MANIFEST_FILE not found${NC}"
        echo "Please ensure the manifest file exists in the project directory"
        exit 1
    fi
}

# Function to clean previous builds
clean_build() {
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    rm -rf ".flatpak-builder"
    rm -rf "$BUILD_DIR"
    rm -rf "$REPO_NAME"
    echo -e "${GREEN}Build directory cleaned${NC}"
}

# Function to build the Flatpak
build_flatpak() {
    echo -e "${YELLOW}Building Flatpak (this may take a while for first build)...${NC}"
    
    # Build the Flatpak with optimizations and proper flags
    flatpak-builder --user --disable-rofiles-fuse --ccache --force-clean \
        --repo="$REPO_NAME" "$BUILD_DIR" "$MANIFEST_FILE"
    
    if [[ $? -eq 0 ]]; then
        echo -e "${GREEN}Flatpak built successfully!${NC}"
    else
        echo -e "${RED}Flatpak build failed!${NC}"
        exit 1
    fi
}

# Function to fix desktop integration for GNOME
fix_desktop_integration() {
    echo -e "${YELLOW}Fixing desktop integration for GNOME...${NC}"
    
    # Define paths
    FLATPAK_DESKTOP_FILE="$HOME/.local/share/flatpak/exports/share/applications/$APP_ID.desktop"
    FLATPAK_ICON_FILE="$HOME/.local/share/flatpak/exports/share/icons/hicolor/256x256/apps/$APP_ID.png"
    
    DESKTOP_DIR="$HOME/.local/share/applications"
    ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
    
    # Create directories if they don't exist
    mkdir -p "$DESKTOP_DIR"
    mkdir -p "$ICON_DIR"
    
    # Copy desktop file to standard location
    if [[ -f "$FLATPAK_DESKTOP_FILE" ]]; then
        echo -e "${YELLOW}Copying desktop file to standard location...${NC}"
        cp "$FLATPAK_DESKTOP_FILE" "$DESKTOP_DIR/"
        
        # Validate desktop file
        if command -v desktop-file-validate >/dev/null 2>&1; then
            desktop-file-validate "$DESKTOP_DIR/$APP_ID.desktop"
            if [[ $? -eq 0 ]]; then
                echo -e "${GREEN}Desktop file is valid${NC}"
            else
                echo -e "${YELLOW}Desktop file has warnings (but should still work)${NC}"
            fi
        fi
    else
        echo -e "${RED}Warning: Desktop file not found at $FLATPAK_DESKTOP_FILE${NC}"
    fi
    
    # Copy icon file to standard location
    if [[ -f "$FLATPAK_ICON_FILE" ]]; then
        echo -e "${YELLOW}Copying icon file to standard location...${NC}"
        cp "$FLATPAK_ICON_FILE" "$ICON_DIR/"
        echo -e "${GREEN}Icon file copied${NC}"
    else
        echo -e "${RED}Warning: Icon file not found at $FLATPAK_ICON_FILE${NC}"
    fi
    
    # Update desktop database
    if command -v update-desktop-database >/dev/null 2>&1; then
        echo -e "${YELLOW}Updating desktop database...${NC}"
        update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
        echo -e "${GREEN}Desktop database updated${NC}"
    fi
    
    echo -e "${GREEN}Desktop integration fix completed!${NC}"
    echo -e "${CYAN}SpeedyNote should now appear in your applications menu${NC}"
}

# Function to install and test
install_and_test() {
    echo -e "${YELLOW}Installing Flatpak for testing...${NC}"
    
    # Add local repository as remote
    echo -e "${YELLOW}Adding local repository as remote...${NC}"
    flatpak --user remote-add --if-not-exists --no-gpg-verify "$REMOTE_NAME" "$(pwd)/$REPO_NAME"
    
    # Install the application
    echo -e "${YELLOW}Installing SpeedyNote from local repository...${NC}"
    flatpak --user install --reinstall --assumeyes "$REMOTE_NAME" "$APP_ID"
    
    if [[ $? -eq 0 ]]; then
        echo -e "${GREEN}Flatpak installed successfully!${NC}"
        
        # Fix desktop integration for GNOME
        fix_desktop_integration
        
        echo
        echo -e "${CYAN}Testing installation:${NC}"
        echo "Run with: flatpak run $APP_ID"
        echo
        read -p "Do you want to test run the application now? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo -e "${YELLOW}Launching SpeedyNote...${NC}"
            flatpak run "$APP_ID" &
            sleep 2
            echo -e "${GREEN}Application launched! Check if it's working correctly.${NC}"
        fi
    else
        echo -e "${RED}Flatpak installation failed!${NC}"
        return 1
    fi
}

# Function to create distributable bundle
create_bundle() {
    echo -e "${YELLOW}Creating distributable bundle...${NC}"
    
    BUNDLE_NAME="SpeedyNote-0.6.1.flatpak"
    
    # Remove old bundle if it exists
    [[ -f "$BUNDLE_NAME" ]] && rm -f "$BUNDLE_NAME"
    
    flatpak build-bundle "$REPO_NAME" "$BUNDLE_NAME" "$APP_ID"
    
    if [[ $? -eq 0 ]]; then
        echo -e "${GREEN}Bundle created: $BUNDLE_NAME${NC}"
        echo -e "${CYAN}Bundle size: $(du -h "$BUNDLE_NAME" | cut -f1)${NC}"
        echo
        echo -e "${CYAN}Users can install this bundle with:${NC}"
        echo "flatpak install $BUNDLE_NAME"
    else
        echo -e "${RED}Bundle creation failed!${NC}"
        return 1
    fi
}

# Function to rebuild SpeedyNote only (for development)
rebuild_speedynote_only() {
    echo -e "${YELLOW}Rebuilding SpeedyNote only (using cached dependencies)...${NC}"
    
    # Check if dependencies are cached
    if [[ ! -d ".flatpak-builder/cache" ]]; then
        echo -e "${RED}No cached dependencies found. Run full build first.${NC}"
        return 1
    fi
    
    # Only remove SpeedyNote build directory to force rebuild
    if [[ -d ".flatpak-builder/build/speedynote-1" ]]; then
        echo -e "${YELLOW}Cleaning only SpeedyNote build directory...${NC}"
        rm -rf .flatpak-builder/build/speedynote-1
    fi
    
    # Build with cached dependencies
    echo -e "${YELLOW}Building SpeedyNote with cached dependencies...${NC}"
    flatpak-builder --user --disable-rofiles-fuse --ccache --force-clean \
        --repo="$REPO_NAME" "$BUILD_DIR" "$MANIFEST_FILE"
    
    if [[ $? -eq 0 ]]; then
        echo -e "${GREEN}SpeedyNote rebuilt successfully!${NC}"
        
        # Reinstall
        echo -e "${YELLOW}Reinstalling SpeedyNote...${NC}"
        flatpak --user install --reinstall --assumeyes "$REMOTE_NAME" "$APP_ID"
        
        if [[ $? -eq 0 ]]; then
            echo -e "${GREEN}SpeedyNote reinstalled successfully!${NC}"
            
            # Fix desktop integration for GNOME
            fix_desktop_integration
        else
            echo -e "${RED}SpeedyNote reinstallation failed!${NC}"
            return 1
        fi
    else
        echo -e "${RED}SpeedyNote rebuild failed!${NC}"
        return 1
    fi
}

# Function to show usage information
show_usage_info() {
    echo
    echo -e "${CYAN}=== Flatpak Information ===${NC}"
    echo -e "App ID: $APP_ID"
    echo -e "Version: 0.6.1"
    echo -e "Runtime: org.kde.Platform 6.8"
    echo
    echo -e "${CYAN}=== Usage Commands ===${NC}"
    echo -e "Run application: ${YELLOW}flatpak run $APP_ID${NC}"
    echo -e "Update: ${YELLOW}flatpak update $APP_ID${NC}"
    echo -e "Uninstall: ${YELLOW}flatpak uninstall $APP_ID${NC}"
    echo -e "Show info: ${YELLOW}flatpak info $APP_ID${NC}"
    echo
    echo -e "${CYAN}=== Distribution ===${NC}"
    echo -e "Bundle file: SpeedyNote-0.6.1.flatpak"
    echo -e "Repository: $REPO_NAME"
    echo -e "For Flathub: Submit manifest to flathub/flathub repository"
    echo
    echo -e "${CYAN}=== Development ===${NC}"
    echo -e "Full build: ${YELLOW}./build-flatpak.sh${NC}"
    echo -e "Quick rebuild: ${YELLOW}./build-flatpak.sh rebuild${NC}"
    echo -e "Clean: ${YELLOW}./build-flatpak.sh clean${NC}"
    echo -e "Build only: ${YELLOW}./build-flatpak.sh build${NC}"
}

# Function to cleanup
cleanup() {
    echo
    read -p "Do you want to clean up build artifacts? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Cleaning up...${NC}"
        rm -rf ".flatpak-builder"
        rm -rf "$BUILD_DIR"
        echo -e "${GREEN}Build artifacts cleaned${NC}"
    fi
}

# Main execution
main() {
    echo -e "${BLUE}Starting Flatpak packaging process...${NC}"
    
    # Step 1: Verify environment
    check_project_directory
    check_flatpak_setup
    
    # Step 2: Clean previous builds
    clean_build
    
    # Step 3: Build Flatpak
    build_flatpak
    
    # Step 4: Install and test
    install_and_test
    
    # Step 5: Create distributable bundle
    create_bundle
    
    # Step 6: Show usage information
    show_usage_info
    
    # Step 7: Cleanup
    cleanup
    
    echo
    echo -e "${GREEN}Flatpak packaging completed successfully!${NC}"
    echo -e "${CYAN}Your SpeedyNote Flatpak is ready for distribution!${NC}"
}

# Handle script arguments
case "${1:-}" in
    "clean")
        clean_build
        exit 0
        ;;
    "build")
        check_project_directory
        check_flatpak_setup
        build_flatpak
        exit 0
        ;;
    "rebuild")
        check_project_directory
        check_flatpak_setup
        rebuild_speedynote_only
        exit 0
        ;;
    "help"|"-h"|"--help")
        echo "Usage: $0 [clean|build|rebuild|help]"
        echo "  clean:   Clean build artifacts"
        echo "  build:   Build Flatpak only"
        echo "  rebuild: Quick rebuild SpeedyNote only (uses cached deps)"
        echo "  help:    Show this help"
        echo "  (no args): Full build process"
        exit 0
        ;;
    *)
        main "$@"
        ;;
esac 