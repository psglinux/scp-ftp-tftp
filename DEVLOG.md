# GotiKinesis -- Project Replication Guide & Development Log

This file documents every step taken to build, test, package, and release
this project. A developer should be able to replicate the entire project
from scratch by following this document sequentially.

Every time a plan is made and executed, it is recorded here.

---

## Author

| | |
|---|---|
| **Spec author** | psglinux@gmail.com |
| **GitHub** | https://github.com/psglinux |

All references to `<author-email>` in this document refer to the address above.

---

## Table of Contents

1. [Replication Gaps & Manual Steps](#1-replication-gaps--manual-steps)
2. [Prerequisites](#2-prerequisites)
3. [Project Structure](#3-project-structure)
4. [Repository Setup](#4-repository-setup)
5. [Application Architecture](#5-application-architecture)
6. [Building from Source](#6-building-from-source)
7. [Testing](#7-testing)
8. [Packaging](#8-packaging)
9. [CI/CD Pipeline](#9-cicd-pipeline)
10. [Releasing](#10-releasing)
11. [Development Log](#11-development-log)
12. [Time Tracking](#12-time-tracking)
13. [Future Roadmap](#13-future-roadmap)

---

## 1. Replication Gaps & Manual Steps

This section documents what CANNOT be replicated from this file alone.
These items require manual intervention, external service setup, or
access to configuration files stored elsewhere in the repository.

### Gaps in this document

| Gap | Why it's a gap | Where the info lives |
|-----|---------------|---------------------|
| CI workflow contents | Matrix strategy, vcpkg caching, Qt install action config, email step -- too complex to summarize in prose | `.github/workflows/ci.yml`, `.github/workflows/release.yml` |
| Docker container config | Env vars, volume mounts, port mappings, health checks for test servers | `tests/docker-compose.yml` |
| Packaging scripts & assets | `.desktop` file, `Info.plist.in`, `postinst`/`postrm`, `.rc` file, NSIS bitmaps, `release.sh` | `packaging/linux/`, `packaging/windows/`, `packaging/macos/`, `scripts/` |
| GitHub Pages HTML | The download page with dynamic release fetching via GitHub API | `docs/index.html` (in `utility-sw` parent repo) |
| SSH test key pair | Generated key for Docker SCP testing -- not committed for security | Must be regenerated: `ssh-keygen -t ed25519 -f tests/docker/ssh_test_key -N ""` |

### Steps that require external service access

These cannot be scripted or replicated without interactive access to
external services:

| Step | What's needed | How to do it |
|------|--------------|--------------|
| GitHub repo creation | Authenticated `gh` CLI session | `gh auth login` then run repo create commands from Section 4 |
| GitHub Pages enablement | API call after repo is public | `gh api repos/<user>/utility-sw/pages -X POST -f source.branch=main -f source.path=/docs` |
| Gmail App Password | Google account with 2FA enabled | https://myaccount.google.com/apppasswords -- create for "Mail" |
| `SMTP_PASSWORD` secret | App Password from above | `gh secret set SMTP_PASSWORD --repo <user>/scp-ftp-tftp --body "APP_PASSWORD"` |
| `SMTP_USERNAME` secret | Author email address | `gh secret set SMTP_USERNAME --repo <user>/scp-ftp-tftp --body "<author-email>"` |
| `GH_PAT` secret | Fine-grained Personal Access Token with repo scope | Create at https://github.com/settings/tokens then `gh secret set GH_PAT --repo <user>/utility-sw --body "TOKEN"` |
| Windows build verification | Requires a Windows machine or VM | Download `.zip` from release, run `gotikinesis.exe`, test SCP/FTP |
| macOS build verification | Requires a macOS machine or VM | Download `.dmg` from release, open app, test SCP/FTP |

### Information learned through trial-and-error

These are decisions and workarounds discovered during development.
They are documented here so they don't have to be rediscovered:

| Issue | Root cause | Fix applied |
|-------|-----------|-------------|
| Qt 6.7.3 fails to install in CI | `aqtinstall` 3.3.x can't parse Qt 6.7.3 metadata | Pinned to Qt 6.5.3 LTS in CI workflows |
| Windows RC compiler can't find `app.ico` | RC compiler runs from build dir, relative path breaks | Added `$<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}/packaging/windows>` to `target_include_directories` |
| NSIS packaging fails in CI | Bitmap format or path issues in headless environment | CI uses ZIP generator; NSIS reserved for local builds |
| `libcurl.dll` not found on Windows | Default vcpkg triplet builds dynamic libs | Switched to `curl[ssh]:x64-windows-static-md` (static curl, dynamic CRT) |
| CI can't clone private submodule | Default `GITHUB_TOKEN` only has access to current repo | Use `GH_PAT` with repo scope, pass as `token:` to `actions/checkout` |
| Email notification SSL error | Port 587 uses STARTTLS, not direct TLS | Set `secure: false` in `dawidd6/action-send-mail` (STARTTLS negotiates automatically) |

### What an AI assistant cannot do

| Limitation | Reason |
|-----------|--------|
| Test on a real Windows machine | No access to Windows; can only verify CI logs |
| Test on a real macOS machine | No access to macOS; can only verify CI logs |
| Set up Gmail App Password | Requires interactive Google login with 2FA |
| Verify SCP works end-to-end on Windows | Requires running the app on Windows with a real SSH server |
| Access private repos without PAT | GitHub API requires authentication tokens |
| Debug runtime crashes | No access to a debugger; can only read code and CI logs |
| Measure real-world network performance | Docker tests run on localhost; real network latency not captured |

---

## 2. Prerequisites

### Linux (Ubuntu 22.04+)

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake git \
    qt6-base-dev libqt6network6 libqt6test6-dev \
    libcurl4-openssl-dev libssh-dev \
    docker.io docker-compose-v2
```

### Windows

- Visual Studio 2022 (C++ workload)
- CMake 3.16+
- Qt 6.5+ (install via https://www.qt.io/download-qt-installer)
- vcpkg (for libcurl): https://github.com/microsoft/vcpkg
- Git for Windows
- NSIS 3.x (optional, for .exe installer)

```bat
:: Install curl with SSH support via vcpkg (static, dynamic CRT)
vcpkg install curl[ssh]:x64-windows-static-md
```

### macOS

```bash
brew install cmake qt@6 curl
```

### GitHub CLI (all platforms)

```bash
# Used for repo creation, CI management, releases
# Install: https://cli.github.com/
gh auth login
```

---

## 3. Project Structure

```
utility-sw/                          # Parent repo (github.com/psglinux/utility-sw)
├── .git/
├── .github/workflows/ci.yml        # Parent CI: validates submodule integrity
├── docs/index.html                  # GitHub Pages download site
├── README.md
└── scp-ftp-tftp/                    # Submodule (github.com/psglinux/scp-ftp-tftp)
    ├── .git/
    ├── .github/workflows/
    │   ├── ci.yml                   # Build + test on 3 platforms
    │   └── release.yml              # Tag-triggered release
    ├── CMakeLists.txt
    ├── LICENSE
    ├── DEVLOG.md                    # This file
    ├── src/
    │   ├── main.cpp
    │   ├── mainwindow.h / .cpp      # Qt GUI
    │   ├── transferengine.h / .cpp   # SCP/FTP via libcurl, dispatches TFTP
    │   └── tftpclient.h / .cpp      # Custom TFTP over UDP (RFC 1350)
    ├── tests/
    │   ├── test_tftp_packets.cpp     # TFTP packet encoding unit tests
    │   ├── test_transferengine.cpp   # TransferEngine config/URL/error tests
    │   ├── test_tftp_integration.cpp # TFTP end-to-end with mock server
    │   ├── test_mainwindow.cpp       # GUI widget/state tests
    │   ├── mock_tftp_server.h        # In-process mock TFTP server (QThread)
    │   ├── docker-compose.yml        # Real SCP/FTP/TFTP servers for testing
    │   ├── docker/                   # SSH keys, server configs
    │   ├── test_helpers.h            # Docker health-check, test utilities
    │   ├── test_scp_integration.cpp  # SCP tests against Docker SSH server
    │   ├── test_ftp_integration.cpp  # FTP tests against Docker FTP server
    │   ├── test_tftp_real_integration.cpp # TFTP tests against Docker TFTP server
    │   └── test_performance.cpp      # Transfer speed benchmarks
    ├── packaging/
    │   ├── linux/                    # .desktop, icons, postinst/postrm
    │   ├── windows/                  # .rc, .ico, NSIS bitmaps, build_installer.bat
    │   └── macos/                    # Info.plist.in, build_dmg.sh
    ├── scripts/
    │   └── release.sh               # Local release directory manager
    └── release/                      # Local build output (gitignored except .gitkeep)
```

**Why submodules?** The parent `utility-sw` repo is designed to hold multiple
independent utility projects. Each is its own git repo that can be cloned,
built, and pushed independently. The parent just tracks which commit of each
submodule to use.

---

## 4. Repository Setup

These are the exact commands used to create the repos and link them.

### 4.1 Create the child repo (scp-ftp-tftp)

```bash
cd /home/ghosh/ws/utility-sw/scp-ftp-tftp

# Create GitHub repo
gh repo create psglinux/scp-ftp-tftp --public \
    --description "GotiKinesis - Cross-platform SCP/FTP/TFTP file transfer utility"

# Initialize and push
git init
git add .
git commit -m "Initial commit: GotiKinesis v1.0.0"
git remote add origin git@github.com:psglinux/scp-ftp-tftp.git
git branch -M main
git push -u origin main
```

### 4.2 Create the parent repo (utility-sw)

```bash
cd /home/ghosh/ws/utility-sw

# Create GitHub repo
gh repo create psglinux/utility-sw --public \
    --description "Collection of utility software"

# Initialize with scp-ftp-tftp as a submodule
git init
git submodule add git@github.com:psglinux/scp-ftp-tftp.git scp-ftp-tftp
git add .
git commit -m "Add scp-ftp-tftp (GotiKinesis) as submodule"
git remote add origin git@github.com:psglinux/utility-sw.git
git branch -M main
git push -u origin main
```

### 4.3 Cloning on a new machine

```bash
git clone --recurse-submodules git@github.com:psglinux/utility-sw.git
```

### 4.4 Adding another project later

```bash
cd utility-sw
git submodule add git@github.com:psglinux/new-project.git new-project
git commit -m "Add new-project as submodule"
git push
```

---

## 5. Application Architecture

### Transfer protocols

| Protocol | Implementation | Library | Auth |
|----------|---------------|---------|------|
| SCP | `TransferEngine::transferWithCurl()` | libcurl + libssh2 | Password, SSH key |
| FTP | `TransferEngine::transferWithCurl()` | libcurl | Username/password |
| TFTP | `TransferEngine::transferWithTftp()` → `TftpClient` | Custom (Qt QUdpSocket) | None (RFC 1350) |

### Key design decisions

- **libcurl for SCP/FTP**: single library handles both protocols, widely
  available, supports auth, progress callbacks, and timeouts.
- **Custom TFTP**: libcurl's TFTP support is limited. A direct UDP
  implementation gives full control over retries, timeouts, and progress.
- **Qt for GUI + networking**: Qt provides cross-platform GUI, UDP sockets
  (for TFTP), and a test framework (QtTest) in one package.
- **Worker thread**: transfers run on a `QThread` to keep the GUI responsive.
  Signals (`progressChanged`, `logMessage`, `transferCompleted`) bridge the
  thread boundary.

### Signal flow

```
User clicks Start
  → MainWindow::onStartTransfer()
    → creates QThread, moves TransferEngine to it
    → calls engine->startTransfer(config)
      → transferWithCurl() or transferWithTftp()
        → emits logMessage(...)        → log panel
        → emits progressChanged(...)   → progress bar
        → emits transferCompleted(...) → status + cleanup
```

---

## 6. Building from Source

### Linux / macOS

```bash
cd scp-ftp-tftp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Windows (MSVC + vcpkg)

```bat
set CMAKE_PREFIX_PATH=C:\Qt\6.5.3\msvc2022_64
set VCPKG_ROOT=C:\vcpkg

cd scp-ftp-tftp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build . --config Release
```

**Why `x64-windows-static-md`?** This triplet statically links libcurl, zlib,
and openssl into the executable (no DLLs to ship), while using the dynamic
MSVC runtime (compatible with Qt, which also uses dynamic CRT). Without this,
users get "libcurl.dll not found" errors.

### Running the application

```bash
# Linux
./build/gotikinesis

# Windows
build\Release\gotikinesis.exe

# macOS
open build/gotikinesis.app
```

---

## 7. Testing

### 7.1 Unit tests (no external dependencies)

```bash
cd build
QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

`QT_QPA_PLATFORM=offscreen` allows GUI tests to run without a display server
(required in CI and headless environments).

### 7.2 Test suites as of v0.1.0

| Suite | File | Tests | Coverage |
|-------|------|-------|----------|
| TftpPackets | `test_tftp_packets.cpp` | 20 | TFTP packet encoding: RRQ, WRQ, ACK, DATA formats, UTF-8 filenames, boundary values |
| TransferEngine | `test_transferengine.cpp` | 22 | Config defaults, URL building (SCP/FTP), error signals, cancellation, enum values |
| TftpIntegration | `test_tftp_integration.cpp` | 22 | End-to-end TFTP using in-process mock server: upload, download, empty files, 512-byte boundary, multi-block, cancellation, error injection, progress signals |
| MainWindow | `test_mainwindow.cpp` | 32 | Widget existence, initial state, protocol switching (port changes, field enable/disable/visibility), password obscured, multiple instances |

**Total: 96 tests, all passing on Linux, Windows, macOS.**

### 7.3 Known test gaps (identified in v0.1.0)

- `transferWithCurl()` -- the entire SCP and FTP backend -- has zero
  integration test coverage. No test ever makes a real SCP or FTP connection.
- No test verifies that libcurl was built with SSH support.
- No test verifies authentication works (password or key).
- Log output is not validated in any test.
- No performance benchmarks exist.

### 7.4 Docker-based integration tests (added in v0.1.1)

```bash
# Start test servers
cd tests && docker compose up -d

# Wait for servers to be healthy, then run all tests
cd ../build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --output-on-failure

# Tear down
cd ../tests && docker compose down
```

Tests skip gracefully (via `QSKIP`) if Docker containers are not running,
so unit tests always work without Docker.

---

## 8. Packaging

### Linux (.deb)

```bash
cd build
cpack -G DEB
# Output: GotiKinesis-<version>-Linux.deb
```

Installs to `/usr/bin/gotikinesis` with `.desktop` file and icons.
Uses `dpkg-shlibdeps` to auto-detect shared library dependencies.

### Windows (.zip or NSIS .exe)

```bat
cd build
cpack -G ZIP
:: Output: GotiKinesis-<version>-win64.zip
```

`windeployqt` runs at install time to bundle Qt DLLs into the package.
NSIS installer is configured but currently only works for local builds
(fails in CI due to bitmap format issues).

### macOS (.dmg)

```bash
cd build
cpack -G DragNDrop
# Output: GotiKinesis-<version>-Darwin.dmg
```

---

## 9. CI/CD Pipeline

### 9.1 CI workflow (`.github/workflows/ci.yml`)

Triggered on every push to `scp-ftp-tftp`.

**Matrix:**

| Platform | Runner | Qt source | curl source |
|----------|--------|-----------|-------------|
| Linux | `ubuntu-latest` | `apt install qt6-base-dev` | `apt install libcurl4-openssl-dev` |
| Windows | `windows-latest` | `jurplel/install-qt-action` (Qt 6.5.3) | `vcpkg install curl[ssh]:x64-windows-static-md` |
| macOS | `macos-latest` | `brew install qt@6` | `brew install curl` |

**Steps per platform:** configure → build → test → package → upload artifact.

**Failure notification:** sends email to `<author-email>` via Gmail SMTP
(requires `SMTP_USERNAME` and `SMTP_PASSWORD` secrets).

### 9.2 Release workflow (`.github/workflows/release.yml`)

Triggered on `git tag v*` push.

Same matrix build as CI, plus:
- Creates a GitHub Release
- Attaches platform packages as release assets
- Sends email with download URL to `<author-email>` on success

### 9.3 Parent repo CI (`.github/workflows/ci.yml` in `utility-sw`)

Validates submodule integrity: checks out with `--recurse-submodules`,
verifies each submodule is a valid git repo.

### 9.4 Required secrets

| Secret | Where | Purpose |
|--------|-------|---------|
| `SMTP_USERNAME` | both repos | Email address for notifications |
| `SMTP_PASSWORD` | both repos | Gmail App Password (https://myaccount.google.com/apppasswords) |
| `GH_PAT` | `utility-sw` | Personal Access Token to access submodules in CI |

---

## 10. Releasing

### Creating a release

```bash
# 1. Update version in CMakeLists.txt
#    project(GotiKinesis VERSION x.y.z LANGUAGES CXX)

# 2. Commit the version bump
git add CMakeLists.txt
git commit -m "Bump version to x.y.z"
git push

# 3. Tag and push
git tag vx.y.z
git push --tags
```

This triggers the release workflow which:
1. Builds on all 3 platforms
2. Runs all tests
3. Creates a GitHub Release at https://github.com/psglinux/scp-ftp-tftp/releases
4. Attaches: `.deb` (Linux), `.zip` (Windows), `.dmg` (macOS)
5. Sends email to `<author-email>` with the download URL

### Re-releasing a version (if a build was broken)

```bash
# Delete old release and tag
gh release delete vx.y.z --yes
git push origin :refs/tags/vx.y.z
git tag -d vx.y.z

# Fix the issue, commit, then re-tag
git tag vx.y.z
git push --tags
```

### Local release build

```bash
./scripts/release.sh          # builds + copies to release/latest/
./scripts/release.sh --clean  # cleans latest/ first
```

Release directory structure:
```
release/
├── latest/          ← overwritten on every build
│   └── GotiKinesis-0.1.0-Linux.deb
└── v0.1.0/          ← created only when HEAD has a git tag, preserved
    └── GotiKinesis-0.1.0-Linux.deb
```

---

## 11. Development Log

### v0.1.0 -- Alpha (2026-03-02)

**Changes:**
- Initial application with SCP/FTP/TFTP GUI
- 96 unit + integration tests (TFTP only)
- CI/CD on 3 platforms
- GitHub Pages download site

**Issues found after release:**
- Windows: SCP transfer failed with no diagnostic output
- Windows: `libcurl.dll` not found -- fixed by switching to static linking
- No SCP or FTP integration tests exist
- Log panel shows minimal transfer information

---

### v0.1.1 -- Robust Core Testing + Verbose Logging (planned)

**Goal:** Make SCP and FTP transfers reliable and observable. Add real-server
integration tests. Establish performance baselines. No new features.

#### Step 1: Verbose curl logging

**File:** `src/transferengine.cpp`

Add `CURLOPT_DEBUGFUNCTION` callback to capture libcurl internal output and
route it to the `logMessage` signal (GUI log panel):

- Connection attempts and DNS resolution
- SSH key exchange and host key verification
- Authentication method negotiation (password, public key)
- TLS/SSL handshake details (for FTPS)
- FTP command/response dialogue (USER, PASS, STOR, RETR, etc.)
- Errors with curl error codes and human-readable messages
- Incremental data progress every 256 KB: bytes transferred, percentage, speed
- Add `QElapsedTimer` for timing
- Completion summary: total bytes, elapsed time, average throughput

#### Step 2: Docker test infrastructure

**Files:** `tests/docker-compose.yml`, `tests/docker/`

Three real servers running in Docker containers:

| Service | Image | Port | Auth |
|---------|-------|------|------|
| `scp-server` | `linuxserver/openssh-server` | 2222 | user: `testuser`, pass: `testpass`, + SSH key |
| `ftp-server` | `fauria/vsftpd` | 2121 | user: `testuser`, pass: `testpass` |
| `tftp-server` | `pghalliday/tftp` | 6969 | none (read/write enabled) |

- SSH test key pair generated: `ssh-keygen -t ed25519 -f tests/docker/ssh_test_key -N ""`
- `tests/test_helpers.h`: utility that checks if containers are healthy;
  tests skip gracefully (via `QSKIP`) if containers are not running

**How to run locally:**
```bash
cd tests && docker compose up -d
# wait for healthy, then build and run tests
cd ../build && cmake .. && cmake --build . -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --output-on-failure
cd ../tests && docker compose down
```

#### Step 3: SCP integration tests

**File:** `tests/test_scp_integration.cpp`

| Test | What it verifies |
|------|-----------------|
| `uploadWithPassword` | Upload a file using password auth, download back, verify byte-for-byte |
| `downloadWithPassword` | Download a pre-seeded file, verify content |
| `uploadWithKeyAuth` | Upload using SSH private key, verify |
| `authFailure` | Wrong password -- verify returns false, log contains auth error |
| `connectionRefused` | Wrong port -- verify returns false, log contains connection error |
| `logContainsExpectedPhases` | Verify log has: connecting, key exchange, authenticating, completed |

#### Step 4: FTP integration tests

**File:** `tests/test_ftp_integration.cpp`

| Test | What it verifies |
|------|-----------------|
| `uploadWithCredentials` | Upload file, download back, verify byte-for-byte |
| `downloadWithCredentials` | Download pre-seeded file, verify content |
| `authFailure` | Wrong password -- verify failure + meaningful log |
| `connectionRefused` | Wrong port -- verify failure + log |
| `logContainsFtpDialogue` | Verify log has FTP response codes and command flow |

#### Step 5: TFTP integration tests against real server

**File:** `tests/test_tftp_real_integration.cpp`

| Test | What it verifies |
|------|-----------------|
| `uploadAndDownload` | Upload to real tftpd, download back, verify byte-for-byte |
| `downloadNonexistentFile` | Request missing file -- verify failure + error log |
| `logContainsExpectedPhases` | Verify RRQ/WRQ and completion messages |

#### Step 6: Performance benchmarks

**File:** `tests/test_performance.cpp`

| File size | SCP | FTP | TFTP |
|-----------|-----|-----|------|
| 1 KB | yes | yes | yes |
| 1 MB | yes | yes | yes |
| 10 MB | yes | yes | yes |
| 100 MB | yes | yes | skip (too slow) |
| 1 GB | yes | yes | skip (protocol limit ~32 MB) |

- Generates test data, uploads then downloads, measures wall-clock time
- Reports throughput in MB/s as a printed table
- Informational only -- no pass/fail threshold
- TFTP capped because RFC 1350 limits block numbers to 65535 x 512 bytes = ~32 MB

#### Step 7: Update CI

- Add Docker service containers to Linux CI job
  (Docker not available on Windows/macOS GitHub runners)
- Integration tests + benchmarks run on Linux only; unit tests on all platforms
- Add email notification step to release workflow: on successful release,
  sends download URL to `<author-email>`

#### Step 8: Release v0.1.1

```bash
# In CMakeLists.txt: project(GotiKinesis VERSION 0.1.1 LANGUAGES CXX)
git add -A
git commit -m "v0.1.1: verbose logging, Docker integration tests, benchmarks"
git push
git tag v0.1.1
git push --tags
```

---

## 12. Time Tracking

This section records the time spent by the spec author (user) and the
AI agent on each session. "User time" is the time spent specifying
requirements, reviewing plans, and providing feedback. "Agent time" is
the wall-clock time the agent spends executing tasks (coding, building,
testing, waiting for CI).

| Session | Date | Task | User Time | Agent Time | Notes |
|---------|------|------|-----------|------------|-------|
| 1 | 2026-03-02 | Initial app, CI/CD, packaging, v0.1.0 | ~45 min | ~3 hr | Setup repos, CI matrix, fix Windows libcurl, Qt CI issues, packaging, release |
| 2 | 2026-03-02 | Plan v0.1.1, create DEVLOG.md | ~30 min | ~1 hr | Drew up test/logging plan, wrote DEVLOG.md, replication gaps |
| 3 | 2026-03-03 | Execute v0.1.1: logging, Docker tests, benchmarks, release | ~1 min | ~1 hr 40 min | User approved plan ("yes lets go"), agent executed all 8 steps, fixed CI test failures, re-tagged release |

**Cumulative totals:**

| | User | Agent |
|---|---|---|
| Total | ~1 hr 16 min | ~5 hr 40 min |

*User time is estimated from message frequency and complexity.
Agent time includes CI wait periods (~30 min this session).*

---

## 13. Future Roadmap

Features to implement after core stability is proven:

**Features:**
- Connection profiles / bookmarks -- save and recall frequently used servers
- Remote file browser -- browse remote directories before transferring
- Transfer queue -- queue multiple files, batch transfers
- Drag & drop -- drop files onto the window to start a transfer
- Transfer resume -- resume interrupted downloads (FTP supports REST command)
- CLI mode -- headless operation (`gotikinesis --upload ...`)
- FTP multi-connection download -- parallel byte ranges using FTP REST command

**UI/UX:**
- Dark/light theme toggle
- Transfer speed & ETA display in the GUI status area
- System tray -- minimize to tray, notify on completion
- Connection status indicator -- test connection before transferring

**Code quality:**
- Fix placeholder metadata in CMakeLists.txt (`maintainer@example.com`)
- Add a proper LICENSE file
- AppImage or Flatpak for universal Linux packaging
- RPM packaging for Fedora/RHEL

**Infrastructure:**
- Set up `SMTP_PASSWORD` secret for CI failure email notifications

**Protocol notes for large file optimization:**
- FTP: supports `REST` (restart) command, enabling parallel range downloads
  across multiple connections. Implementable and worthwhile for large files.
- SCP: single-stream, no seek/resume. Cannot be chunked.
- SFTP: supports random-access reads/writes, enabling parallel chunked
  transfers. Consider adding SFTP as a fourth protocol (superior to SCP).
- TFTP: stop-and-wait, 512-byte blocks, ~32 MB limit. No optimization possible.
