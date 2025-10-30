# SpeedyNote macOS Build Guide

This guide explains how to build SpeedyNote on macOS using Homebrew dependencies.

## Prerequisites

### 1. Install Homebrew
If you don't have Homebrew installed, install it from [https://brew.sh](https://brew.sh):

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 2. Install Required Dependencies

The compile script can install dependencies automatically, but you can also install them manually:

```bash
brew install qt@6 poppler sdl2 cmake pkg-config
```

## System Requirements

### Supported Architectures
- **Apple Silicon (ARM64)**: M1, M2, M3, M4 Macs
  - Optimized with `-mcpu=apple-m1` for native ARM performance
  - Uses `/opt/homebrew` for Homebrew packages
  
- **Intel (x86-64)**: Intel-based Macs
  - Optimized with `-march=nehalem` (1st gen Intel Core i series and newer)
  - Uses `/usr/local` for Homebrew packages

### macOS Version
- macOS 10.15 (Catalina) or later recommended
- Qt 6 requires macOS 10.14 or later

## Building SpeedyNote

### Quick Build (Recommended)

Simply run the provided compilation script:

```bash
./compile-mac.sh
```

The script will:
1. Check if Homebrew is installed
2. Verify all dependencies (offer to install missing ones)
3. Configure Qt paths automatically
4. Build with architecture-specific optimizations
5. Optionally create a macOS .app bundle

### Manual Build

If you prefer to build manually:

1. **Set up Qt environment**:

   For Apple Silicon:
   ```bash
   export HOMEBREW_PREFIX="/opt/homebrew"
   export PATH="${HOMEBREW_PREFIX}/opt/qt@6/bin:$PATH"
   ```
   
   For Intel Mac:
   ```bash
   export HOMEBREW_PREFIX="/usr/local"
   export PATH="${HOMEBREW_PREFIX}/opt/qt@6/bin:$PATH"
   ```

2. **Create build directory**:

   ```bash
   mkdir build
   cd build
   ```

3. **Configure with CMake**:

   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
   ```

4. **Compile**:

   ```bash
   make -j$(sysctl -n hw.ncpu)
   ```

5. **Run**:

   ```bash
   ./NoteApp
   ```

## Build Optimizations

### Apple Silicon (M1/M2/M3/M4)
- Target CPU: Apple M1 (`-mcpu=apple-m1`)
- Optimization level: `-O3`
- Fast math enabled for aggressive floating-point optimizations
- Loop unrolling and frame pointer omission enabled

### Intel Macs
- Target architecture: Nehalem (`-march=nehalem`)
- SSE4.2 and POPCNT instructions enabled
- Optimization level: `-O3`
- Fast math, loop unrolling, and frame pointer omission enabled

## Creating a macOS App Bundle

The compile script can optionally create a macOS .app bundle. When prompted, choose 'y' to create `SpeedyNote.app`.

The app bundle includes:
- Compiled executable
- Info.plist with app metadata
- App icon (if available)
- Bundled Qt frameworks (if `macdeployqt` is available)

To install `macdeployqt`:
```bash
# It comes with Qt, just ensure Qt bin is in PATH
export PATH="$(brew --prefix qt@6)/bin:$PATH"
```

## Troubleshooting

### "Qt not found" error
Ensure Qt is in your CMAKE_PREFIX_PATH:
```bash
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
```

### "poppler-qt6 not found" error
Install poppler with Qt6 support:
```bash
brew install poppler
```

**Note**: On macOS, Homebrew's poppler doesn't provide a `poppler-qt6.pc` file, so the build system manually configures the poppler paths from `${HOMEBREW_PREFIX}/opt/poppler`.

### "SDL2 not found" error
Install SDL2:
```bash
brew install sdl2
```

### Architecture mismatch
If you're on Apple Silicon but seeing x86-64 errors, ensure you're not running under Rosetta:
```bash
arch
# Should output: arm64
```

### Permission denied when running compile-mac.sh
Make the script executable:
```bash
chmod +x compile-mac.sh
```

## Dependencies Details

| Package | Purpose | Homebrew Formula | Configuration Method |
|---------|---------|------------------|----------------------|
| Qt 6 | GUI framework | `qt@6` | CMake find_package |
| Poppler | PDF rendering | `poppler` | Manual (no .pc file) |
| SDL2 | Controller/audio support | `sdl2` | pkg-config |
| CMake | Build system | `cmake` | N/A |
| pkg-config | Dependency configuration | `pkg-config` | N/A |

### Poppler on macOS
Unlike Linux, Homebrew's poppler package on macOS doesn't provide a `poppler-qt6.pc` file. The build system handles this by:
- Manually setting include paths: `${HOMEBREW_PREFIX}/opt/poppler/include/poppler/qt6`
- Manually setting library paths: `${HOMEBREW_PREFIX}/opt/poppler/lib`
- Explicitly linking: `poppler-qt6` and `poppler` libraries

This is similar to how Windows builds handle Poppler.

## Audio Backend

SpeedyNote uses **CoreAudio** framework on macOS for audio playback, which is automatically linked by the build system:
- CoreAudio.framework
- AudioToolbox.framework
- AudioUnit.framework

## Notes

- All dependencies are installed via Homebrew, not the Qt Maintenance Tool
- The build system automatically detects your Mac's architecture (Apple Silicon vs Intel)
- Optimizations are architecture-specific for maximum performance
- Translation files (.qm) are automatically compiled if `lrelease` is available

## Building for Distribution

To create a distributable app bundle:

1. Run the compile script and choose to create an app bundle
2. Use `macdeployqt` to bundle Qt frameworks:
   ```bash
   macdeployqt SpeedyNote.app
   ```
3. Optionally sign and notarize the app for Gatekeeper (requires Apple Developer account)

## Next Steps

After building:
- Run the app: `./build/NoteApp` or `open SpeedyNote.app`
- Read the main README for usage instructions
- Report issues on GitHub

## Support

For build issues or questions:
- Check the troubleshooting section above
- Review CMakeLists.txt for platform-specific configuration
- Open an issue on GitHub with your build log

