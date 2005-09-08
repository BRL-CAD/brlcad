# Microsoft Developer Studio Project File - Name="itkstub" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=itkstub - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "itkstub.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "itkstub.mak" CFG="itkstub - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "itkstub - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "itkstub - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
RSC=rc.exe

!IF  "$(CFG)" == "itkstub - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE F90 /compile_only /nologo /warn:nofileopt
# ADD F90 /compile_only /nologo /warn:nofileopt
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../include" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/bitmaps" /I "../../../src/other/libtk/xlib" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /I "../../../src/incrTcl/itcl/generic" /I "../../../src/incrTcl/itk/generic" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D TCL_THREADS=1 /D "IGNORE_CONFIG_H" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Release\itkstub33.lib"
# Begin Special Build Tool
TargetPath=.\Release\itkstub33.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "itkstub - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE F90 /check:bounds /compile_only /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD F90 /check:bounds /compile_only /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/bitmaps" /I "../../../src/other/libtk/xlib" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /I "../../../src/incrTcl/itcl/generic" /I "../../../src/incrTcl/itk/generic" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D TCL_THREADS=1 /D "IGNORE_CONFIG_H" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\itkstub33_d.lib"
# Begin Special Build Tool
TargetPath=.\Debug\itkstub33_d.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "itkstub - Win32 Release"
# Name "itkstub - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;f90;for;f;fpp"
# Begin Source File

SOURCE=../../../src/other/incrTcl/itk/generic/itkStubLib.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# End Group
# End Target
# End Project
