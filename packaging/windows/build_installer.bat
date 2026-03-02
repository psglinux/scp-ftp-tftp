@echo off
setlocal enabledelayedexpansion

:: ===================================================================
:: GotiKinesis — Windows Build + NSIS Installer + Release Management
::
:: Keeps only two kinds of packages in release\:
::   release\latest\   — always overwritten with the newest build
::   release\<tag>\    — preserved for every git-tagged commit
::
:: Prerequisites:
::   1. CMake         (cmake.org or winget install cmake)
::   2. Visual Studio 2022 with C++ Desktop workload
::      — or MinGW-w64 (g++ on PATH)
::   3. Qt 6          (qt.io installer or vcpkg)
::   4. libcurl       (vcpkg install curl[ssh] or pre-built)
::   5. NSIS 3.x      (nsis.sourceforge.io or winget install nsis)
::   6. Git           (for tag detection)
::
:: Set CMAKE_PREFIX_PATH to your Qt installation, e.g.:
::   set CMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2022_64
:: ===================================================================

set "PROJECT_DIR=%~dp0..\.."
pushd "%PROJECT_DIR%"
set "PROJECT_DIR=%CD%"
popd
set "RELEASE_DIR=%PROJECT_DIR%\release"

if "%CMAKE_PREFIX_PATH%"=="" (
    echo WARNING: CMAKE_PREFIX_PATH is not set.
    echo   Set it to your Qt installation, e.g.:
    echo   set CMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2022_64
    echo.
)

echo ====================================
echo  Building GotiKinesis
echo ====================================

if not exist "%PROJECT_DIR%\build" mkdir "%PROJECT_DIR%\build"
cd /d "%PROJECT_DIR%\build"

cmake "%PROJECT_DIR%" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
if errorlevel 1 (
    echo CMake configuration failed.
    pause
    exit /b 1
)

cmake --build . --config Release --parallel
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo ====================================
echo  Deploying Qt runtime
echo ====================================

for /f "tokens=*" %%i in ('where windeployqt 2^>nul') do set WINDEPLOYQT=%%i
if "%WINDEPLOYQT%"=="" (
    echo windeployqt not found on PATH.
    echo Add your Qt bin directory to PATH, e.g.:
    echo   set PATH=%%PATH%%;C:\Qt\6.7.0\msvc2022_64\bin
    pause
    exit /b 1
)

"%WINDEPLOYQT%" --release --no-compiler-runtime --no-translations ^
    --no-system-d3d-compiler --no-opengl-sw ^
    Release\gotikinesis.exe

echo.
echo ====================================
echo  Creating NSIS Installer
echo ====================================

cpack -G NSIS -C Release
if errorlevel 1 (
    echo CPack/NSIS packaging failed.
    echo   Make sure NSIS is installed and on PATH.
    pause
    exit /b 1
)

:: ====================================
::  Release directory management
:: ====================================
echo.
echo ====================================
echo  Organizing release directory
echo ====================================

:: Find the generated installer
set "PKG="
for /f "delims=" %%f in ('dir /b /o-d "%PROJECT_DIR%\build\GotiKinesis-*.exe" 2^>nul') do (
    if not defined PKG set "PKG=%PROJECT_DIR%\build\%%f"
)

if not defined PKG (
    echo WARNING: No installer .exe found in build\
    pause
    exit /b 1
)

for %%f in ("%PKG%") do set "BASENAME=%%~nxf"
echo Package: %BASENAME%

:: 1. Always update latest\
if not exist "%RELEASE_DIR%\latest" mkdir "%RELEASE_DIR%\latest"
del /q "%RELEASE_DIR%\latest\*.exe" 2>nul
copy /y "%PKG%" "%RELEASE_DIR%\latest\" >nul
echo   -^> latest\%BASENAME%

:: 2. If HEAD is tagged, preserve under tag name
set "TAG="
for /f "tokens=*" %%t in ('git -C "%PROJECT_DIR%" describe --exact-match --tags HEAD 2^>nul') do set "TAG=%%t"

if defined TAG (
    if not exist "%RELEASE_DIR%\%TAG%" mkdir "%RELEASE_DIR%\%TAG%"
    copy /y "%PKG%" "%RELEASE_DIR%\%TAG%\" >nul
    echo   -^> %TAG%\%BASENAME%
)

:: 3. Clean directories that are neither latest\ nor a valid git tag
for /d %%d in ("%RELEASE_DIR%\*") do (
    set "DNAME=%%~nxd"
    if /i not "!DNAME!"=="latest" (
        if /i "!DNAME!"=="_CPack_Packages" (
            echo   Removing CPack staging: !DNAME!\
            rmdir /s /q "%%d"
        ) else (
            git -C "%PROJECT_DIR%" rev-parse "refs/tags/!DNAME!" >nul 2>&1
            if errorlevel 1 (
                echo   Removing untagged: !DNAME!\
                rmdir /s /q "%%d"
            )
        )
    )
)

:: 4. Remove loose files in release\ root (except .gitkeep)
for %%f in ("%RELEASE_DIR%\*") do (
    if /i not "%%~nxf"==".gitkeep" del /q "%%f" 2>nul
)

echo.
echo ====================================
echo  release\ contents
echo ====================================
dir /s /b "%RELEASE_DIR%" 2>nul
echo.
echo Done!
pause
