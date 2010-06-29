@ECHO OFF

REM                      A R C H E R . B A T
REM  BRL-CAD
REM 
REM  Copyright (c) 2006-2010 United States Government as represented by
REM  the U.S. Army Research Laboratory.
REM 
REM  This library is free software; you can redistribute it and/or
REM  modify it under the terms of the GNU Lesser General Public License
REM  version 2.1 as published by the Free Software Foundation.
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

SETLOCAL

REM XXX FIXME: SHOULD NOT NEED TO SET BRLCAD_DATA OR CAD_VERSION XXX
SET CAD_VERSION=7.16.8

SET SAVE_CD=%CD%
SET PATH=%~dp0
SET ARCHER=%~dp0archer
CD %PATH%\..
SET BRLCAD_DATA=%CD%\share\brlcad\%CAD_VERSION%
CD %SAVE_CD%

START /B bwish "%ARCHER%" %1

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
