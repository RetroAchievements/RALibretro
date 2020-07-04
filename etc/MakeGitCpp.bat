@echo off

if exist ..\etc\MakeGitCpp.bat cd ..\etc\
set TEMPLATE=Git.cpp.template

git rev-parse HEAD > Temp.txt
set /p FULLHASH=<Temp.txt
set MINIHASH=%FULLHASH:~0,7%

git describe --tags > Temp.txt
set /p VERSION_TAG=<Temp.txt
for /f "tokens=1,2 delims=-" %%a in ("%VERSION_TAG%") do set VERSION_NUM=%%a&set VERSION_REVISION=%%b

if "%VERSION_REVISION%" neq "" set VERSION_NUM=%VERSION_NUM%.%VERSION_REVISION%

echo Tag: %VERSION_TAG% (%VERSION_NUM%)



if exist Git.cpp del Git.cpp
setlocal enabledelayedexpansion
for /f "tokens=1* delims=]" %%a in ('type "%TEMPLATE%" ^| find /V /N ""') do (
    set "LINE=%%b"
    if "!LINE!"=="" (
        echo. >> Git.cpp
    ) else (
        set LINE=!LINE:GITMINIHASH=%MINIHASH%!
        set LINE=!LINE:GITFULLHASH=%FULLHASH%!
        set LINE=!LINE:GITRELEASE=%VERSION_NUM%!
        echo !LINE! >> Git.cpp
    )
)

if not exist Git.cpp copy %TEMPLATE% Git.cpp

if not exist ..\src\Git.cpp goto nonexistant
fc ..\src\Git.cpp Git.cpp > nul
if errorlevel 1 goto different
del Git.cpp
goto done
:different
del ..\src\Git.cpp
:nonexistant
move Git.cpp ..\src\Git.cpp > nul
:done

del Temp.txt
