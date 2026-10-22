@echo off
set "APP_DIR=%~dp0udoo-key-app\build"
if not exist "%APP_DIR%\udoo-key-app.exe" (
    echo Error: Could not find udoo-key-app.exe in %APP_DIR%
    pause
    exit /b 1
)
cd /d "%APP_DIR%"
start "" "%APP_DIR%\udoo-key-app.exe"
