# SpeedyNote Windows ARM64 Build

### Preparation

- Windows 11 x86-64 or ARM64

- Official MSYS2 installer

- InnoSetup

---

### Environment

##### Qt Online Installer

Qt online installer is **no longer needed**.

##### MSYS2

Install with default settings.  

Install these packages:

```bash
pacman -S mingw-w64-clang-x86_64-toolchain mingw-w64-x86_64-cmake
pacman -S mingw-w64-clang-x86_64-qt6-base mingw-w64-clang-x86_64-qt6-tools
pacman -S mingw-w64-clang-x86_64-poppler
pacman -S mingw-w64-clang-x86_64-poppler-qt6
pacman -S mingw-w64-clang-x86_64-SDL2
```
If you have an arm64 system and want to compile SpeedyNote arm64, you may replace the `x86_64` with `aarch64` on the commands above.

##### Additional Files

Create a folder called `poppler` in `C:\msys64\clang64\lib\cmake` or `C:\msys64\clangarm64\lib\cmake` based on your PC's architecture and copy the `FindPoppler.cmake` file from the root of the source code to that folder. If you have an arm64 system, replace the paths from clang64 to clangarm64. 

##### Path

Add this directory to Path. If you have an arm64 system, replace the paths from clang64 to clangarm64. 

```cmd
C:\msys64\clang64\bin
```

and then restart your PC.

##### CMake Setup

In the CMake Tools extension of Visual Studio Code, find the `Cmake: Cmake Path` option and change it to `C:\msys64\clang64\bin\cmake.exe` .
Add this directory to Path. As always, if you have an arm64 system, replace the paths from clang64 to clangarm64. 

##### Other Dependencies

The `share` folders hold the required font files. I'm not sure if it's a good idea to put them into my repository. You may extract them from a SpeedyNote installation and put them into the SpeedyNote source code root. The `share` folder should be copied to the same folder as where the executable is. 

---

### Build

Run `compile.ps1`to build SpeedyNote. The complete build is in `build`.You may want to delete some temporary files that CMake generated. 

---

### Packaging

Use`InnoSetup`to pack SpeedyNote. Hit the play button and InnoSetup automatically finish the packaging process. 
