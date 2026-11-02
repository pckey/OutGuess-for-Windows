# OutGuess for Windows

**OutGuess** 的 Windows 64 位移植版，支持命令行，并提供原生的 Win32 图形界面，覆盖最常用的隐写工作流程。

> **使用须知：** 请仅在你自己拥有或已获授权的文件上使用本工具。隐写不能替代加密；若内容敏感，请先妥善加密再隐藏。

## 下载

前往 [Releases](https://github.com/pckey/OutGuess-for-Windows/releases) 页面下载最新版本。

完整的 Windows 使用说明请见 [README-WINDOWS.zh-CN.md](README-WINDOWS.zh-CN.md)。

## 功能特性

- 在 JPEG 图片中隐藏与提取文件。
- 支持 PNM、PPM 图片（与 OutGuess 一致）。
- 使用密钥对载荷进行编码。
- 通过 `-e` 选项启用 Golay 纠错。
- 保留 JPEG 的频率统计特征。

`-k` 选项为可选项。省略时，OutGuess 使用与原版兼容的默认密钥：

```powershell
.\outguess.exe -d ".\message.txt" ".\image.jpg" ".\output.jpg"
```

- Windows 64 位可执行文件。
- 原生 Win32 图形界面。
- 使用 MinGW-w64 编译。
- 命令行与传统 OutGuess 用法兼容。

## 快速开始

预编译好的二进制文件位于 `bin-win64/` 目录。

### 隐藏文件

```powershell
cd "C:\path\OutGuess-for-Windows\bin-win64"

.\outguess.exe -k "my-key" -d ".\message.txt" ".\image.jpg" ".\output.jpg"
```

### 提取文件

```powershell
.\outguess.exe -k "my-key" -r ".\output.jpg" ".\recovered-message.txt"
```

提取时使用的密钥必须与隐藏时完全一致。输入文件与目标目录必须已存在。

密钥与纠错选项：密钥留空时，使用原版 OutGuess 的默认密钥。

```powershell
.\outguess.exe -h
```

## 命令行语言

所有命令行工具（`outguess`、`histogram`、`capacity`、`metadata`、`batch`）默认以**简体中文**输出信息。如需切换为英文，可加 `-L en`（或 `--lang en` / `--lang=en`）：

```powershell
.\outguess.exe -L en -k "my-key" -d ".\message.txt" ".\image.jpg" ".\output.jpg"
.\capacity.exe --lang=en -e -p 85 .\image.jpg
```

也可以通过环境变量设置，无需改动命令行：

```powershell
$env:OUTGUESS_LANG = "en"   # Windows PowerShell
# 或 LC_ALL / LANG 取含 en（英文）/ zh（CN，中文）的值
```

优先级：`-L` / `--lang` 命令行选项优先于 `OUTGUESS_LANG`、`LC_ALL`、`LANG` 环境变量。两个 GUI 程序（`outguess-gui.exe` 为中文、`outguess-gui-en.exe` 为英文）界面语言固定，不受此选项影响。

## 图形界面

构建会产出**两个**逻辑完全相同、仅界面语言不同的 GUI 程序：

- `outguess-gui.exe` —— 简体中文界面。
- `outguess-gui-en.exe` —— 英文界面（源码为纯 ASCII，在任何语言版本的
  Windows 上都能零字体/代码页风险地正确显示，可作为对照与回退）。

两者均使用 Unicode（W）版 Win32 接口，因此即使是英文版，也能在任意语言的
Windows 上正确处理中文或其它非 ASCII 的文件路径与密钥。按个人喜好任选其一，
但都必须与 `outguess.exe` 放在同一文件夹。

编译完成后，请将 `outguess-gui.exe`（或 `outguess-gui-en.exe`）与
`outguess.exe` 放在同一文件夹中。

```powershell
Start-Process ".\outguess-gui.exe"
```

图形界面可让你选择图片、待隐藏的文件、输出位置、密钥以及纠错选项。

## 编译

完整的安装、依赖、编译与排错说明请见 [README-WINDOWS.zh-CN.md](README-WINDOWS.zh-CN.md)。

使用 MSYS2 MinGW64 的简要步骤：

```bash
pacman -Syu
pacman -Su
pacman -S --needed mingw-w64-x86_64-gcc make

cd /c/path/OutGuess-for-Windows/src
make -f Makefile.mingw
```

编译产物依当前 `Makefile.mingw` 的配置生成在 `src/` 目录下。

## GitHub Actions

`.github/workflows/build-windows.yml` 工作流会在 Windows 上使用 MSYS2 MinGW64 自动编译可执行文件，并生成包含二进制文件与文档的 `outguess-windows.zip`。

- 每次向 `main` 推送或提交 Pull Request 时，都可在 **Actions > Artifacts** 中获取该 ZIP。
- 要创建自动 Release，请推送一个版本标签：

```powershell
git tag v1.0.2
git push origin v1.0.2
```

工作流会创建 Release 并附上 ZIP，其中包含 `outguess.exe`、`outguess-gui.exe`、`histogram.exe`、`capacity.exe`、`metadata.exe` 与 `batch.exe`。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `src/` | OutGuess 源码与构建文件 |
| `src/jpeg-6b-steg/` | 修改后的 JPEG 6b 库 |
| `src/win32/` | Windows 兼容层 |
| `bin-win64/` | 二进制文件与 GUI 源码 |
| `tests/` | 测试脚本与文件 |
| `README-WINDOWS.zh-CN.md` | 详细的 Windows 指南 |
| `man/outguess.txt` | 命令行参考 |

## 测试

集成测试使用项目的构建环境：

```bash
make check
```

基本的往返（round-trip）检查要求：隐藏的文件在 `embed` 与 `extract` 之后应原样恢复。

## 辅助工具

Windows 构建还会生成用于分析与批量处理的工具：

```text
capacity.exe -e -p 85 image.jpg
metadata.exe image.jpg
batch.exe embed -e -p 85 -k "password" -m .\message.txt -i ".\images\*.jpg" -o .\output
batch.exe extract -e -k "password" -i ".\output\*.jpg" -o .\extracted
```

`capacity.exe` 在不改动图片的前提下估算可用比特数与纠错后的容量。`metadata.exe` 显示尺寸、分量、色彩空间以及 JFIF、EXIF、IPTC、XMP 标记。`batch.exe` 按通配符批量处理文件，并报告成功与失败的总数。批量模式下：`-e` 启用纠错，`-p` 设置 JPEG 质量，`-f` 允许覆盖已有文件；不使用 `-f` 时跳过已存在的输出。

## 兼容性与可移植性

Windows 移植将系统相关改动集中在 `src/win32/`，使 OutGuess 主逻辑与兼容层分离。

关于 `getopt()`、字节序、BSD 类型、文件二进制模式以及 JPEG 库配置的更多技术细节，请见 [README-WINDOWS.zh-CN.md](README-WINDOWS.zh-CN.md)。

## 由 AI 驱动 / AI 辅助开发

本项目是一个实际示例，展示人工智能如何用于加速遗留软件的现代化、可移植化与逆向工程。

为了让原版 **OutGuess**（历史上依赖 Unix 生态）原生地运行在现代 Windows 11 生态中，开发过程在以下方面借助了 AI：

* **代码翻译与重构：** 将 POSIX 特有的系统调用改写为 Windows 原生 API（`Win32`/`MSVC`）的等价实现。
* **遗留依赖处理：** 将遗留的构建文件（如经典 *Makefile*）自动转换为适配 Windows 环境的现代构建结构。
* **验证与测试用例：** 基于已知隐写模式自动生成测试脚本，确保原生移植的密码学行为与原版 Linux 版 100% 一致。

借助 AI，分析 1999/2001 年原始代码行为、规避指针冲突、以及在 Windows 上安全高效地操作二进制文件所需的时间被大幅缩短。

## 许可证与致谢

本项目采用 BSD 许可证，详见 [LICENSE](LICENSE) 与源码文件头。

OutGuess 最初由 **Niels Provos** 开发，现由 [Resurrecting Open Source Projects](https://github.com/resurrecting-open-source-projects/outguess) 社区维护。

本项目还包含 JPEG 6b、Arc4、MD5 以及 Golay 纠错组件。贡献方式请见 [CONTRIBUTING.md](CONTRIBUTING.md)。
