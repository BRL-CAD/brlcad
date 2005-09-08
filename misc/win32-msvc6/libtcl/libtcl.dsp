# Microsoft Developer Studio Project File - Name="libtcl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libtcl - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libtcl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libtcl.mak" CFG="libtcl - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libtcl - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libtcl - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libtcl - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE F90 /compile_only /dll /nologo /warn:nofileopt
# ADD F90 /compile_only /dll /nologo /warn:nofileopt
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../include" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D TCL_PIPE_DLL=\"tclpipe.dll\" /D TCL_THREADS=1 /D "BUILD_tcl" /D "IGNORE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /i "../../../include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib advapi32.lib /nologo /dll /machine:I386 /out:"Release/tcl84.dll" /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Release
TargetPath=.\Release\tcl84.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\tcl84.lib ..\..\..\lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "libtcl - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE F90 /check:bounds /compile_only /debug:full /dll /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD F90 /check:bounds /compile_only /debug:full /dll /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D TCL_PIPE_DLL=\"tclpipe.dll\" /D TCL_THREADS=1 /D "BUILD_tcl" /D "IGNORE_CONFIG_H" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /i "../../../include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib advapi32.lib /nologo /dll /debug /machine:I386 /out:"Debug/tcl84_d.dll" /pdbtype:sept /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Debug
TargetPath=.\Debug\tcl84_d.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\tcl84_d.lib ..\..\..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "libtcl - Win32 Release"
# Name "libtcl - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;f90;for;f;fpp"
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/regcomp.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/regerror.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/regexec.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/regfree.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/compat/strftime.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/compat/strtoll.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/compat/strtoull.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclAlloc.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclAsync.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclBasic.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclBinary.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclCkalloc.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclClock.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclCmdAH.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclCmdIL.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclCmdMZ.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclCompCmds.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclCompExpr.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclCompile.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclDate.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclEncoding.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclEnv.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclEvent.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclExecute.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclFCmd.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclFileName.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclGet.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclHash.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclHistory.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclIndexObj.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclInterp.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclIO.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclIOCmd.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclIOGT.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclIOSock.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclIOUtil.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclLink.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclListObj.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclLiteral.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclLoad.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclMain.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclNamesp.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclNotify.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclObj.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclPanic.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclParse.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclParseExpr.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclPipe.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclPkg.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclPosixStr.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclPreserve.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclProc.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclRegexp.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclResolve.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclResult.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclScan.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclStringObj.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclStubInit.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclStubLib.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclThread.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclThreadAlloc.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclThreadJoin.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclTimer.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclUtf.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclUtil.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/generic/tclVar.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWin32Dll.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinChan.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinConsole.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinError.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinFCmd.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinFile.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinInit.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinLoad.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinMtherr.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinNotify.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinPipe.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinSerial.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinSock.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinThrd.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtcl/win/tclWinTime.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\..\src\other\libtcl\win\tcl.rc
# End Source File
# End Group
# End Target
# End Project
