# eufymake-studio-windows-thumbnails

Windows Explorer thumbnails and Preview-pane support for eufyMake Studio `.empf` project files.

Double-click still opens eufyMake Studio. This project only adds a visual handler so folders of `.empf` files are recognizable without launching Studio.

**License:** [Apache License 2.0](LICENSE)

## Features

- Explorer thumbnails for `.empf` files
- Explorer Preview pane (`Alt+P`)
- Works with older ZIP-style `.empf` files and current Studio wrapped files
- Per-user registration — no administrator account required
- Does not take over the `.empf` file type from eufyMake Studio

## Requirements

- Windows 10/11 64-bit
- [CMake](https://cmake.org/) 3.20+
- Visual Studio 2022 or 2026 with the C++ desktop workload

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Outputs:

| File | Purpose |
|------|---------|
| `build\Release\EmpfThumbs.dll` | Explorer thumbnail + preview handler |
| `build\Release\empf-extract.exe` | Extract the embedded preview PNG from a file |

## Install

```powershell
.\scripts\register.ps1
```

Then open a folder of `.empf` files in large or extra-large icons. Turn on the Preview pane with `Alt+P`.

If Explorer still shows generic icons, press `F5` or reopen the folder so the thumbnail cache refreshes.

## Uninstall

```powershell
.\scripts\unregister.ps1
```

## Extract a preview without Explorer

```powershell
.\build\Release\empf-extract.exe "C:\path\to\file.empf" .\preview.png
```

## How it works

`.empf` project files are ZIP archives. Current eufyMake Studio exports wrap that ZIP in an `eufyMake` header. The handler unwraps the archive and shows `Asset/images/thumbnail.png` when present, otherwise the largest PNG in the file.

Format notes follow the MIT [empf-web-preview](https://github.com/Davidobot/empf-web-preview) parser. ZIP inflate is [miniz](vendor/miniz) (MIT).

## Credits

- [Davidobot/empf-web-preview](https://github.com/Davidobot/empf-web-preview) — published EMPF layout used for compatibility
- [TapuCosmo/empf-generator](https://github.com/TapuCosmo/empf-generator) — older ZIP-style EMPF shape
- [richgel999/miniz](https://github.com/richgel999/miniz) — ZIP inflate
