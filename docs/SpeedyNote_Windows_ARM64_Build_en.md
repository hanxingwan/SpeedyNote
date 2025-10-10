# SpeedyNote Windows ARM64 Build

### Preparation

- Windows 11 ARM64

- Official MSYS2 ARM64 installer

- InnoSetup

---

### Environment

##### Qt Online Installer

Unlike the Windows x86-64 version, Qt online installer is **no longer needed** for this build, because the online installer only provided one option, which is MSVC2022 ARM64. I found it really painful to install a whole Visual Studio on my poor WoA tablet with only 4 GiB of RAM.

##### MSYS2

Install with default settings.  

Install these packages:

```bash
pacman -S mingw-w64-clang-aarch64-toolchain mingw-w64-x86_64-cmake
pacman -S mingw-w64-clang-aarch64-qt6-base mingw-w64-clang-aarch64-qt6-tools
pacman -S mingw-w64-clang-aarch64-poppler
pacman -S mingw-w64-clang-aarch64-poppler-qt6
pacman -S mingw-w64-clang-aarch64-SDL2
```

##### Additional Files

Create a folder called `poppler` in `C:\msys64\clangarm64\lib\cmake` and copy the `FindPoppler_ARM64.cmake` file from the root of the source code to that folder, and rename it to `FindPoppler.cmake`.

##### Path

Add this directory to Path.

```cmd
C:\msys64\clangarm64\bin
```

and then restart your PC.

##### CMake Setup

In the CMake Tools extension of Visual Studio Code, find the `Cmake: Cmake Path` option and change it to `C:\msys64\clangarm64\bin\cmake.exe` .

##### Other Dependencies

The`arm64dllpack` and `share` folders hold the required runtime and font files. I'm not sure if it's a good idea to put them into my repository. You may extract them from a SpeedyNote installation and put them into the SpeedyNote source code root.

```powershell
    Directory: C:\Users\1\Documents\GitHub\SpeedyNote

Mode                 LastWriteTime         Length Name
----                 -------------         ------ ----
d----           2025/9/13    23:40                build
d----           2025/9/13    22:54                arm64dllpack
d----           2025/9/13    22:51                docs
d----           2025/9/13    22:51                markdown
d----           2025/9/13    23:17                Output
d----           2025/9/13    22:51                readme
d----           2025/9/13    22:51                resources
d----           2025/9/13    22:59                share
d----           2025/9/13    22:51                source
-a---           2025/9/13    23:14            455 .gitignore
-a---           2025/9/13    23:04            971 app_icon.rc
-a---           2025/9/13    23:04          33437 build-package.sh
-a---           2025/9/13    23:40           7241 CMakeLists.txt
-a---           2025/9/13    23:00           1011 compile.ps1
-a---           2025/9/13    22:51            870 copy_dlls.bat
-a---           2025/9/13    23:13       42641076 dllpack.7z
-a---           2025/9/13    22:51            900 FindPoppler.cmake
-a---           2025/9/13    22:51           1090 LICENSE
-a---           2025/9/13    23:04           5330 packaging.iss
-a---           2025/9/13    22:51           4637 PDF_ASSOCIATION_README.md
-a---           2025/9/13    22:51           4313 README.md
-a---           2025/9/13    22:51           5865 resources.qrc
-a---           2025/9/13    23:12        1577524 share.7z
-a---           2025/9/13    22:52            489 translate.ps1
```

Pay attention to the location of these folders.

---

### Build

Run `compile_arm64.ps1`to build SpeedyNote. The complete build is in `build`.You may want to delete some temporary files that CMake generated. 

---

### Packaging

Use`InnoSetup`to pack SpeedyNote. Hit the play button and InnoSetup automatically finish the packaging process. 
