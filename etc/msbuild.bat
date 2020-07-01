@echo off
setlocal

rem === Globals ===

set BUILDCLEAN=0
set DEBUG=0
set RELEASE=0
set W32=0
set W64=0

for %%A in (%*) do (
    if /I "%%A" == "Clean" (
        set BUILDCLEAN=1
    ) else if /I "%%A" == "Debug" (
        set DEBUG=1
    ) else if /I "%%A" == "Release" (
        set RELEASE=1
    ) else if /I "%%A" == "x86" (
        set W32=1
    ) else if /I "%%A" == "x64" (
        set W64=1
    )
)

if %W32% equ 0 if %W64% equ 0 (
    set W64=1
)

echo Initializing Visual Studio environment
if "%VSINSTALLDIR%"=="" set VSINSTALLDIR=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\
if not exist "%VSINSTALLDIR%" set VSINSTALLDIR=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\
if not exist "%VSINSTALLDIR%" set VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\
if not exist "%VSINSTALLDIR%" (
    echo Could not determine VSINSTALLDIR
    exit /B 1
)
echo using VSINSTALLDIR=%VSINSTALLDIR%

call "%VSINSTALLDIR%VC\Auxiliary\Build\vcvars32.bat"

rem === Build each project ===

if %BUILDCLEAN% equ 1 (
    if %DEBUG% equ 1 (
        call :build Clean Debug || goto eof
    )

    if %RELEASE% equ 1 (
        call :build Clean Release || goto eof
    )
) else (
    if %DEBUG% equ 1 (
        call :build RALibretro Debug || goto eof
    )

    if %RELEASE% equ 1 (
        call :build RALibretro Release || goto eof
    )
)

exit /B 0

rem === Build subroutine ===

:build

set PROJECT=%~1
set CONFIG=%~2

if %W32% equ 1 call :build2 %PROJECT% %CONFIG% x86
if %W64% equ 1 call :build2 %PROJECT% %CONFIG% x64

exit /B %ERRORLEVEL%

:build2

rem === Do the build ===

echo.
echo Building %~1 %~2 %~3

rem -- vsbuild doesn't like periods in the "-t" parameter, so replace them with underscores
rem -- https://stackoverflow.com/questions/56253635/what-could-i-be-doing-wrong-with-my-msbuild-argument
rem -- https://stackoverflow.com/questions/15441422/replace-character-of-string-in-batch-script/25812295
set ESCAPEDKEY=%~1
:replace_periods
for /f "tokens=1* delims=." %%i in ("%ESCAPEDKEY%") do (
   set ESCAPEDKEY=%%j
   if defined ESCAPEDKEY (
      set ESCAPEDKEY=%%i_%%j
      goto replace_periods
   ) else (
      set ESCAPEDKEY=%%i
   )
)

msbuild.exe RALibretro.sln -t:%ESCAPEDKEY% -p:Configuration=%~2 -p:Platform=%~3 /nowarn:C4091
set RESULT=%ERRORLEVEL%

rem === If build failed, bail ===

if not %RESULT% equ 0 (
    echo %~1 %~2 failed: %RESULT%
    exit /B %RESULT%
)

rem === For termination from within function ===

:eof
exit /B %ERRORLEVEL%
