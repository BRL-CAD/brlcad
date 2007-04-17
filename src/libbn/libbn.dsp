# Microsoft Developer Studio Project File - Name="libbn" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libbn - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "libbn.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "libbn.mak" CFG="librt - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "libbn - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libbn - Win32 Release Multithreaded" (based on "Win32 (x86) Static Library")
!MESSAGE "libbn - Win32 Release Multithreaded DLL" (basierend auf "Win32 (x86) Static Library")
!MESSAGE "libbn - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "libbn - Win32 Debug Multithreaded" (based on "Win32 (x86) Static Library")
!MESSAGE "libbn - Win32 Debug Multithreaded DLL" (bbased on "Win32 (x86) Static Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "libbn"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libbn - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\misc\win32-msvc\Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_LIB" /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../../include" /I "../../src/other/tcl/generic" /D "WIN32" /D "NDEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbn - Win32 Release Multithreaded"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\misc\win32-msvc\ReleaseMt"
# PROP Intermediate_Dir "ReleaseMt"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_LIB" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../include" /I "../../src/other/tcl/generic" /D "WIN32" /D "NDEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbn - Win32 Release Multithreaded DLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\misc\win32-msvc\ReleaseMtDll"
# PROP Intermediate_Dir "ReleaseMtDll"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_LIB" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /I "../../src/other/tcl/generic" /D "WIN32" /D "NDEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbn - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\misc\win32-msvc\Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_LIB" /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../include" /I "../../src/other/tcl/generic" /D "WIN32" /D "_DEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbn - Win32 Debug Multithreaded"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\misc\win32-msvc\DebugMt"
# PROP Intermediate_Dir "DebugMt"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_LIB" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../include" /I "../../src/other/tcl/generic" /D "WIN32" /D "_DEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbn - Win32 Debug Multithreaded DLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\misc\win32-msvc\DebugMtDll"
# PROP Intermediate_Dir "DebugMtDll"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_LIB" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /I "../../src/other/tcl/generic" /D "WIN32" /D "_DEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF

# Begin Target

# Name "libbn - Win32 Release"
# Name "libbn - Win32 Release Multithreaded"
# Name "libbn - Win32 Release Multithreaded DLL"
# Name "libbn - Win32 Debug"
# Name "libbn - Win32 Debug Multithreaded"
# Name "libbn - Win32 Debug Multithreaded DLL"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\anim.c
# End Source File
# Begin Source File

SOURCE=.\axis.c
# End Source File
# Begin Source File

SOURCE=.\bn_tcl.c
# End Source File
# Begin Source File

SOURCE=.\complex.c
# End Source File
# Begin Source File

SOURCE=.\const.c
# End Source File
# Begin Source File

SOURCE=.\font.c
# End Source File
# Begin Source File

SOURCE=.\fortran.c
# End Source File
# Begin Source File

SOURCE=.\list.c
# End Source File
# Begin Source File

SOURCE=.\marker.c
# End Source File
# Begin Source File

SOURCE=.\mat.c
# End Source File
# Begin Source File

SOURCE=.\msr.c
# End Source File
# Begin Source File

SOURCE=.\noise.c
# End Source File
# Begin Source File

SOURCE=.\plane.c
# End Source File
# Begin Source File

SOURCE=.\plot3.c
# End Source File
# Begin Source File

SOURCE=.\poly.c
# End Source File
# Begin Source File

SOURCE=.\qmath.c
# End Source File
# Begin Source File

SOURCE=.\rand.c
# End Source File
# Begin Source File

SOURCE=.\scale.c
# End Source File
# Begin Source File

SOURCE=.\sphmap.c
# End Source File
# Begin Source File

SOURCE=.\symbol.c
# End Source File
# Begin Source File

SOURCE=.\tabdata.c
# End Source File
# Begin Source File

SOURCE=.\tplot.c
# End Source File
# Begin Source File

SOURCE=.\vectfont.c
# End Source File
# Begin Source File

SOURCE=.\vector.c
# End Source File
# Begin Source File

SOURCE=.\vers.c
# End Source File
# Begin Source File

SOURCE=.\vert_tree.c
# End Source File
# Begin Source File

SOURCE=.\wavelet.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
