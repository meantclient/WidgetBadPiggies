@echo off
setlocal
echo ========================================================
echo   Building Bad Piggies Setup Installer (.exe)
echo ========================================================

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set "WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10\"
set "WindowsSDKVersion=10.0.26100.0\"
set "PATH=C:\Windows\System32;C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64;%PATH%"
set "INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared"
set "LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"

echo.
echo Compiling installer resources with embedded payload...
rc.exe /nologo /fo installer.res installer\installer.rc
cvtres.exe /NOLOGO /MACHINE:X64 /OUT:installer_res.obj installer.res

echo.
echo Compiling BadPiggiesSetup.exe...
cl.exe /nologo /O2 /W3 /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
    installer\installer_main.cpp installer_res.obj ^
    /link /OUT:"%CD%\BadPiggiesSetup.exe" ^
    user32.lib gdi32.lib shell32.lib advapi32.lib shlwapi.lib ole32.lib ^
    /SUBSYSTEM:WINDOWS

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo   INSTALLER CREATED: %CD%\BadPiggiesSetup.exe
    echo ========================================================
    del *.obj 2>nul
    del installer.res 2>nul
) else (
    echo.
    echo [ERROR] Installer build failed with code %ERRORLEVEL%!
    exit /b %ERRORLEVEL%
)
