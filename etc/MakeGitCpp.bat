@echo off

if exist ..\etc\MakeGitCpp.bat cd ..\etc\
set TEMPLATE=Git.cpp.template

git rev-parse HEAD > Temp.txt
set /p FULLHASH=<Temp.txt
set MINIHASH=%FULLHASH:~0,7%

if exist Git.cpp del Git.cpp
setlocal enabledelayedexpansion
for /f "tokens=* delims=" %%i in (%TEMPLATE%) do (
    set LINE=%%i
    set LINE=!LINE:GITMINIHASH=%MINIHASH%!
    set LINE=!LINE:GITFULLHASH=%FULLHASH%!
    echo !LINE! >> Git.cpp
)

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
