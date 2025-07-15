# SpeedyNote - Linux Port

SpeedyNote is a fast note-taking application with PDF annotation support and controller input, now ported to Arch Linux.

## Features

- Fast note-taking with Qt6-based interface
- PDF annotation and viewing support via Poppler
- Controller input support via SDL2
- Markdown editing capabilities
- Multi-language support (Chinese, Spanish, French)
- Modern Qt6 interface

## Dependencies

### Runtime Dependencies
- `qt6-base` - Core Qt6 framework
- `qt6-multimedia` - Qt6 multimedia support  
- `poppler-qt6` - PDF rendering support
- `sdl2-compat` - SDL2 compatibility layer for controller support

### Build Dependencies
- `cmake` - Build system
- `qt6-tools` - Qt6 development tools (includes lrelease)
- `pkgconf` - Package configuration tool

### Optional Dependencies
- `qt6-wayland` - Wayland support
- `qt6-imageformats` - Additional image format support

## Installation

### From Source

1. **Install dependencies:**
   ```bash
   sudo pacman -S qt6-base qt6-multimedia qt6-tools poppler-qt6 sdl2-compat cmake pkgconf
   ```

2. **Build and run:**
   ```bash
   ./compile.sh
   ```

   The script will:
   - Check for missing dependencies and offer to install them
   - Build the application using CMake
   - Compile translation files
   - Run the application

### Using Pacman (Recommended)

1. **Build the package:**
   ```bash
   makepkg -si
   ```

2. **Install from package file:**
   ```bash
   sudo pacman -U speedynote-*.pkg.tar.zst
   ```

3. **Run SpeedyNote:**
   ```bash
   speedynote
   ```

   Or launch from your application menu.

## Building

### Manual Build

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make -j$(nproc)

# Run
./NoteApp
```

### Package Build

The project includes a `PKGBUILD` file for creating Arch Linux packages:

```bash
# Create source tarball (if needed)
tar -czf speedynote-0.6.1.tar.gz --exclude=build --exclude=.git* .

# Build package
makepkg -f

# Install package
sudo pacman -U speedynote-*.pkg.tar.zst
```

## Architecture

The application uses a cross-platform CMakeLists.txt that automatically detects the build environment:

- **Windows**: Uses hardcoded paths for Qt, SDL2, and Poppler
- **Linux**: Uses pkg-config to find system libraries

Key components:
- **Qt6**: Modern UI framework with multimedia support
- **Poppler-Qt6**: PDF rendering and annotation
- **SDL2**: Controller input handling
- **QMarkdownTextEdit**: Markdown editing widget

## Files Structure

```
├── Main.cpp                    # Application entry point
├── MainWindow.cpp/h            # Main application window
├── InkCanvas.cpp/h             # Drawing/annotation canvas
├── ControlPanelDialog.cpp/h    # Control panel UI
├── SDLControllerManager.cpp/h  # Controller input handling
├── MarkdownWindow.cpp/h        # Markdown editor window
├── markdown/                   # QMarkdownTextEdit submodule
├── resources/                  # Icons, fonts, translations
├── CMakeLists.txt              # Cross-platform build configuration
├── compile.sh                  # Linux build script
├── PKGBUILD                    # Arch Linux package configuration
└── README-Linux.md             # This file
```

## Distribution

The application can be distributed as:

1. **Source package** - Users compile from source
2. **Binary package** - Pre-compiled `.pkg.tar.zst` for pacman
3. **AUR package** - Upload PKGBUILD to Arch User Repository

### For AUR Distribution

1. Create AUR repository
2. Upload `PKGBUILD` and `.SRCINFO` files
3. Users can install with AUR helpers or manual `makepkg`

## Troubleshooting

### Build Issues

1. **Missing dependencies**: Run `./compile.sh` which will check and offer to install missing packages
2. **CMake configuration fails**: Ensure all Qt6 packages are installed
3. **Linking errors**: Verify poppler-qt6 and sdl2-compat are installed

### Runtime Issues

1. **Segmentation fault**: This may be related to graphics drivers or Qt platform plugins
   - Try setting `QT_QPA_PLATFORM=xcb` for X11
   - Install `qt6-wayland` for Wayland support
2. **No controller detected**: Ensure SDL2 has proper permissions and drivers
3. **Missing translations**: lrelease might not be available, install `qt6-tools`

### Graphics Issues

If you encounter graphics-related crashes:
```bash
# For X11
QT_QPA_PLATFORM=xcb speedynote

# For Wayland  
QT_QPA_PLATFORM=wayland speedynote

# Software rendering fallback
QT_QUICK_BACKEND=software speedynote
```

## Contributing

To contribute to the Linux port:

1. Test on different Arch Linux configurations
2. Improve the CMakeLists.txt for better cross-platform support
3. Report and fix any Linux-specific issues
4. Enhance the packaging scripts

## License

SpeedyNote is licensed under GPL3. See LICENSE file for details. 