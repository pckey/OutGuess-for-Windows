# OutGuess for Windows

A port of **OutGuess** to Windows 64-bit, with command-line support and a
native Win32 graphical interface for the most common steganography workflows.

> **Responsible use:** only use it on files you own or are authorized to modify.
> Steganography is not a substitute for encryption; if the content is sensitive,
> protect it appropriately before hiding it.

## Download

Download the latest release from the
[Releases](https://github.com/pckey/OutGuess-for-Windows/releases) page.

For complete Windows instructions, see
[README-WINDOWS.md](README-WINDOWS.md).

## Features

- Hide and extract files inside JPEG images.
- PNM and PPM image support, as in OutGuess.
- Secret key for payload encoding.
- Golay error correction with the `-e` option.
- JPEG frequency-statistics preservation.

The `-k` option is optional. When omitted, OutGuess uses the default key
compatible with the original version:

```powershell
.\outguess.exe -d ".\message.txt" ".\image.jpg" ".\output.jpg"
```

- Windows 64-bit executables.
- Native Win32 graphical interface.
- Built with MinGW-w64.
- Command-line compatible with traditional OutGuess usage.

## Quick start

Prebuilt binaries live in `bin-win64/`.

### Hide a file

```powershell
cd "C:\path\OutGuess-for-Windows\bin-win64"

.\outguess.exe -k "my-key" -d ".\message.txt" ".\image.jpg" ".\output.jpg"
```

### Extract a file

```powershell
.\outguess.exe -k "my-key" -r ".\output.jpg" ".\recovered-message.txt"
```

The key used for extraction must be exactly the same as the one used for
embedding. The input files and the destination directories must already exist.

The key and the error-correction option: if the key is left empty, the default
key of the original OutGuess is used.

```powershell
.\outguess.exe -h
```

## Graphical interface

After building, keep `outguess-gui.exe` and `outguess.exe` in the same folder.

```powershell
Start-Process ".\outguess-gui.exe"
```

The GUI lets you select the image, the file to hide, the destination, the key,
and the error-correction option.

## Building

For complete installation, dependency, build, and troubleshooting instructions
on Windows, see [README-WINDOWS.md](README-WINDOWS.md).

Quick summary with MSYS2 MinGW64:

```bash
pacman -Syu
pacman -Su
pacman -S --needed mingw-w64-x86_64-gcc make

cd /c/path/OutGuess-for-Windows/src
make -f Makefile.mingw
```

The build produces the project's executables in `src/`, according to the
current `Makefile.mingw` configuration.

## GitHub Actions

The workflow in `.github/workflows/build-windows.yml` automatically builds the
executables on Windows using MSYS2 MinGW64 and produces `outguess-windows.zip`
with the binaries and documentation.

- On every push to `main` or pull request, the ZIP is available under **Actions > Artifacts**.
- To create an automatic Release, push a version tag:

```powershell
git tag v1.0.2
git push origin v1.0.2
```

The workflow creates the Release and attaches the ZIP containing `outguess.exe`,
`outguess-gui.exe`, `histogram.exe`, `capacity.exe`, `metadata.exe` and `batch.exe`.

## Structure

| Path | Description |
| --- | --- |
| `src/` | OutGuess source code and build files |
| `src/jpeg-6b-steg/` | Modified JPEG 6b library |
| `src/win32/` | Windows compatibility layer |
| `bin-win64/` | Binaries and GUI source code |
| `tests/` | Test scripts and files |
| `README-WINDOWS.md` | Detailed Windows guide |
| `man/outguess.txt` | Command-line reference |

## Tests

The integration tests use the project's build environment:

```bash
make check
```

For a basic round-trip check, the hidden file should be recovered unchanged
after `embed` and `extract`.

## Auxiliary tools

The Windows build also produces utilities for analysis and batch processing:

```text
capacity.exe -e -p 85 image.jpg
metadata.exe image.jpg
batch.exe embed -e -p 85 -k "password" -m .\message.txt -i ".\images\*.jpg" -o .\output
batch.exe extract -e -k "password" -i ".\output\*.jpg" -o .\extracted
```

`capacity.exe` estimates the usable bits and the capacity with error correction
without modifying the image. `metadata.exe` shows dimensions, components, color
space and JFIF, EXIF, IPTC and XMP markers. `batch.exe` processes files matched
by wildcards and reports the total of successes and failures. In batch mode,
`-e` enables error correction, `-p` sets the JPEG quality and `-f` allows
overwriting existing files; without `-f`, existing outputs are skipped.

## Compatibility and portability

The Windows adaptation concentrates system-specific changes in `src/win32/` and
keeps the main OutGuess logic separate from the compatibility layer.

See [README-WINDOWS.md](README-WINDOWS.md) for technical details about
`getopt()`, byte order, BSD types, binary file mode and JPEG library
configuration.

## Powered by AI / AI-assisted development

This project is a practical example of how Artificial Intelligence can be used to
accelerate the modernization, portability and reverse engineering of legacy
software.

To bring the original **OutGuess** (historically dependent on the Unix
ecosystem) natively to the modern Windows 11 ecosystem, the development was
assisted by AI in the following areas:

* **Code translation and refactoring:** adapting POSIX-specific system calls to
  native Windows API equivalents (`Win32`/`MSVC`).
* **Legacy dependency resolution:** automated conversion of legacy build files
  (such as classic *Makefiles*) into modern build structures compatible with the
  Windows environment.
* **Validation and test cases:** generation of automated scripts based on known
  steganography patterns to ensure that the cryptographic behavior of the native
  port is 100% identical to the original on Linux.

The use of AI drastically reduced the time needed to analyze the behavior of the
original 1999/2001 code, work around pointer conflicts and safely and
efficiently handle binary file manipulation on Windows.

## License and credits

The project is under the BSD license. See [LICENSE](LICENSE) and the source-file
headers.

OutGuess was originally developed by **Niels Provos** and is maintained by the
[Resurrecting Open Source Projects](https://github.com/resurrecting-open-source-projects/outguess)
community.

The project also incorporates JPEG 6b, Arc4, MD5 and Golay error-correction
components. See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution information.
