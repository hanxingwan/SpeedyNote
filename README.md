# 📝 SpeedyNote

_A lightweight, fast, and stylus-optimized note-taking app built for classic tablet PCs, low-resolution screens, and vintage hardware._

如果您恰好不懂英文，请移步[中文README](https://github.com/alpha-liu-01/SpeedyNote/blob/main/speedynote_README_zh_CN.md)

<a href="https://hellogithub.com/repository/alpha-liu-01/SpeedyNote" target="_blank"><img src="https://abroad.hellogithub.com/v1/widgets/recommend.svg?rid=e86680d007424ab59d68d5e787ad5c12&claim_uid=e5oCIWstjbEUv9D" alt="Featured｜HelloGitHub" style="width: 250px; height: 54px;" width="250" height="54" /></a>

![cover](https://i.imgur.com/UTNNbnM.png)

---

## ✨ Features

- 🖊️ **Pressure-sensitive inking** with stylus support
- 📄 **Multi-page notebooks** with tabbed or scrollable page view
- 📌 **PDF background integration** with annotation overlay
- 🌀 **Dial UI + Joy-Con support** for intuitive one-handed control
- 🎨 **Per-page background styles**: grid, lined, or blank (customizable)
- 💾 **Portable `.snpkg` notebooks** for export/import & sharing
- 🔎 **Zoom, pan, thickness, and color preset switching** via dial
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

- Windows 7/8/10/11/Ubuntu amd64/Kali amd64/PostmarketOS arm64
- Qt 5 or Qt 6 runtime (bundled in Windows releases)
- Stylus input (Wacom recommended)

### 🛠️ Usage

1. Launch `NoteApp.exe`
2. Click **Folder Icon** to select a working folder or **Import `.snpkg` Package**
3. Start writing/drawing using your stylus
4. Use the **MagicDial** or **Joy-Con** to change tools, zoom, scroll, or switch pages
5. Notebooks can be exported as `.snpkg`

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


1. Install **Qt 6** and **CMake**
2. Clone this repository
3. Run:

```bash
rm -r build
mkdir build
# ✅ Update translation source files (ensure the .ts files exist already)
& "C:\Qt\6.8.2\mingw_64\bin\lupdate.exe" . -ts ./resources/translations/app_fr.ts ./resources/translations/app_zh.ts ./resources/translations/app_es.ts
& "C:\Qt\6.8.2\mingw_64\bin\linguist.exe" resources/translations/app_zh.ts
& "C:\Qt\6.8.2\mingw_64\bin\linguist.exe" resources/translations/app_fr.ts
& "C:\Qt\6.8.2\mingw_64\bin\linguist.exe" resources/translations/app_es.ts
```
4. (Optional) Modify translations in the GUI interface
5. Run:
```bash
rm -r build
mkdir build
& "C:\Qt\6.8.2\mingw_64\bin\lrelease.exe" ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts

Copy-Item -Path "C:\Games\yourfolder\resources\translations\*.qm" -Destination "C:\Games\yourfolder\build" -Force

cd .\build
cmake -G "MinGW Makefiles" .. 
cmake --build .  
& "C:\Qt\6.8.2\mingw_64\bin\windeployqt.exe" "NoteApp.exe"
Copy-Item -Path "C:\yourfolder\dllpack\*.dll" -Destination "C:\yourfolder\build" -Force
Copy-Item -Path "C:\yourfolder\bsdtar.exe" -Destination "C:\yourfolder\build" -Force
./NoteApp.exe
cd ../
```
