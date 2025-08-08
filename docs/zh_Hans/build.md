# 写在前面

倘若您翻到了这里，相比您希望为这个项目做一些贡献……至少，您懂得 C++ 。

如果您可以在一定程度上协助我们，我们感激不尽。

## 编译

当然，倘若您对 C++ 足够熟悉，可以随意调整。

### 拉取代码

随意找一个好地方，就地拉取即可。

```shell
git clone https://github.com/alpha-liu-01/SpeedyNote.git
```

如果可以的话，您也可以使用`ssh`拉取。

```shell
git clone git@github.com:alpha-liu-01/SpeedyNote.git
```

当然，您也可以尝试使用`浅层拉取`，加上`--depth 1`参数，不再赘述。如果您有一个趁手的编辑器，或者是集成开发工具，便是更好。

### 补充依赖

#### Windows

##### Poppler

如果您是 Windows，我们强烈建议您使用`MSYS2`配置所需的东西。至少，您需要一个`Unix-Like`环境，不想装`MSYS2`，也可以考虑`Windows Subsystem Linux`。如果您有一个与`QT6`绑定好的`Poppler`，考虑跳过此步骤。

于此，我们建议您使用`Unix-Like`环境工具链，例如`GNU Toolchain`或者`LLVM Toolchain`。对于后者，我们还没有测试过。**不要使用`MSVC`工具链以增加不必要的麻烦。或者说，对于`MSVC`，我们还没成功过。**

此处，假设您使用`MSYS2`配置所需依赖。

使用`msys-mingw`。对于`msys-ucrt`或`mingw-clang`，我们还没试过。

```shell
pacman -S mingw-w64-x86_64-poppler mingw-w64-x86_64-poppler-qt6
```

##### SDL2

您可以在 [此处下载](https://github.com/libsdl-org/SDL/releases)，请选择 SDL 版本号为 2.x.x 的包，请下载包含 Mingw 字样的包，例如：`SDL2-devel-2.32.8-mingw.zip`

> 如果您使用`MSVC`工具链，请下载带有 VC 字样的包。

##### QT

对于`QT`，可前往此处下载[QT](https://qt.io)，请按照步骤下载及安装。

对于可选的包，请安装`MINGW`字样的包。

> 如果您使用`MSVC`工具链，请安装带有 MSVC 字样的包。

#### Linux

安装以下包：

```text
cmake
qt6-base
qt6-multimedia
poppler-qt6
sdl2-compat
pkg-config
ninja # 可选，或替换成 make
qt6-tools
sdl2-compat
```

包的名字可能随不同包管理器而不同。

### 编译

尝试使用

```shell
cmake -B build -DQT_PATH={QT-PATH} -DSDL2_ROOT={SDL2-ROOT} -DPOPPLER_PATH={POPPLER-ROOT}
```

倘若将此项目就此导入IDE，可选择跳过命令行编译。

如果提示`Can't find FindPoppler.cmake`，手动补全`FindPoppler.cmake`即可，[具体于此](./FindPoppler.cmake)。

随即构建即可。

对于Windows，如果想制作成安装程序，可以考虑使用`InnerSetup`。