# Microsoft Developer Studio Project File - Name="liboptical" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

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
!MESSAGE "liboptical - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "liboptical - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
MTL=midl.exe
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
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE F90 /compile_only /dll /nologo /warn:nofileopt
# ADD F90 /compile_only /dll /nologo /warn:nofileopt
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBOPTICAL_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /I "../../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "OPTICAL_EXPORT_DLL" /D TCL_THREADS=1 /D "__win32" /D "BRLCAD_DLL" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib advapi32.lib tcl84.lib libbu.lib libbn.lib libsysv.lib librt.lib /nologo /dll /machine:I386 /libpath:"../../../lib"
# SUBTRACT LINK32 /nodefaultlib
# Begin Special Build Tool
TargetDir=.\Release
TargetPath=.\Release\liboptical.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\liboptical.lib ..\..\..\lib
# End Special Build Tool

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
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE F90 /check:bounds /compile_only /debug:full /dll /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD F90 /check:bounds /compile_only /debug:full /dll /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBOPTICAL_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /I "../../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "OPTICAL_EXPORT_DLL" /D TCL_THREADS=1 /D "__win32" /D "BRLCAD_DEBUG" /D "BRLCAD_DLL" /D "HAVE_CONFIG_H" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib advapi32.lib tcl84_d.lib libbu_d.lib libbn_d.lib libsysv_d.lib librt_d.lib /nologo /dll /debug /machine:I386 /out:"Debug/liboptical_d.dll" /pdbtype:sept /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Debug
TargetPath=.\Debug\liboptical_d.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\liboptical_d.lib ..\..\..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "liboptical - Win32 Release"
# Name "liboptical - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=../../../src/liboptical/init.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/material.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/photonmap.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/refract.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_air.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_billboard.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_brdf.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_camo.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_cloud.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_cook.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_fbm.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_fire.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_flat.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_gauss.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_grass.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_light.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_noise.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_null.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_plastic.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_points.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_prj.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_rtrans.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_scloud.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_spm.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_stack.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_stxt.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_tcl.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_text.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_toyota.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_treetherm.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/sh_wood.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/shade.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/turb.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/vers.c
# End Source File
# Begin Source File

SOURCE=../../../src/liboptical/wray.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
