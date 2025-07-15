#!/bin/bash

set -e

# Function to fix desktop integration for GNOME
fix_desktop_integration() {
    echo "🔧 Fixing desktop integration for GNOME..."
    
    # Define paths
    APP_ID="com.github.alpha_liu_01.SpeedyNote"
    FLATPAK_DESKTOP_FILE="$HOME/.local/share/flatpak/exports/share/applications/$APP_ID.desktop"
    FLATPAK_ICON_FILE="$HOME/.local/share/flatpak/exports/share/icons/hicolor/256x256/apps/$APP_ID.png"
    
    DESKTOP_DIR="$HOME/.local/share/applications"
    ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
    
    # Create directories if they don't exist
    mkdir -p "$DESKTOP_DIR"
    mkdir -p "$ICON_DIR"
    
    # Copy desktop file to standard location
    if [[ -f "$FLATPAK_DESKTOP_FILE" ]]; then
        echo "📋 Copying desktop file to standard location..."
        cp "$FLATPAK_DESKTOP_FILE" "$DESKTOP_DIR/"
        echo "✅ Desktop file copied"
    else
        echo "⚠️ Warning: Desktop file not found at $FLATPAK_DESKTOP_FILE"
    fi
    
    # Copy icon file to standard location
    if [[ -f "$FLATPAK_ICON_FILE" ]]; then
        echo "🎨 Copying icon file to standard location..."
        cp "$FLATPAK_ICON_FILE" "$ICON_DIR/"
        echo "✅ Icon file copied"
    else
        echo "⚠️ Warning: Icon file not found at $FLATPAK_ICON_FILE"
    fi
    
    # Update desktop database
    if command -v update-desktop-database >/dev/null 2>&1; then
        echo "🔄 Updating desktop database..."
        update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
        echo "✅ Desktop database updated"
    fi
    
    echo "🎉 Desktop integration fix completed!"
    echo "📱 SpeedyNote should now appear in your applications menu"
}

# Function to create distributable bundle
create_bundle() {
    echo "📦 Creating distributable bundle..."
    
    BUNDLE_NAME="SpeedyNote-0.6.1.flatpak"
    REPO_NAME="repo"
    APP_ID="com.github.alpha_liu_01.SpeedyNote"
    
    # Remove old bundle if it exists
    [[ -f "$BUNDLE_NAME" ]] && rm -f "$BUNDLE_NAME"
    
    flatpak build-bundle "$REPO_NAME" "$BUNDLE_NAME" "$APP_ID"
    
    if [[ $? -eq 0 ]]; then
        echo "✅ Bundle created: $BUNDLE_NAME"
        echo "📏 Bundle size: $(du -h "$BUNDLE_NAME" | cut -f1)"
        echo ""
        echo "🚀 Ready for distribution!"
        echo "📤 Upload this file to GitHub releases: $BUNDLE_NAME"
        echo "👥 Users can install with: flatpak install $BUNDLE_NAME"
    else
        echo "❌ Bundle creation failed!"
        return 1
    fi
}

echo "🚀 Building SpeedyNote only (using cached dependencies)..."

# Check if dependencies are cached
if [ -d ".flatpak-builder/cache" ]; then
    echo "✅ Found cached dependencies (poppler, SDL2)"
else
    echo "❌ No cached dependencies found. Run full build first with ./build-flatpak.sh"
    exit 1
fi

# Only remove SpeedyNote build directory to force rebuild
if [ -d ".flatpak-builder/build/speedynote-1" ]; then
    echo "🧹 Cleaning only SpeedyNote build directory..."
    rm -rf .flatpak-builder/build/speedynote-1
fi

# Build with cached dependencies - this should skip poppler and SDL2 since they're cached
echo "🔨 Building SpeedyNote with cached dependencies..."
flatpak-builder --user --disable-rofiles-fuse --ccache --force-clean \
    --repo=repo build-dir com.github.alpha_liu_01.SpeedyNote.yml

echo "✅ SpeedyNote build completed!"

# Add local repo as remote and install
echo "📦 Adding local repo as remote..."
flatpak --user remote-add --if-not-exists --no-gpg-verify local-speedynote-repo $(pwd)/repo

echo "📦 Installing SpeedyNote from local repo..."
flatpak --user install --reinstall --assumeyes local-speedynote-repo com.github.alpha_liu_01.SpeedyNote

echo "🎉 SpeedyNote installed successfully!"

# Fix desktop integration for GNOME
fix_desktop_integration

# Create distributable bundle
create_bundle

echo "🚀 Run with: flatpak run com.github.alpha_liu_01.SpeedyNote" 