# OutGuess — native build for Windows 11

This project adapts the **OutGuess** source code for native compilation and
execution on Windows 11 (x86-64), without requiring WSL, Cygwin or the MSYS2
runtime on the machine that runs the binaries.

> **Important:** the MSYS2 environment is used to build, but it is not a
> requirement to run the distributed binaries once they are linked as the
> project's build does.

## Goal of the adaptation

The portability layer is kept separate from the main logic. The intent is to
preserve OutGuess's behavior and change only what is necessary for the Windows
environment.

Among the compatibility points are:

| Area | Unix/BSD environment | Windows adaptation |
| --- | --- | --- |
| File reading | `mmap()` when available | `read()`-based fallback when `HAVE_MMAP` is undefined |
| Arguments | `getopt()` from `<unistd.h>` | Compatible implementation in `src/win32/getopt.c` |
| Byte order | `htonl()` / `ntohl()` | Implementation in `src/win32/compat.h` |
| BSD types | Types from `<sys/types.h>` | Compatible definitions based on `<stdint.h>` |
| Binary files | Unix APIs | Use of `O_BINARY` to avoid CRT data translation |
| `strcasecmp()` | glibc/BSD | Mapping to `_stricmp()` in MSVCRT |
| JPEG | Unix configuration | Windows configuration of the JPEG library |

The specific adaptation is concentrated mainly in:

- `src/win32/compat.h`
- `src/win32/getopt.c`
- `src/config.h`

## Prebuilt binaries

The distributed binaries live in `bin-win64/`.

When a release provides prebuilt executables, the end user can simply download
the package from the Releases page and run the compatible binaries without
installing a development environment.

The GUI executable must stay in the same folder as `outguess.exe`:

```text
bin-win64/
├── outguess.exe
└── outguess-gui.exe
```

> The availability and contents of the binaries depend on the published
> release. Check the corresponding release notes before distributing or
> automating a download.

## Build requirements

### Windows + MSYS2

1. Install [MSYS2](https://www.msys2.org/).
2. Open **MSYS2 MinGW 64-bit**.
3. Update the environment:

```bash
pacman -Syu
```

If MSYS2 asks to close the terminal, close it, reopen **MSYS2 MinGW 64-bit**
and continue:

```bash
pacman -Su
pacman -S --needed mingw-w64-x86_64-gcc make
```

### Build

From the project root:

```bash
cd /c/path/OutGuess-for-Windows/src
make -f Makefile.mingw
```

The generated artifacts depend on the rules in `Makefile.mingw`. In the
documented project configuration, the following are produced:

- `outguess.exe`
- `histogram.exe`
- `outguess-gui.exe`
- `capacity.exe`
- `metadata.exe`
- `batch.exe`

## Run the GUI

In PowerShell, go to the directory where the executables were generated:

```powershell
cd "C:\path\OutGuess-for-Windows\src"
Start-Process ".\outguess-gui.exe"
```

Keep `outguess.exe` in the same directory as the GUI. The secret key is
optional; when the field is left empty, the GUI omits `-k` and the executable
uses the `Default key`, as in the original OutGuess.

### Analysis and batch tools

Check an image's capacity without producing output:

```powershell
.\capacity.exe -e -p 85 .\image.jpg
```

Inspect dimensions, components and JPEG markers:

```powershell
.\metadata.exe .\image.jpg
```

Process several images using Windows wildcards:

```powershell
.\batch.exe embed -e -p 85 -k "password" -m .\message.txt -i ".\images\*.jpg" -o .\output
.\batch.exe extract -e -k "password" -i ".\output\*.jpg" -o .\extracted
```

`batch.exe` creates the output directory when needed and reports the number of
processed files, successes and failures. The `-k` option is also optional;
without it, the OutGuess default key is used. The `-f` option allows
overwriting existing outputs; without `-f`, those files are skipped.

The GUI offers the following features:

- Type a message directly or select a message file.
- Drag and drop an image or file onto the window.
- History of the last used paths in the file fields.
- Button to query an image's capacity before embedding.
- Button to copy the log to the clipboard.
- JPEG quality between 75 and 100.
- Iteration limit between 0 and 65535.
- Progress bar and status during long operations.
- Buttons to clear the log and open the result folder.
- Overwrite protection enabled by default, with confirmation before the operation.
- High-DPI support via the Windows manifest.

The quality and iteration options are converted to the traditional OutGuess
parameters `-p` and `-i`. The effective capacity of the image is still computed
by the executable and shown in the log, since it depends on the JPEG content
and the image statistics.

## Command-line usage

The basic flow remains compatible with the traditional OutGuess syntax.

### Embed

```powershell
.\outguess.exe -k "my-key" -d ".\message.txt" ".\image.jpg" ".\output.jpg"
```

### Extract

```powershell
.\outguess.exe -k "my-key" -r ".\output.jpg" ".\recovered-message.txt"
```

### Help

```powershell
.\outguess.exe -h
```

For the complete option reference, see
[`man/outguess.txt`](man/outguess.txt).

## Cross-compilation

On Debian/Ubuntu, install the toolchain:

```bash
sudo apt install mingw-w64 make
```

Then:

```bash
cd src

make -f Makefile.mingw   CC=x86_64-w64-mingw32-gcc   AR=x86_64-w64-mingw32-ar   RANLIB=x86_64-w64-mingw32-ranlib
```

The result depends on the rules in `Makefile.mingw`.

## Tests

The integration tests can be run in the POSIX environment:

```bash
make check
```

An important test to validate the steganography flow is the **round-trip**:

```text
original file
      ↓
    embed
      ↓
   image
      ↓
   extract
      ↓
recovered file
```

The recovered file should be compared byte by byte with the original.

## Troubleshooting

### `make: command not found`

You are probably in the wrong MSYS2 terminal. Open **MSYS2 MinGW 64-bit** and
install:

```bash
pacman -S --needed mingw-w64-x86_64-gcc make
```

### `gcc: command not found`

Check that the MinGW-w64 package was installed and that the terminal used is
**MSYS2 MinGW 64-bit**, not the common MSYS terminal.

### The GUI does not start

Confirm that the two executables are together:

```text
outguess.exe
outguess-gui.exe
```

Also try running it directly in PowerShell:

```powershell
.\outguess-gui.exe
```

### The JPEG image behaves unexpectedly

Do not open/re-save the image in an editor that recompresses the JPEG between
embedding and extraction. Preserve the output file produced by OutGuess.

## Relevant structure

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

## License

BSD, as in the original project. See `LICENSE` and the source-file headers.
The original OutGuess was developed by Niels Provos and later maintained by the
open-source community.
