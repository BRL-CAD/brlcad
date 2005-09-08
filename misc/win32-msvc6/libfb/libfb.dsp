# Microsoft Developer Studio Project File - Name="libfb" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

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
!MESSAGE "libfb - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libfb - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
MTL=midl.exe
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
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE F90 /compile_only /dll /nologo /warn:nofileopt
# ADD F90 /compile_only /dll /nologo /warn:nofileopt
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBFB_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/xlib" /I "../../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FB_EXPORT_DLL" /D TCL_THREADS=1 /D "__win32" /D "IF_X" /D "IF_OGL" /D "IF_REMOTE" /D "BRLCAD_DLL" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 opengl32.lib gdi32.lib kernel32.lib user32.lib advapi32.lib tcl84.lib tk84.lib libbu.lib libpkg.lib /nologo /dll /machine:I386 /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Release
TargetPath=.\Release\libfb.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\libfb.lib ..\..\..\lib
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
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE F90 /check:bounds /compile_only /debug:full /dll /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD F90 /check:bounds /compile_only /debug:full /dll /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBFB_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/xlib" /I "../../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FB_EXPORT_DLL" /D TCL_THREADS=1 /D "__win32" /D "BRLCAD_DEBUG" /D "BRLCAD_DLL" /D "IF_X" /D "IF_OGL" /D "HAVE_CONFIG_H" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 opengl32.lib gdi32.lib kernel32.lib user32.lib advapi32.lib tcl84_d.lib tk84_d.lib libbu_d.lib libpkg_d.lib /nologo /dll /debug /machine:I386 /out:"Debug/libfb_d.dll" /pdbtype:sept /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Debug
TargetPath=.\Debug\libfb_d.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\libfb_d.lib ..\..\..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "libfb - Win32 Release"
# Name "libfb - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;f90;for;f;fpp"
# Begin Source File

SOURCE=../../../src/libfb/asize.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/fb_generic.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/fb_log.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/fb_obj.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/fb_paged_io.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/fb_rect.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/fb_util.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/fbserv_obj_win32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\libfb\getput.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/if_debug.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/if_disk.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/if_mem.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/if_null.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/if_ogl_win32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\libfb\if_remote.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/if_stack.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/server.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/tcl.c
# End Source File
# Begin Source File

SOURCE=../../../src/libfb/vers.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
