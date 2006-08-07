# Microsoft Developer Studio Project File - Name="libdm" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libdm - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libdm.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libdm.mak" CFG="libdm - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libdm - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libdm - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
RSC=rc.exe

!IF  "$(CFG)" == "libdm - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../h" /I "../libtk8.4/xlib" /I "../libtk8.4/generic" /I "../libtk8.4/win" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "DM_WGL" /D "DM_X" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
# Begin Special Build Tool
TargetPath=.\Release\libdm.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) "..\lib"
# End Special Build Tool

!ELSEIF  "$(CFG)" == "libdm - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../h" /I "../libtk8.4/xlib" /I "../libtk8.4/generic" /I "../libtk8.4/win" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "DM_WGL" /D "DM_X" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\libdm_d.lib"
# Begin Special Build Tool
TargetPath=.\Debug\libdm_d.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "libdm - Win32 Release"
# Name "libdm - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\clip.c
# End Source File
# Begin Source File

SOURCE=".\dm-generic.c"
# End Source File
# Begin Source File

SOURCE=".\dm-Null.c"
# End Source File
# Begin Source File

SOURCE=".\dm-wgl.c"
# End Source File
# Begin Source File

SOURCE=".\dm-plot.c"
# End Source File
# Begin Source File

SOURCE=".\dm-ps.c"
# End Source File
# Begin Source File

SOURCE=.\dm_obj.c
# End Source File
# Begin Source File

SOURCE=.\knob.c
# End Source File
# Begin Source File

SOURCE=.\options.c
# End Source File
# Begin Source File

SOURCE=.\query.c
# End Source File
# Begin Source File

SOURCE=.\tcl.c
# End Source File
# Begin Source File

SOURCE=.\vers.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
