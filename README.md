# 📝 SpeedyNote

_A lightweight, fast, and stylus-optimized note-taking app built for classic tablet PCs, low-resolution screens, and vintage hardware._

![cover](https://i.imgur.com/HkCVF5W.png)

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

---

## 📸 Screenshots

| Drawing | Dial UI / Joycon Controls | Overlay Grid Options |
|----------------|------------------------|-----------------------|
| ![draw](https://i.imgur.com/iARL6Vo.gif) | ![pdf](https://i.imgur.com/NnrqOQQ.gif) | ![grid](https://i.imgur.com/YaEdx1p.gif) |


---

## 🚀 Getting Started

### ✅ Requirements

- Windows 7/8/10/11/Mac OS 10.13+/Ubuntu amd64/Kali amd64/PostmarketOS arm64
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

Notice that the Mac version is VERY UNDERTESTED and I even had to compile poppler-qt6 myself to make it work. 
ZL and L buttons on Nintendo Switch Joy-Con aren't mapped to anything. 

For now, the Mac version is only a proof of concept and It may not work properly. 

1. Install **Qt 6** and **CMake**
2. Clone this repository
3. Run:

```bash
./build_poppler_qt6.sh
./compile.sh
