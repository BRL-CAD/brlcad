# Microsoft Developer Studio Project File - Name="liboptical" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=liboptical - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "liboptical.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "liboptical.mak" CFG="liboptical - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "liboptical - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "liboptical - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "liboptical - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "liboptical - Win32 Debug"

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
# ADD LIB32 /nologo /out:"Debug\liboptical_d.lib"
# Begin Custom Build
TargetPath=.\Debug\liboptical_d.lib
TargetName=liboptical_d
InputPath=.\Debug\liboptical_d.lib
SOURCE="$(InputPath)"

"$(TargetName).lib" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetPath) "..\lib"

# End Custom Build

!ENDIF 

# Begin Target

# Name "liboptical - Win32 Release"
# Name "liboptical - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\init.c
# End Source File
# Begin Source File

SOURCE=.\sh_air.c
# End Source File
# Begin Source File

SOURCE=.\sh_brdf.c
# End Source File
# Begin Source File

SOURCE=.\sh_camo.c
# End Source File
# Begin Source File

SOURCE=.\sh_cloud.c
# End Source File
# Begin Source File

SOURCE=.\sh_cook.c
# End Source File
# Begin Source File

SOURCE=.\sh_fire.c
# End Source File
# Begin Source File

SOURCE=.\sh_flat.c
# End Source File
# Begin Source File

SOURCE=.\sh_gauss.c
# End Source File
# Begin Source File

SOURCE=.\sh_grass.c
# End Source File
# Begin Source File

SOURCE=.\sh_light.c
# End Source File
# Begin Source File

SOURCE=.\sh_noise.c
# End Source File
# Begin Source File

SOURCE=.\sh_null.c
# End Source File
# Begin Source File

SOURCE=.\sh_plastic.c
# End Source File
# Begin Source File

SOURCE=.\sh_points.c
# End Source File
# Begin Source File

SOURCE=.\sh_prj.c
# End Source File
# Begin Source File

SOURCE=.\sh_rtrans.c
# End Source File
# Begin Source File

SOURCE=.\sh_scloud.c
# End Source File
# Begin Source File

SOURCE=.\sh_spm.c
# End Source File
# Begin Source File

SOURCE=.\sh_stack.c
# End Source File
# Begin Source File

SOURCE=.\sh_stxt.c
# End Source File
# Begin Source File

SOURCE=.\sh_tcl.c
# End Source File
# Begin Source File

SOURCE=.\sh_text.c
# End Source File
# Begin Source File

SOURCE=.\sh_toyota.c
# End Source File
# Begin Source File

SOURCE=.\sh_treetherm.c
# End Source File
# Begin Source File

SOURCE=.\sh_wood.c
# End Source File
# Begin Source File

SOURCE=.\turb.c
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
