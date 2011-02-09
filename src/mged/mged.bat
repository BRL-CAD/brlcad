@ECHO OFF

REM                      M G E D . B A T
REM  BRL-CAD
REM 
REM  Copyright (c) 2006-2011 United States Government as represented by
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

IF "%1"=="-g" (
    "%~dp0\"mged.exe %2
) ELSE (
    "%~dp0\"mged.exe %1
)

CLS
EXIT
