# 📝 SpeedyNote

_A lightweight, fast, and stylus-optimized note-taking app built for classic tablet PCs, low-resolution screens, and vintage hardware._

如果您恰好不懂英文，请移步[中文README](https://github.com/alpha-liu-01/SpeedyNote/blob/main/speedynote_README_zh_CN.md)

<a href="https://hellogithub.com/repository/alpha-liu-01/SpeedyNote" target="_blank"><img src="https://abroad.hellogithub.com/v1/widgets/recommend.svg?rid=e86680d007424ab59d68d5e787ad5c12&claim_uid=e5oCIWstjbEUv9D" alt="Featured｜HelloGitHub" style="width: 250px; height: 54px;" width="250" height="54" /></a>

![cover](https://i.imgur.com/U161QSH.png)

---

## ✨ Features

- 🖊️ **Pressure-sensitive inking** with stylus support
- 📄 **Multi-page notebooks** with tabbed or scrollable page view
- 📌 **PDF background integration** with annotation overlay
- 🌀 **Dial UI + Joy-Con support** for intuitive one-handed control
- 🎨 **Per-page background styles**: grid, lined, or blank (customizable)
- 💾 **Portable `.snpkg` notebooks** for export/import & sharing
- 🔎 **Zoom, pan, thickness, and color preset switching** via dial
- 🗔 **Markdown sticky notes are supported** for text-based notes
- 💡 **Designed for low-spec devices** (133Hz Sample Rate @ Intel Atom N450)
- 🌏 **Supports multiple languages across the globe** (Covers half the global population)

---

## 📸 Screenshots

| Drawing | Dial UI / Joycon Controls | Overlay Grid Options |
|----------------|------------------------|-----------------------|
| ![draw](https://i.imgur.com/iARL6Vo.gif) | ![pdf](https://i.imgur.com/NnrqOQQ.gif) | ![grid](https://i.imgur.com/YaEdx1p.gif) |


---

## 🚀 Getting Started

### ✅ Requirements

- Windows 7/8/10/11/Ubuntu/Debian/Fedora/RedHat/ArchLinux/AlpineLinux
- Qt 5 or Qt 6 runtime (bundled in Windows releases)
- Stylus input (Wacom recommended)

🛠️ Usage

1. Launch `SpeedyNote` shortcut on desktop
2. Click **Folder Icon** to select a working folder or **Import `.snpkg` Package
3. *(Optitonal)* Click the PDF button on the tool bar to import a PDF document
4. Start writing/drawing using your stylus
5. Use the **MagicDial** or **Joy-Con** to change tools, zoom, scroll, or switch pages
6. Notebooks can be exported as `.snpkg`

###### OR

1. Right click a PDF file in File Explorer (or equivalent)  
2. Click open with and select SpeedyNote  
3. Create a folder for the directory of the current notebook  
4. Next time when you double click a PDF with a working directory already created, step 3 will be skipped.  
5. Start writing/drawing using your stylus

---

## 📦 Notebook Format

- Can be saved as:
  - 📁 A **folder** with `.png` pages + metadata
  - 🗜️ A **`.snpkg` archive** for portability (non-compressed `.tar`)
- Each notebook may contain:
  - Annotated page images (`annotated_XXXX.png`)
  - Optional background images from PDF (`XXXX.png`)
  - Metadata: background style, density, color, and PDF path

---

## 🎮 Controller Support

SpeedyNote supports controller input, ideal for tablet users:

- ✅ **Left Joy-Con supported**
- 🎛️ Analog stick → Dial control
- 🔘 Buttons can be mapped to:
  - Control the dial with multiple features
  - Toggle fullscreen
  - Change color / thickness
  - Open control panel
  - Create/delete pages

> Long press + turn = hold-and-turn mappings

---

## 📁 Building From Source


#### Windows

1. Run translation and compiling scripts
  ```powershell
  ./translate.ps1
  ./compile.ps1
  ```

(Dependency directories may need to be modified)

2. Install `InnoSetup` and open `packaging.iss` with it to pack SpeedyNote into an executable installer.
3. Run `SpeedyNoteInstaller.exe` to install SpeedyNote to your PC. 



#### Linux
##### Flatpak
1. Run compile and package scripts
   
   ```bash
   ./compile.sh
   ./build-flatpak.sh
   ```

2. Install the flatpak package
   
   ```bash
   flatpak install ./speedynote.flatpak
   ```
##### Native Packages
1. run `./compile.sh` and `./build-package.sh`
2. Install the packages for your Linux distro. Note that the dependencies for Fedora and RedHat are not yet tested.`.deb`,`.pkg.tar.zst` and `.apk` are tested and working.
   
