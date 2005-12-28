@ECHO OFF

REM	Author:		Bob Parker
REM	Company:	Survice Engineering

SETLOCAL
SET SAVE_CD=%CD%
SET PATH=%~dp0
SET ARCHER=%~dp0archer
CD %PATH%
CD ..
SET BRLCAD_DATA=%CD%
CD %SAVE_CD%

START /B wish "%ARCHER%"

CLS
EXIT
