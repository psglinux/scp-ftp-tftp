@echo off
:: ===================================================================
:: GotiKinesis — Manual Install (run as Administrator)
:: ===================================================================
setlocal

set INSTALL_DIR=%ProgramFiles%\GotiKinesis
set STARTMENU_DIR=%ProgramData%\Microsoft\Windows\Start Menu\Programs\GotiKinesis

echo Installing GotiKinesis to %INSTALL_DIR% ...

:: Require admin
net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: Please run this script as Administrator.
    pause
    exit /b 1
)

:: Create install directory and copy files
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
xcopy /E /Y /I "%~dp0\*" "%INSTALL_DIR%\" >nul
echo   Files copied.

:: Create Start Menu shortcut
if not exist "%STARTMENU_DIR%" mkdir "%STARTMENU_DIR%"
powershell -NoProfile -Command ^
    "$ws = New-Object -ComObject WScript.Shell; ^
     $sc = $ws.CreateShortcut('%STARTMENU_DIR%\GotiKinesis.lnk'); ^
     $sc.TargetPath = '%INSTALL_DIR%\gotikinesis.exe'; ^
     $sc.WorkingDirectory = '%INSTALL_DIR%'; ^
     $sc.Description = 'SCP / FTP / TFTP File Transfer Utility'; ^
     $sc.Save()"
echo   Start Menu shortcut created.

:: Create Desktop shortcut
powershell -NoProfile -Command ^
    "$ws = New-Object -ComObject WScript.Shell; ^
     $sc = $ws.CreateShortcut([Environment]::GetFolderPath('Desktop') + '\GotiKinesis.lnk'); ^
     $sc.TargetPath = '%INSTALL_DIR%\gotikinesis.exe'; ^
     $sc.WorkingDirectory = '%INSTALL_DIR%'; ^
     $sc.Description = 'SCP / FTP / TFTP File Transfer Utility'; ^
     $sc.Save()"
echo   Desktop shortcut created.

:: Add/Remove Programs registry entry
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GotiKinesis" ^
    /v DisplayName    /t REG_SZ /d "GotiKinesis" /f >nul
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GotiKinesis" ^
    /v DisplayVersion /t REG_SZ /d "1.0.0" /f >nul
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GotiKinesis" ^
    /v Publisher      /t REG_SZ /d "GotiKinesis" /f >nul
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GotiKinesis" ^
    /v InstallLocation /t REG_SZ /d "%INSTALL_DIR%" /f >nul
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GotiKinesis" ^
    /v UninstallString /t REG_SZ /d "\"%INSTALL_DIR%\uninstall.bat\"" /f >nul
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GotiKinesis" ^
    /v NoModify /t REG_DWORD /d 1 /f >nul
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GotiKinesis" ^
    /v NoRepair /t REG_DWORD /d 1 /f >nul
echo   Add/Remove Programs entry created.

:: Copy uninstall script into install dir
copy /Y "%~dp0uninstall.bat" "%INSTALL_DIR%\uninstall.bat" >nul

echo.
echo Installation complete!
echo   Launch from Start Menu or Desktop shortcut.
pause
