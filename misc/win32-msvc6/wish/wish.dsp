# Microsoft Developer Studio Project File - Name="wish" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=wish - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "wish.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "wish.mak" CFG="wish - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "wish - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "wish - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "wish - Win32 Release"

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
# ADD BASE F90 /compile_only /nologo /warn:nofileopt /winapp
# ADD F90 /compile_only /nologo /warn:nofileopt /winapp
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../include" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/bitmaps" /I "../../../src/other/libtk/xlib" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D TCL_THREADS=1 /D "IGNORE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /i "../../../include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib advapi32.lib user32.lib shell32.lib gdi32.lib comdlg32.lib winspool.lib imm32.lib comctl32.lib tcl84.lib tk84.lib /nologo /subsystem:windows /machine:I386 /libpath:"../../../lib"
# Begin Special Build Tool
TargetPath=.\Release\wish.exe
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin
# End Special Build Tool

!ELSEIF  "$(CFG)" == "wish - Win32 Debug"

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
# ADD BASE F90 /check:bounds /compile_only /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt /winapp
# ADD F90 /browser /check:bounds /compile_only /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt /winapp
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/bitmaps" /I "../../../src/other/libtk/xlib" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D TCL_THREADS=1 /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /i "../../../include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib advapi32.lib user32.lib shell32.lib gdi32.lib comdlg32.lib winspool.lib imm32.lib comctl32.lib tcl84_d.lib tk84_d.lib /nologo /subsystem:windows /debug /machine:I386 /out:"Debug/wish_d.exe" /pdbtype:sept /libpath:"../../../lib"
# Begin Special Build Tool
TargetPath=.\Debug\wish_d.exe
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "wish - Win32 Release"
# Name "wish - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;f90;for;f;fpp"
# Begin Source File

SOURCE=../../../src/other/libtk/win/winMain.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\wish.ico
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\wish.rc
# End Source File
# End Group
# End Target
# End Project
