# SpeedyNote Windows Build

### Preparation

- Windows 10 1809+ or Windows 11

- Official MSYS2 AMD64 installer

- Qt Online Installer

- InnoSetup

---

### Environment

##### Qt Online Installer

Log in your Qt account (or sign up for free) and download Qt online installer. The only option that needs to be ticked is the`MinGW 13.1.0 64-bit` below the latest version of Qt (6.9.2 when this document was created)，and the mandatory Qt Maintenance Tool. There is no need to restart your PC after the installation. 

##### MSYS2

Install with default settings.  

Install these packages:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
pacman -S mingw-w64-x86_64-poppler
pacman -S mingw-w64-x86_64-poppler-qt6
pacman -S mingw-w64-x86_64-SDL2
```

##### Additional Files

Create a folder called `poppler` in `C:\Qt\6.9.2\mingw_64\lib\cmake` and copy the `FindPoppler.cmake` file from the root of the source code to that folder.

##### Path

Add these directories to Path.

```cmd
C:\msys64\mingw64\bin
C:\Qt\Tools\mingw1310_64\bin
```

and then restart your PC.

##### Other Dependencies

The`dllpack` and `share` folders hold the required runtime and font files. I'm not sure if it's a good idea to put them into my repository. You may extract them from a SpeedyNote installation and put them into the SpeedyNote source code root.

```powershell
    Directory: C:\Users\1\Documents\GitHub\SpeedyNote

Mode                 LastWriteTime         Length Name
----                 -------------         ------ ----
d----           2025/9/13    23:40                build
d----           2025/9/13    22:54                dllpack
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

Run `compile.ps1`to build SpeedyNote. The complete build is in `build`.You may want to delete some temporary files that CMake generated. `opengl32sw.dll`can also be deleted as long as you have any kind of hardware acceleration. 

---

### Packaging

Use`InnoSetup`to pack SpeedyNote. Hit the play button and InnoSetup automatically finish the packaging process. 
