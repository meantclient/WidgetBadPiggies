@echo off
setlocal
set "PATH=C:\Windows\System32;C:\Windows;%PATH%"
taskkill /f /im BadPiggiesDotaWidget.exe 2>nul
echo ========================================================
echo   Building Bad Piggies Dota 2 Widget (C++)
echo ========================================================

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set "WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10\"
set "WindowsSDKVersion=10.0.26100.0\"
set "PATH=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64;%PATH%"
set "INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared"
set "LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"

echo.
echo Compiling resources...
rc.exe /nologo /fo app.res src\app.rc

echo.
echo Compiling source files with UTF-8 support and embedded icon...
cl.exe /nologo /O2 /W3 /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /Iinclude /Isrc ^
    src\main.cpp src\config.cpp src\dota_gsi.cpp src\overlay.cpp src\vignette.cpp src\settings_gui.cpp src\dota_settings_gui.cpp src\dota_welcome.cpp src\hero_advisor.cpp src\voice_assistant.cpp src\voice.cpp src\updater.cpp src\tray.cpp ^
    app.res ^
    /link /OUT:BadPiggiesDotaWidget.exe ^
    user32.lib gdi32.lib gdiplus.lib ws2_32.lib shell32.lib advapi32.lib shlwapi.lib winmm.lib comctl32.lib uxtheme.lib ole32.lib urlmon.lib wininet.lib ^
    /SUBSYSTEM:WINDOWS

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo   BUILD SUCCESSFUL! Created BadPiggiesDotaWidget.exe
    echo ========================================================
    del *.obj 2>nul
    del app.res 2>nul
) else (
    echo.
    echo [ERROR] Build failed with code %ERRORLEVEL%!
    exit /b %ERRORLEVEL%
)
