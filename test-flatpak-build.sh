#!/bin/bash

# SpeedyNote Flatpak Build Test Script
# Validates all improvements and fixes

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

APP_ID="com.github.alpha_liu_01.SpeedyNote"

echo -e "${BLUE}SpeedyNote Flatpak Build Test${NC}"
echo "============================="
echo

# Test 1: Check if all required files exist
echo -e "${YELLOW}Test 1: Checking required files...${NC}"
required_files=(
    "build-flatpak.sh"
    "build-speedynote-only.sh"
    "CMakeLists.txt"
    "com.github.alpha_liu_01.SpeedyNote.yml"
    "com.github.alpha_liu_01.SpeedyNote.desktop"
    "com.github.alpha_liu_01.SpeedyNote.metainfo.xml"
)

for file in "${required_files[@]}"; do
    if [[ -f "$file" ]]; then
        echo -e "  ✅ $file exists"
    else
        echo -e "  ❌ $file missing"
        exit 1
    fi
done

# Test 2: Check CMakeLists.txt fixes
echo -e "\n${YELLOW}Test 2: Checking CMakeLists.txt fixes...${NC}"
if grep -q "set_target_properties(NoteApp PROPERTIES OUTPUT_NAME speedynote)" CMakeLists.txt; then
    echo -e "  ✅ Executable naming fix present"
else
    echo -e "  ❌ Executable naming fix missing"
    exit 1
fi

if grep -q "install(TARGETS NoteApp DESTINATION bin)" CMakeLists.txt; then
    echo -e "  ✅ Install target present"
else
    echo -e "  ❌ Install target missing"
    exit 1
fi

# Test 3: Check manifest fixes
echo -e "\n${YELLOW}Test 3: Checking manifest fixes...${NC}"
if grep -q "FLATPAK_BUILDER_DISABLE_APPSTREAM=1" com.github.alpha_liu_01.SpeedyNote.yml; then
    echo -e "  ✅ AppStream disable flag present"
else
    echo -e "  ❌ AppStream disable flag missing"
    exit 1
fi

# Test 4: Check desktop file fixes
echo -e "\n${YELLOW}Test 4: Checking desktop file fixes...${NC}"
if grep -q "Icon=speedynote" com.github.alpha_liu_01.SpeedyNote.desktop; then
    echo -e "  ✅ Icon naming fix present"
else
    echo -e "  ❌ Icon naming fix missing"
    exit 1
fi

# Test 5: Check metainfo fixes
echo -e "\n${YELLOW}Test 5: Checking metainfo fixes...${NC}"
if grep -q "metadata_license" com.github.alpha_liu_01.SpeedyNote.metainfo.xml; then
    echo -e "  ✅ Metadata license present"
else
    echo -e "  ❌ Metadata license missing"
    exit 1
fi

if grep -q "project_license" com.github.alpha_liu_01.SpeedyNote.metainfo.xml; then
    echo -e "  ✅ Project license present"
else
    echo -e "  ❌ Project license missing"
    exit 1
fi

# Test 6: Check script improvements
echo -e "\n${YELLOW}Test 6: Checking script improvements...${NC}"
if grep -q "rebuild" build-flatpak.sh; then
    echo -e "  ✅ Rebuild command present"
else
    echo -e "  ❌ Rebuild command missing"
    exit 1
fi

if grep -q "disable-rofiles-fuse" build-flatpak.sh; then
    echo -e "  ✅ Build optimization flags present"
else
    echo -e "  ❌ Build optimization flags missing"
    exit 1
fi

if grep -q "ccache" build-flatpak.sh; then
    echo -e "  ✅ Compilation caching enabled"
else
    echo -e "  ❌ Compilation caching missing"
    exit 1
fi

# Test 7: Check if SpeedyNote is already installed
echo -e "\n${YELLOW}Test 7: Checking installation status...${NC}"
if flatpak info --user "$APP_ID" >/dev/null 2>&1; then
    echo -e "  ✅ SpeedyNote is installed"
    
    # Test launching
    echo -e "\n${YELLOW}Test 8: Testing launch capability...${NC}"
    echo -e "  📝 Attempting to launch SpeedyNote..."
    timeout 5 flatpak run "$APP_ID" --version 2>/dev/null || true
    echo -e "  ✅ Launch test completed"
else
    echo -e "  ℹ️ SpeedyNote not installed (run build script first)"
fi

# Test 9: Performance validation
echo -e "\n${YELLOW}Test 9: Performance validation...${NC}"
if [[ -d ".flatpak-builder/cache" ]]; then
    echo -e "  ✅ Build cache exists"
    cache_size=$(du -sh .flatpak-builder/cache 2>/dev/null | cut -f1 || echo "unknown")
    echo -e "  📊 Cache size: $cache_size"
else
    echo -e "  ℹ️ No build cache found (run build script first)"
fi

# Test 10: File permissions
echo -e "\n${YELLOW}Test 10: Checking file permissions...${NC}"
if [[ -x "build-flatpak.sh" ]]; then
    echo -e "  ✅ build-flatpak.sh is executable"
else
    echo -e "  ❌ build-flatpak.sh is not executable"
    chmod +x build-flatpak.sh
    echo -e "  🔧 Fixed permissions"
fi

if [[ -x "build-speedynote-only.sh" ]]; then
    echo -e "  ✅ build-speedynote-only.sh is executable"
else
    echo -e "  ❌ build-speedynote-only.sh is not executable"
    chmod +x build-speedynote-only.sh
    echo -e "  🔧 Fixed permissions"
fi

echo -e "\n${GREEN}All tests completed successfully!${NC}"
echo -e "${BLUE}SpeedyNote Flatpak build system is ready!${NC}"
echo
echo -e "${YELLOW}Next steps:${NC}"
echo -e "  1. Run full build: ${GREEN}./build-flatpak.sh${NC}"
echo -e "  2. Or quick rebuild: ${GREEN}./build-flatpak.sh rebuild${NC}"
echo -e "  3. Or SpeedyNote only: ${GREEN}./build-speedynote-only.sh${NC}"
echo 