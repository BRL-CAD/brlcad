@echo off

REM  Author: David Gravereaux <davygvry@bigfoot.com> 11:44 AM 2/28/1999
REM
REM  A sunday morning project.  I was bored and wanted a helpfile.
REM  HTML is nice, but IE5 beta2 is sorta slow on this 486.
REM
REM  man2tcl.c, man2help.tcl, man2help2.tcl, and index.tcl
REM  are *somewhat* modified versions from, i think, the 8.0a1 Tcl
REM  release from Sun.  There are still a few errors while converting
REM  to rtf, but the helpfile is built so this will do for now.
REM
REM  Enjoy!


REM Build man2tcl.exe if it isn't there
if exist man2tcl.exe goto :TclConvert
call vcvars32.bat
echo.
cl /G4 /ML /O2 man2tcl.c
if exist man2tcl.obj del man2tcl.obj
if not exist man2tcl.exe goto :CLError

REM Invoke the script
:TclConvert
echo.
echo Converting from man to rtf
echo.
tclsh80 man2help.tcl incr 30 ../../itcl/doc ../../itk/doc ../../iwidgets3.0.0/doc
if not errorlevel 0 goto :rtfError
echo.

REM Invoke HCW.exe to compile the helpfile
echo Building Win32 Help files
echo.
start /wait hcw /C /E /M "incr30.hpj"
if errorlevel 1 goto :Error
echo.

REM Move The helpfile and contents up a directory level
if exist ..\incr30.cnt del ..\incr30.cnt
if exist ..\incr30.hlp del ..\incr30.hlp
move incr30.cnt ..
move incr30.hlp ..
echo.
echo DONE!!
echo.
pause
goto :done


:CLError
echo.
echo man2tcl.exe : error: Problem encountered building the executable
pause
goto :done

:Error
echo incr30.hpj : error: Problem encountered creating help file
pause
goto :done

:rtfError
echo incr30.rtf : error: Problem encountered converting man pages
pause

:done
