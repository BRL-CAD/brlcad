REM                      A R C H E R . B A T
REM  BRL-CAD
REM 
REM  Copyright (c) 2006 United States Government as represented by
REM  the U.S. Army Research Laboratory.
REM 
REM  This library is free software; you can redistribute it and/or
REM  modify it under the terms of the GNU Lesser General Public License
REM  as published by the Free Software Foundation; either version 2 of
REM  the License, or (at your option) any later version.
REM 
REM  This library is distributed in the hope that it will be useful, but
REM  WITHOUT ANY WARRANTY; without even the implied warranty of
REM  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
REM  Lesser General Public License for more details.
REM 
REM  You should have received a copy of the GNU Lesser General Public
REM  License along with this file; see the file named COPYING for more
REM  information.
REM 
REM REM REM 
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

REM Local Variables:
REM mode: sh
REM tab-width: 8
REM sh-indentation: 4
REM sh-basic-offset: 4
REM indent-tabs-mode: t
REM End:
REM ex: shiftwidth=4 tabstop=8
