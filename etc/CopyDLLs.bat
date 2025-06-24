@echo off
@setlocal

set W64=0
if "%1"=="x64" set W64=1

set PROJECTDIR=%2
set BINDIR=%3

rem === SDL2 is required by RALibretro, libstdc++-6 and libwinpthread-1 are required by cores ===
set LIBSTDCPP=libstdc++-6.dll
set LIBPTHREAD=libwinpthread-1.dll
set SDL=SDL2.dll

set SDLSRC=SDL2\lib\x86
if %W64% equ 1 set SDLSRC=SDL2\lib\x64
if not exist "%BINDIR%\%SDL%" (
    echo Copying SDL2.dll from %SDLSRC%
    copy /y "%PROJECTDIR%\%SDLSRC%\%SDL%" "%BINDIR%"
)

if exist "%BINDIR%\%LIBSTDC%" (
    if exist "%BINDIR%\%LIBPTHREAD%" (
        exit /B 0
    )
)

if %W64% equ 1 if exist C:\msys64\mingw64 set SRCDIR=C:\msys64\mingw64
if %W64% neq 1 if exist C:\msys64\mingw32 set SRCDIR=C:\msys64\mingw32
if "%SRCDIR%"=="" set SRCDIR="%MINGW%"
if not exist "%SRCDIR%" set SRCDIR=C:\mingw
)

if not exist "%SRCDIR%" (
    echo Could not locate MINGW
    exit /B 0
)

if not exist "%BINDIR%\%LIBSTDCPP%" (
    if not exist "%SRCDIR%\bin\%LIBSTDCPP%" (
        echo WARNING: %LIBSTDCPP% not found in %SRCDIR%\bin
    ) else (
        echo Copying %LIBSTDCPP% from %SRCDIR%
        copy /y "%SRCDIR%\bin\%LIBSTDCPP%" "%BINDIR%"
    )
)

if not exist "%BINDIR%\%LIBPTHREAD%" (
    if not exist "%SRCDIR%\bin\%LIBPTHREAD%" (
        echo WARNING: %LIBPTHREAD% not found in %SRCDIR%\bin
    ) else (
        echo Copying %LIBPTHREAD% from %SRCDIR%
        copy /y "%SRCDIR%\bin\%LIBPTHREAD%" "%BINDIR%"
    )
)
