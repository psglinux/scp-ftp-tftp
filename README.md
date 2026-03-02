# GotiKinesis (GK)

Cross-platform desktop file transfer utility supporting **SCP**, **FTP**, and **TFTP** protocols.

*Goti (গতি) — speed* + *Kinesis (κίνησις) — motion*

Built with C++ 17, Qt (5.15+ / 6.x), and libcurl.

---

## Quick Start

```bash
# Linux / macOS
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
./gotikinesis            # GUI mode
./gotikinesis --help     # CLI options
```

---

## Dependencies

| Library | Purpose | Ubuntu/Debian | macOS (Homebrew) | Windows (vcpkg) |
|---------|---------|---------------|------------------|-----------------|
| Qt 6 (or 5.15+) | GUI | `sudo apt install qt6-base-dev` | `brew install qt` | `vcpkg install qt` |
| libcurl + SSH | SCP & FTP | `sudo apt install libcurl4-openssl-dev` | `brew install curl` | `vcpkg install curl[ssh]` |
| CMake 3.16+ | Build | `sudo apt install cmake` | `brew install cmake` | `winget install cmake` |

---

## Building on Windows 11

### Prerequisites

1. **Visual Studio 2022** — "Desktop development with C++" workload
2. **Qt 6** — [qt.io](https://www.qt.io/download-qt-installer) or vcpkg
3. **libcurl** — `vcpkg install curl[ssh]:x64-windows`
4. **CMake** — included with VS or `winget install cmake`
5. **NSIS 3** (for installer) — `winget install NSIS.NSIS`

### Build + Run

```powershell
set CMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2022_64
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build . --config Release --parallel
Release\gotikinesis.exe
```

### Create NSIS Installer

```powershell
set CMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2022_64
packaging\windows\build_installer.bat
```

The installer provides:
- Install to `C:\Program Files\GotiKinesis`
- Branded icon on `.exe`, Start Menu, Desktop, and installer UI
- Add/Remove Programs entry with full uninstaller

### Manual Install / Uninstall

```powershell
packaging\windows\install.bat       # Run as Administrator
packaging\windows\uninstall.bat     # Run as Administrator
```

---

## Building on Linux (Debian / Ubuntu)

```bash
sudo apt install cmake qt6-base-dev libcurl4-openssl-dev libgl1-mesa-dev
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
ctest --output-on-failure
cpack -G DEB
```

### Install / Uninstall

```bash
sudo dpkg -i GotiKinesis-1.0.0-Linux.deb
sudo dpkg -r gotikinesis
```

---

## Building on macOS

```bash
brew install cmake qt curl
packaging/macos/build_dmg.sh
```

---

## CLI Usage

```bash
gotikinesis --cli --protocol scp --host 192.168.1.10 \
    --user admin --password secret \
    --upload --local firmware.bin --remote /opt/firmware.bin

gotikinesis --cli --protocol ftp --host ftp.example.com \
    --download --local ./data.csv --remote /pub/data.csv

gotikinesis --cli --protocol tftp --host 10.0.0.1 \
    --upload --local image.bin --remote image.bin
```

---

## Project Structure

```
CMakeLists.txt                     Build + CPack packaging
LICENSE                            MIT license
src/
  main.cpp                         Entry point (CLI + GUI routing)
  mainwindow.h / .cpp              Qt GUI (QMainWindow)
  transferengine.h / .cpp          SCP/FTP via libcurl, async worker
  tftpclient.h / .cpp              TFTP client (RFC 1350, QUdpSocket)
tests/
  mock_tftp_server.h               Mock TFTP server
  test_tftp_packets.cpp            20 packet encoding tests
  test_transferengine.cpp          22 engine logic tests
  test_tftp_integration.cpp        22 integration tests
  test_mainwindow.cpp              32 GUI widget tests
packaging/
  generate_icons.py                Icon / bitmap generator
  logo.png                         Full GotiKinesis logo
  linux/
    gotikinesis.desktop            Desktop entry
    gotikinesis_*.png              App icons (48/128/256)
    postinst / postrm              Debian scripts
  windows/
    app.ico                        Multi-size Windows icon
    gotikinesis.rc                 Version resource
    nsis_header.bmp / nsis_welcome.bmp   Installer graphics
    build_installer.bat            Build + NSIS packaging
    install.bat / uninstall.bat    Manual install/uninstall
  macos/
    Info.plist.in                  Bundle metadata
    build_dmg.sh                   Build + DMG packaging
```

## License

MIT
