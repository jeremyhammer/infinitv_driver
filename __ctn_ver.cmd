@ECHO OFF
IF NOT EXIST "%~dp0makefile" EXIT /B

PUSHD %~dp0

SETLOCAL

SET __VersionFile=ctn_ver.h

CALL :GetDate year month day
CALL :GetTime hours mins secs hsecs

SET VER_YEAR=0000%year%
SET VER_YEAR=%VER_YEAR:~-2%

SET VER_MONTH=00%month%
SET VER_MONTH=%VER_MONTH:~-2%

SET VER_DAY=00%day%
SET VER_DAY=%VER_DAY:~-2%

IF "%hours:~0,1%"=="0" SET hours=%hours:~1,1%
SET /A VER_TIME=%hours% * 60 + %mins%
SET VER_TIME=0000%VER_TIME%
SET VER_TIME=%VER_TIME:~-4%

ECHO #ifndef CTN_VER_H>%__VersionFile%
ECHO #define CTN_VER_H>>%__VersionFile%
ECHO.>>%__VersionFile%
ECHO #define CTN_VER_YEAR     %VER_YEAR%>>%__VersionFile%
ECHO #define CTN_VER_MONTH    %VER_MONTH%>>%__VersionFile%
ECHO #define CTN_VER_DAY      %VER_DAY%>>%__VersionFile%
ECHO #define CTN_VER_TIME     %VER_TIME%>>%__VersionFile%
ECHO.>>%__VersionFile%
ECHO #endif // CTN_VER_H>>%__VersionFile%

ECHO %VER_YEAR%.%VER_MONTH%.%VER_DAY%.%VER_TIME%>version.txt

:Exit

ENDLOCAL

POPD

EXIT /B

:GetDate yy mm dd
setlocal ENABLEEXTENSIONS
set t=2&if "%date%z" LSS "A" set t=1
for /f "skip=1 tokens=2-4 delims=(-)" %%a in ('echo/^|date') do (
  for /f "tokens=%t%-4 delims=.-/ " %%d in ('date/t') do (
    set %%a=%%d&set %%b=%%e&set %%c=%%f))
endlocal&set %1=%yy%&set %2=%mm%&set %3=%dd%&goto :EOF

:GetTime hh mm ss tt
setlocal ENABLEEXTENSIONS
for /f "tokens=5-8 delims=:. " %%a in ('echo/^|time') do (
  set hh=%%a&set mm=%%b&set ss=%%c&set cs=%%d)
if 1%hh% LSS 20 set hh=0%hh%
endlocal&set %1=%hh%&set %2=%mm%&set %3=%ss%&set %4=%cs%&goto :EOF