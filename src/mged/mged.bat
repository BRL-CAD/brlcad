@ECHO OFF

REM	Author:		Bob Parker
REM	Company:	Survice Engineering

SETLOCAL
SET CAD_VERSION=7.11.0
SET SAVE_CD=%CD%
SET PATH=%~dp0
CD %PATH%\..
SET BRLCAD_DATA=%CD%\share\brlcad\%CAD_VERSION%
CD %SAVE_CD%
SET WEB_BROWSER=C:\Program Files\Internet Explorer\IEXPLORE.EXE

IF "%1"=="-g" (
    START /B mged.exe %2
) ELSE (
    START /B mged.exe 2>&1 nul
)

CLS
EXIT
