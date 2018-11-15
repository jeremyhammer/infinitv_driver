@ECHO OFF
IF NOT EXIST "%~dp0makefile" EXIT /B

SETLOCAL DISABLEDELAYEDEXPANSION

:: SET CODE_DIR=%~dp0
:: IF NOT "%1"=="" SET CODE_DIR=%1
:: IF NOT "%CODE_DIR:~-1%"=="\" SET CODE_DIR=%CODE_DIR%\

SET CODE_DIR=%~dp0release
IF NOT EXIST %CODE_DIR% (ECHO Directory "%CODE_DIR%" does NOT exist) & (GOTO End)

PUSHD %CODE_DIR%

IF EXIST ceton_*.cat DEL ceton_*.cat

inf2cat.exe /driver:. /os:Vista_X86,Vista_X64 /verbose

FOR %%I IN (*.inf) DO SET DRIVER_NAME=%%~nI

IF "%DRIVER_NAME%"=="" GOTO Failed

signtool.exe sign /v /ac MSCV-VSClass3.cer /s my /n "Ceton Corporation" /t http://timestamp.verisign.com/scripts/timestamp.dll i386\%DRIVER_NAME%.sys
IF ERRORLEVEL  1 GOTO Failed

signtool.exe sign /v /ac MSCV-VSClass3.cer /s my /n "Ceton Corporation" /t http://timestamp.verisign.com/scripts/timestamp.dll amd64\%DRIVER_NAME%.sys
IF ERRORLEVEL  1 GOTO Failed

signtool.exe sign /v /ac MSCV-VSClass3.cer /s my /n "Ceton Corporation" /t http://timestamp.verisign.com/scripts/timestamp.dll %DRIVER_NAME%.cat
IF ERRORLEVEL  1 GOTO Failed

signtool.exe verify /pa /v %DRIVER_NAME%.cat
IF ERRORLEVEL  1 GOTO Failed

signtool.exe verify /kp /v %DRIVER_NAME%.cat
IF ERRORLEVEL  1 GOTO Failed

signtool.exe verify /kp /v /c %DRIVER_NAME%.cat amd64\%DRIVER_NAME%.sys
IF ERRORLEVEL  1 GOTO Failed

signtool.exe verify /kp /v /c %DRIVER_NAME%.cat i386\%DRIVER_NAME%.sys
IF ERRORLEVEL  1 GOTO Failed

FOR /F %%I IN (..\version.txt) DO SET DRIVER_VERSION=%%I
FOR %%I IN (%DRIVER_NAME%.inf) DO SET DRIVER_DATE=%%~tI

SET FILENAME=%DRIVER_NAME%_%DRIVER_DATE:~6,4%%DRIVER_DATE:~0,2%%DRIVER_DATE:~3,2%_%DRIVER_VERSION:.=_%.zip

ECHO.
ECHO Driver %DRIVER_NAME% v%DRIVER_VERSION% signed successfully

SET CETON_ROOT=%~dp0
CALL :FIND_CETON_ROOT %CETON_ROOT%

ECHO .cer >"%TEMP%\exclude.txt"
ECHO .dll>>"%TEMP%\exclude.txt"

IF EXIST "%CETON_ROOT%ceton_win_drivers\archive" (
	IF NOT EXIST "%CETON_ROOT%ceton_win_drivers\archive\%DRIVER_NAME%\%DRIVER_VERSION%" MKDIR "%CETON_ROOT%ceton_win_drivers\archive\%DRIVER_NAME%\%DRIVER_VERSION%"

	XCOPY /EXCLUDE:%TEMP%\exclude.txt /Y /K /Z /S /D *.* "%CETON_ROOT%ceton_win_drivers\archive\%DRIVER_NAME%\%DRIVER_VERSION%">nul
	FOR %%X IN (7za.exe) DO IF EXIST "%%~$PATH:X" IF NOT EXIST "%CETON_ROOT%ceton_win_drivers\archive\%DRIVER_NAME%\%Filename%" (7za.exe a -tzip -r -x!*.dll -x!*.cer -mx=9 "%CETON_ROOT%ceton_win_drivers\archive\%DRIVER_NAME%\%Filename%" *)
)

IF /I "%1"=="/release" IF EXIST "\\KNIFE\shares\release\win_drivers" (
	IF NOT EXIST "\\KNIFE\shares\release\win_drivers\%DRIVER_NAME%" MKDIR "\\KNIFE\shares\release\win_drivers\%DRIVER_NAME%"

	FOR %%X IN (7za.exe) DO IF EXIST "%%~$PATH:X" IF NOT EXIST "\\KNIFE\shares\release\win_drivers\%DRIVER_NAME%\%Filename%" (7za.exe a -tzip -r -x!*.dll -x!*.cer -mx=9 "\\KNIFE\shares\release\win_drivers\%DRIVER_NAME%\%Filename%" *)
)

IF EXIST "%CETON_ROOT%ceton_infinitv_win" (
	IF NOT EXIST "%CETON_ROOT%ceton_infinitv_win\setup\files\%DRIVER_NAME%\" MKDIR "%CETON_ROOT%ceton_infinitv_win\setup\files\%DRIVER_NAME%\"

	XCOPY /EXCLUDE:%TEMP%\exclude.txt /Y /K /Z /S /D *.* "%CETON_ROOT%ceton_infinitv_win\setup\files\%DRIVER_NAME%\">nul
)

GOTO End

:Failed

ECHO Failed to sign %DRIVER_NAME%

:End

POPD

ENDLOCAL

EXIT /B




:FIND_CETON_ROOT

IF NOT EXIST "%1" GOTO :EOF
IF "%1"=="%~d1" GOTO :EOF

IF EXIST "%CETON_ROOT%ceton_infinitv_win" GOTO :EOF

SET CETON_ROOT=%CETON_ROOT%..\

CALL :FIND_CETON_ROOT %CETON_ROOT%