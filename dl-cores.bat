@ECHO OFF

REM dl-cores.bat
REM Download all cores supported by RALibretro
REM
REM dependencies: wget, unzip
REM
REM meleu, Raphael Zumer - 2018

FOR %%A IN (%1 %2 %3) DO (
	@IF /I "%%A"=="-Verbose" ECHO ON
)

SET SUPPORTED_CORES=fbalpha fceumm gambatte genesis_plus_gx handy mednafen_ngp ^
mednafen_supergrafx mednafen_vb mgba picodrive snes9x stella prosystem
SET NIGHTLY_URL="https://buildbot.libretro.com/nightly/windows/x86/latest"
SET DEST_DIR=bin\Cores
SET DEPS=wget unzip
SET HELP_ARGS=-? -Help
SET VERSION_ARGS=-Version
REM uncomment the line below to generate a log file
REM SET LOGFILE="dl-cores.log"

@FOR %%A IN (%1 %2 %3) DO (
	@IF /I "%%A"=="-Debug" (
		@SET LOGFILE="dl-cores-debug.log"
	)
)

GOTO main

:print_help
    ECHO USAGE: %0 [CORE_NAME]
    ECHO CORE_NAME: supported core, or 'all'
    ECHO List of supported cores:
    ECHO %SUPPORTED_CORES%
	EXIT /B 0
	
:print_version
	ECHO dl-cores.bat
	ECHO Version 1.0

:check_deps
	FOR %%D IN (%DEPS%) DO (
		%%D /? 2> NUL
		IF ERRORLEVEL 9009 (
			1>&2 ECHO Missing dependency: %%D
			EXIT /B 1
		)
	)
	
	EXIT /B 0

:dl_core
	SET core_filename=%~1_libretro.dll.zip
	
	>> %LOGFILE% ECHO Downloading %core_filename%. Please wait...
	>> %LOGFILE% 2>&1 wget -nv "%NIGHTLY_URL%/%core_filename%" -O "%DEST_DIR%\\%core_filename%"
	IF NOT ERRORLEVEL 1 (
		>> %LOGFILE% ECHO --- Downloaded %core_filename%
		>> %LOGFILE% 2>&1 unzip -o "%DEST_DIR%\%core_filename%" -d "%DEST_DIR%"
		IF NOT ERRORLEVEL 1 (
			>> %LOGFILE% ECHO --- Extracted %DEST_DIR%\%core_filename%
			DEL "%DEST_DIR%\\%core_filename%"
			EXIT /B 0
		) ELSE (
			>> %LOGFILE% ECHO WARNING: failed to extract %DEST_DIR%\%core_filename%
			EXIT /B 1
		)
	) ELSE (
		>> %LOGFILE% ECHO WARNING: failed to download %core_filename%
		EXIT /B 1
	)
	
	EXIT /B 0

:main
	FOR %%A IN (%HELP_ARGS%) DO (
		IF /I "%1"=="%%A" (
			GOTO print_help
			EXIT /B 0
		)
	)
	
	FOR %%A IN (%VERSION_ARGS%) DO (
		IF /I "%1"=="%%A" (
			GOTO print_version
			EXIT /B 0
		)
	)
	
	SET core=
	FOR %%C IN (%SUPPORTED_CORES%) DO (
		IF NOT defined core IF /I "%1"=="%%C" (
			SET core=%%C
		)
	)
	IF NOT defined core IF /I "%1"=="all" (
		SET core="all"
	)
	
	IF NOT defined core (
		1>&2 ECHO Invalid core.
		CALL :print_help
		EXIT /B 1
	)

    IF NOT defined LOGFILE (
		SET LOGFILE="NUL"
    ) ELSE (
		SET LOGFILE=%LOGFILE: =%
	)
	
	>> %LOGFILE% ECHO --- starting %0 log - %DATE% ---

    CALL :check_deps
	IF ERRORLEVEL 1 EXIT /B 1

    IF NOT EXIST "%DEST_DIR%" MKDIR "%DEST_DIR%"
	
	IF %core%=="all" (
		SET failed_cores=
		
		>> %LOGFILE% ECHO --- DOWNLOADING ALL SUPPORTED CORES ---
		
		FOR %%C IN (%SUPPORTED_CORES%) DO (
			CALL :dl_core %%C
			IF NOT ERRORLEVEL 1 (
				>> %LOGFILE% ECHO SUCCESS: %%C has been installed
			) ELSE (
				>> %LOGFILE% ECHO WARNING: failed to install %%C
				SET failed_cores=%failed_cores%;%%C
			)
		)
		
		IF NOT defined failed_cores (
			>> %LOGFILE% ECHO --- ALL CORES WERE SUCCESSFULLY INSTALLED ---
			ECHO Success!
			EXIT /B 0
		) ELSE (
			>> %LOGFILE% ECHO WARNING: Failed to install the following cores:
			>> %LOGFILE% ECHO %failed_cores%
			ECHO At least one core was unable to be installed. Re-run with -Debug to enable logging if needed.
			EXIT /B 1
		)
	) ELSE (
		CALL :dl_core %core%
		IF NOT ERRORLEVEL 1 (
			>> %LOGFILE% ECHO SUCCESS: %core% has been installed
			ECHO Success!
			EXIT /B 0
		) ELSE (
			>> %LOGFILE% ECHO WARNING: failed to install %core%.
			ECHO Installation failed. Re-run with -Debug to enable logging if needed.
			EXIT /B 1
		)
	)
	
	EXIT /B 0
