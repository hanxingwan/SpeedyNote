# SpeedyNote Windows 构建

### 您需要准备的东西

- Windows 10 1809+ 或者 Windows 11

- MSYS2 官方的 AMD64版

- InnoSetup

---

### 准备环境

##### Qt Online Installer

你已经不需要使用Qt Online Installer了。

##### MSYS2

下载安装就可以了，安装在默认位置无需变化。  

安装这样几个软件包

```bash
pacman -S mingw-w64-clang-x86_64-toolchain mingw-w64-x86_64-cmake
pacman -S mingw-w64-clang-x86_64-qt6-base mingw-w64-clang-x86_64-qt6-tools
pacman -S mingw-w64-clang-x86_64-poppler
pacman -S mingw-w64-clang-x86_64-poppler-qt6
pacman -S mingw-w64-clang-x86_64-SDL2
```
如果你的机器是arm64的，那么你把上边命令中的`x86_64` 更换为`aarch64`。

##### 额外的文件

在你需要在`C:\msys64\clang64\lib\cmake` （对于arm的机器来说是`C:\msys64\clangarm64\lib\cmake` ）中创建一个名为`poppler` 的文件夹，然后你把
 `FindPoppler.cmake` 这个文件放进去。如果你的机器是arm64 CPU的，那么记得把文件内的路径改一下，改成clangarm64. 

##### Path

下列几个路径需要添加到Path中。

```cmd
C:\msys64\clang64\bin
```
arm64机器的改动同理。

然后重启计算机

##### 其它依赖

去社区 `QQ 744918470` 下载需要的 `share` 压缩包，这里边装的是中文的字体，解压后放到SpeedyNote的`NoteApp.exe`同文件夹里。因为我不确定这东西能不能合规分发，所以我选择不把它们放在GitHub repository里。如果不想来社区也可以从自解压安装包里抠，反正也是一样的。

---

### 编译

运行 `compile.ps1`即可完成编译。编译后的内容在`build`文件夹中。你可以选择将一些CMake产生的临时文件删去来减小磁盘占用。

---

### 打包

使用`InnoSetup`进行打包。您只需要按播放按钮，它就能自动将`build`文件夹中的一切打包为安装包，并修改注册表来添加打开`PDF` 和 `spn`的选项。
