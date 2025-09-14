# SpeedyNote Windows 构建

### 您需要准备的东西

- Windows 10 1809+ 或者 Windows 11

- MSYS2 官方的 AMD64版

- Qt 在线安装器

- InnoSetup

---

### 准备环境

##### Qt Online Installer

免费注册Qt账户后下载Qt在线安装器，登录后，选择自定义安装，然后你仅需要这一样东西，仅勾选Qt最新版（当前是6.9.2）下面的`MinGW 13.1.0 64-bit` 这一个东西，和下面强制安装的Qt Maintenance Tool（这个不需要勾选），稍作等待即可。无需重启电脑。

##### MSYS2

下载安装就可以了，安装在默认位置无需变化。  

安装这样几个软件包

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
pacman -S mingw-w64-x86_64-poppler
pacman -S mingw-w64-x86_64-poppler-qt6
pacman -S mingw-w64-x86_64-SDL2
```

##### 额外的文件

在Qt的默认安装位置`C:\Qt\6.9.2\mingw_64\cmake`下创建一个文件夹，名叫`poppler`，然后把SpeedyNote源码根目录的`FindPoppler.cmake` 给放进去。

##### Path

下列几个路径需要添加到Path中。

```cmd
C:\msys64\mingw64\bin
C:\Qt\Tools\mingw1310_64\bin
```

然后重启计算机

##### 其它依赖

去社区 `QQ 744918470` 下载需要的`dllpack` 和 `share` 两个压缩包，解压后放到SpeedyNote的根目录。因为我不确定这东西能不能合规分发，所以我选择不把它们放在GitHub repository里。如果不想来社区也可以从自解压安装包里抠，反正也是一样的。

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

就像这样。注意两个文件夹的位置。

---

### 编译

运行 `compile.ps1`即可完成编译。编译后的内容在`build`文件夹中。你可以选择将一些CMake产生的临时文件删去来减小磁盘占用。另外`opengl32sw.dll`也可以删去。只要你有显卡驱动，这东西就没用。

---

### 打包

使用`InnoSetup`进行打包。您只需要按播放按钮，它就能自动将`build`文件夹中的一切打包为安装包，并修改注册表来添加打开`PDF` 和 `spn`的选项。
