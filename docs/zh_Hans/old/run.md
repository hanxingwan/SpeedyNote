# Windows 下潜在的问题

## 找不到`xx.dll`

对于此问题，建议您往系统 Path 中添加 dll 库的位置。

例如，如果缺少的是`QT.xxx.dll`，添加您 QT 的 Mingw 安装位置到系统 Path 即可。

如果您缺少的是 `Poppler.xx.dll`，添加 `msys\\mingw64\\bin`，如果您用的不是mingw，路径需要微调。

### 对于 SDL2 的特殊操作。

由于 SDL2 只有一个dll，建议将 `SDL2.dll` 移动到编译目录即可。

## 无法定位程序入口点……于动态链接库上。

此问题由 ABI 引起，VC 生成的文件和 Mingw 生成的文件 ABI 底层并不相同。

只需要将 dll 全部换成 Mingw 或者 MSVC（如果您用的是这个工具链的话）即可。

注意，操作系统定义的 Path 也需要查看