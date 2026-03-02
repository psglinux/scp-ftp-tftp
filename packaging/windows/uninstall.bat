@echo off
:: ===================================================================
:: GotiKinesis — Uninstall (run as Administrator)
:: ===================================================================
setlocal

set INSTALL_DIR=%ProgramFiles%\GotiKinesis
set STARTMENU_DIR=%ProgramData%\Microsoft\Windows\Start Menu\Programs\GotiKinesis

echo Uninstalling GotiKinesis ...

:: Require admin
net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: Please run this script as Administrator.
    pause
    exit /b 1
)

:: Remove Desktop shortcut
del "%USERPROFILE%\Desktop\GotiKinesis.lnk" 2>nul
:: Also check Public Desktop
del "%PUBLIC%\Desktop\GotiKinesis.lnk" 2>nul
echo   Desktop shortcut removed.

:: Remove Start Menu folder
if exist "%STARTMENU_DIR%" rmdir /S /Q "%STARTMENU_DIR%"
echo   Start Menu entry removed.

:: Remove Add/Remove Programs registry entry
reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GotiKinesis" /f >nul 2>nul
echo   Registry entry removed.

:: Remove installation directory (schedule if locked)
if exist "%INSTALL_DIR%" (
    rmdir /S /Q "%INSTALL_DIR%" 2>nul
    if exist "%INSTALL_DIR%" (
        echo   Some files were locked. They will be removed on next reboot.
        rmdir /S /Q "%INSTALL_DIR%" 2>nul
    )
)
echo   Installation directory removed.

echo.
echo Uninstallation complete.
pause
