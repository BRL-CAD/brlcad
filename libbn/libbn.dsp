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
!MESSAGE NMAKE /f "libbn.mak" CFG="libbn - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libbn - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libbn - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
F90=df.exe
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
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 -I "../h" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

# Begin Special Build Tool
TargetPath=.\Release\libbn.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) "..\lib"
# End special Build Tool

!ELSEIF  "$(CFG)" == "libbn - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../h" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\libbn_d.lib"
TargetPath=.\Debug\libbn_d.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "libbn - Win32 Release"
# Name "libbn - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\anim.c
# End Source File
# Begin Source File

SOURCE=.\asize.c
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

SOURCE=.\number.c
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

SOURCE=.\wavelet.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
