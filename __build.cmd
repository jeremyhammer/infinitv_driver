@ECHO OFF
IF NOT EXIST "%~dp0makefile" EXIT /B

PUSHD %~dp0

IF NOT EXIST version.txt (ECHO Error: File version.txt does not exist, make sure auto-versioning is setup and x86 is built before x64) & (GOTO Exit)

FOR /F %%I IN (version.txt) DO SET STAMPINF_VERSION=%%I
IF "%STAMPINF_VERSION%"=="" (ECHO Error: STAMPINF not populated, make sure auto-versioning is setup and x86 is built before x64) & (GOTO Exit)

SETLOCAL

SET __SYMBOL_LOCAL_SHARE=\\%COMPUTERNAME%\symbols
SET __SYMBOL_CETON_SHARE=\\KNIFE\shares\builds\symbols

SET __BUILDARCH=amd64
IF "%_BUILDARCH%"=="x86" SET __BUILDARCH=i386

REM Bug in STAMPINF
IF EXIST obj%BUILD_ALT_DIR%\%__BUILDARCH%\ceton_*.inf DEL obj%BUILD_ALT_DIR%\%__BUILDARCH%\ceton_*.inf

build -czZMgz

IF NOT EXIST obj%BUILD_ALT_DIR%\%__BUILDARCH%\ceton_*.inf GOTO Exit

FOR %%I IN (obj%BUILD_ALT_DIR%\%__BUILDARCH%\ceton_*.inf) DO SET DRIVER_NAME=%%~nI

IF NOT EXIST release\%__BUILDARCH%\ MKDIR release\%__BUILDARCH%\>NUL

COPY /Y obj%BUILD_ALT_DIR%\%__BUILDARCH%\ceton_*.inf release
COPY /Y obj%BUILD_ALT_DIR%\%__BUILDARCH%\ceton_*.sys release\%__BUILDARCH%\
COPY /Y obj%BUILD_ALT_DIR%\%__BUILDARCH%\ceton_*.pdb release\%__BUILDARCH%\

IF EXIST release\*.cat DEL release\*.cat

FOR /F %%I IN ('whoami.exe') DO SET WHOAMI=%%I

IF NOT EXIST "%ProgramFiles%\Debugging Tools for Windows (x64)\symstore.exe" (ECHO WARNING: Debugging Tools for Windows x64 not found) && (GOTO Exit)

IF EXIST "%__SYMBOL_LOCAL_SHARE%" (
	ECHO Adding "%DRIVER_NAME% v%STAMPINF_VERSION% (%__BUILDARCH%, %BUILD_ALT_DIR%)" to symbol server %__SYMBOL_LOCAL_SHARE%
	"%ProgramFiles%\Debugging Tools for Windows (x64)\symstore.exe" add /r /f "obj%BUILD_ALT_DIR%\%__BUILDARCH%\*.*" /s "%__SYMBOL_LOCAL_SHARE%" /t "%DRIVER_NAME%" /v "%STAMPINF_VERSION% (%__BUILDARCH%, %BUILD_ALT_DIR%)" /c "%DATE% %TIME% by %WHOAMI% using %_NTDRIVE%%_NTROOT%"
)

IF /I "%1"=="/release" IF EXIST "%__SYMBOL_CETON_SHARE%" (
	ECHO Adding "%DRIVER_NAME% v%STAMPINF_VERSION% (%__BUILDARCH%, %BUILD_ALT_DIR%)" to symbol server %__SYMBOL_CETON_SHARE%
	"%ProgramFiles%\Debugging Tools for Windows (x64)\symstore.exe" add /r /f "obj%BUILD_ALT_DIR%\%__BUILDARCH%\*.*" /s "%__SYMBOL_CETON_SHARE%" /t "%DRIVER_NAME%" /v "%STAMPINF_VERSION% (%__BUILDARCH%, %BUILD_ALT_DIR%)" /c "%DATE% %TIME% by %WHOAMI% using %_NTDRIVE%%_NTROOT%"
)

:Exit

ENDLOCAL

POPD

EXIT /B
