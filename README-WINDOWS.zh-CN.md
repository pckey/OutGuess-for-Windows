# OutGuess — Windows 11 原生构建

本项目对 **OutGuess** 源码进行改造，使其能在 Windows 11（x86-64）上原生编译与运行，在运行二进制文件的机器上无需 WSL、Cygwin 或 MSYS2 运行时。

> **重要：** MSYS2 环境仅用于编译；按本项目构建方式链接完成后，分发后的二进制文件运行时不依赖 MSYS2。

## 改造目标

可移植层与主逻辑分离。目的是保留 OutGuess 的行为，仅对 Windows 环境做必要修改。

主要兼容点如下：

| 领域 | Unix/BSD 环境 | Windows 适配 |
| --- | --- | --- |
| 文件读取 | 可用时 `mmap()` | 未定义 `HAVE_MMAP` 时回退到基于 `read()` 的实现 |
| 参数解析 | `<unistd.h>` 的 `getopt()` | `src/win32/getopt.c` 中的兼容实现 |
| 字节序 | `htonl()` / `ntohl()` | `src/win32/compat.h` 中的实现 |
| BSD 类型 | `<sys/types.h>` 的类型 | 基于 `<stdint.h>` 的兼容定义 |
| 二进制文件 | Unix API | 使用 `O_BINARY` 避免 CRT 对数据的转译 |
| `strcasecmp()` | glibc/BSD | 映射到 MSVCRT 的 `_stricmp()` |
| JPEG | Unix 配置 | JPEG 库的 Windows 配置 |

具体改造主要集中在：

- `src/win32/compat.h`
- `src/win32/getopt.c`
- `src/config.h`

## 预编译二进制

分发用的二进制文件位于 `bin-win64/`。

当某个 Release 提供预编译可执行文件时，最终用户只需从 Releases 页面下载压缩包并运行兼容的二进制文件，无需安装开发环境。

GUI 可执行文件必须与 `outguess.exe` 放在同一文件夹：

```text
bin-win64/
├── outguess.exe
└── outguess-gui.exe
```

> 二进制文件的可用性与内容取决于所发布的 Release。在分发或自动化下载前，请先查看对应的 Release 说明。

## 编译要求

### Windows + MSYS2

1. 安装 [MSYS2](https://www.msys2.org/)。
2. 打开 **MSYS2 MinGW 64-bit**。
3. 更新环境：

```bash
pacman -Syu
```

若 MSYS2 要求关闭终端，请关闭后重新打开 **MSYS2 MinGW 64-bit** 并继续：

```bash
pacman -Su
pacman -S --needed mingw-w64-x86_64-gcc make
```

### 编译

从项目根目录：

```bash
cd /c/path/OutGuess-for-Windows/src
make -f Makefile.mingw
```

生成产物取决于 `Makefile.mingw` 中的规则。在本文档所述的项目配置下，会生成：

- `outguess.exe`
- `histogram.exe`
- `outguess-gui.exe`
- `capacity.exe`
- `metadata.exe`
- `batch.exe`

## 运行 GUI

在 PowerShell 中进入生成可执行文件的目录：

```powershell
cd "C:\path\OutGuess-for-Windows\src"
Start-Process ".\outguess-gui.exe"
```

请将 `outguess.exe` 与 GUI 放在同一目录。密钥为可选项；当该字段留空时，GUI 会省略 `-k`，由可执行文件使用 `Default key`，与原版 OutGuess 一致。

### 分析与批量工具

在不产生输出的情况下查看图片容量：

```powershell
.\capacity.exe -e -p 85 .\image.jpg
```

查看尺寸、分量与 JPEG 标记：

```powershell
.\metadata.exe .\image.jpg
```

使用 Windows 通配符批量处理多张图片：

```powershell
.\batch.exe embed -e -p 85 -k "password" -m .\message.txt -i ".\images\*.jpg" -o .\output
.\batch.exe extract -e -k "password" -i ".\output\*.jpg" -o .\extracted
```

`batch.exe` 会在需要时创建输出目录，并报告已处理文件数、成功数与失败数。`-k` 选项同样可选；省略时使用 OutGuess 默认密钥。`-f` 选项允许覆盖已有输出；不使用 `-f` 时跳过这些文件。

GUI 提供以下功能：

- 直接输入消息或选择消息文件。
- 将图片或文件拖放到窗口中。
- 文件字段中保留最近使用过的路径历史。
- 在隐藏前查询图片容量的按钮。
- 将日志复制到剪贴板的按钮。
- JPEG 质量介于 75 与 100 之间。
- 迭代次数限制介于 0 与 65535 之间。
- 长操作期间的进度条与状态显示。
- 清空日志与打开结果文件夹的按钮。
- 默认开启防覆盖保护，并在操作前进行确认。
- 通过 Windows 清单支持高 DPI。

质量与迭代选项会转换为 OutGuess 传统的 `-p` 与 `-i` 参数。图片的有效容量仍由可执行文件计算并显示在日志中，因为它取决于 JPEG 内容与图片统计特征。

## 命令行用法

基本流程与传统 OutGuess 语法兼容。

### 隐藏

```powershell
.\outguess.exe -k "my-key" -d ".\message.txt" ".\image.jpg" ".\output.jpg"
```

### 提取

```powershell
.\outguess.exe -k "my-key" -r ".\output.jpg" ".\recovered-message.txt"
```

### 帮助

```powershell
.\outguess.exe -h
```

完整选项参考请见 [`man/outguess.txt`](man/outguess.txt)。

## 交叉编译

在 Debian/Ubuntu 上安装工具链：

```bash
sudo apt install mingw-w64 make
```

然后：

```bash
cd src

make -f Makefile.mingw   CC=x86_64-w64-mingw32-gcc   AR=x86_64-w64-mingw32-ar   RANLIB=x86_64-w64-mingw32-ranlib
```

结果取决于 `Makefile.mingw` 中的规则。

## 测试

集成测试可在 POSIX 环境中运行：

```bash
make check
```

验证隐写流程的一个重要测试是 **往返（round-trip）**：

```text
原始文件
      ↓
    embed
      ↓
   图片
      ↓
   extract
      ↓
恢复文件
```

恢复出的文件应与原始文件逐字节一致。

## 排错

### `make: command not found`

你可能打开了错误的 MSYS2 终端。请打开 **MSYS2 MinGW 64-bit** 并安装：

```bash
pacman -S --needed mingw-w64-x86_64-gcc make
```

### `gcc: command not found`

检查是否已安装 MinGW-w64 软件包，且所使用的终端是 **MSYS2 MinGW 64-bit**，而不是普通的 MSYS 终端。

### GUI 无法启动

确认两个可执行文件在一起：

```text
outguess.exe
outguess-gui.exe
```

也可以在 PowerShell 中直接运行试试：

```powershell
.\outguess-gui.exe
```

### JPEG 图片表现异常

在隐藏与提取之间，不要用会重新压缩 JPEG 的编辑器打开/另存该图片。请保留 OutGuess 生成的输出文件。

## 相关结构

```text
OutGuess-for-Windows/
├── bin-win64/
├── man/
├── src/
│   ├── jpeg-6b-steg/
│   └── win32/
├── tests/
├── README.md
├── README-WINDOWS.md
├── LICENSE
└── CONTRIBUTING.md
```

## 许可证

采用 BSD 许可证，与原项目一致。详见 `LICENSE` 与源码文件头。原版 OutGuess 由 Niels Provos 开发，后由开源社区维护。
