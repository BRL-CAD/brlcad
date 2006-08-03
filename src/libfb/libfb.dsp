# Microsoft Developer Studio Project File - Name="libfb" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libfb - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libfb.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libfb.mak" CFG="libfb - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libfb - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libfb - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
RSC=rc.exe

!IF  "$(CFG)" == "libfb - Win32 Release"

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
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../h" /I "../libtk8.4/xlib" /I "../libtk8.4/generic" /I "../libtk8.4/win" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "IF_WGL" /D "IF_X" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
# Begin Special Build Tool
TargetPath=.\Release\libfb.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) "..\lib"
# End Special Build Tool

!ELSEIF  "$(CFG)" == "libfb - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../h" /I "../libtk8.4/xlib" /I "../libtk8.4/generic" /I "../libtk8.4/win" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "IF_WGL" /D "IF_X" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\libfb_d.lib"
# Begin Special Build Tool
TargetPath=.\Debug\libfb_d.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "libfb - Win32 Release"
# Name "libfb - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\fb_generic.c
# End Source File
# Begin Source File

SOURCE=.\fb_log.c
# End Source File
# Begin Source File

SOURCE=.\fb_obj.c
# End Source File
# Begin Source File

SOURCE=.\fb_paged_io.c
# End Source File
# Begin Source File

SOURCE=.\fb_rect.c
# End Source File
# Begin Source File

SOURCE=.\fb_util.c
# End Source File
# Begin Source File

SOURCE=.\fbserv_obj_win32.c
# End Source File
# Begin Source File

SOURCE=.\getput.c
# End Source File
# Begin Source File

SOURCE=.\if_debug.c
# End Source File
# Begin Source File

SOURCE=.\if_disk.c
# End Source File
# Begin Source File

SOURCE=.\if_mem.c
# End Source File
# Begin Source File

SOURCE=.\if_null.c
# End Source File
# Begin Source File

SOURCE=.\if_wgl.c
# End Source File
# Begin Source File

SOURCE=.\if_stack.c
# End Source File
# Begin Source File

SOURCE=.\server.c
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
