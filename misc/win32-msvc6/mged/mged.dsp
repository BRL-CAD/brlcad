# Microsoft Developer Studio Project File - Name="mged" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mged - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mged.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mged.mak" CFG="mged - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mged - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mged - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
RSC=rc.exe

!IF  "$(CFG)" == "mged - Win32 Release"

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
# ADD BASE F90 /compile_only /nologo /warn:nofileopt
# ADD F90 /compile_only /nologo /warn:nofileopt
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/xlib" /I "../../../include" /I "../../../src/mged" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D TCL_THREADS=1 /D "__win32" /D "BRLCAD_DLL" /D "HAVE_CONFIG_H" /D "DM_OGL" /D "DM_X" /D "USE_FBSERV" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib advapi32.lib ws2_32.lib opengl32.lib gdi32.lib tcl84.lib tk84.lib itcl33.lib itk33.lib libbu.lib libbn.lib libsysv.lib librt.lib libwdb.lib libdm.lib libtclcad.lib libpkg.lib libpng.lib libz.lib libfb.lib /nologo /subsystem:console /machine:I386 /libpath:"../../../lib"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
TargetPath=.\Release\mged.exe
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin
# End Special Build Tool

!ELSEIF  "$(CFG)" == "mged - Win32 Debug"

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
# ADD BASE F90 /check:bounds /compile_only /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD F90 /check:bounds /compile_only /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/xlib" /I "../../../include" /I "../../../src/mged" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D TCL_THREADS=1 /D "__win32" /D "BRLCAD_DLL" /D "BRLCAD_DEBUG" /D "HAVE_CONFIG_H" /D "DM_OGL" /D "DM_X" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib advapi32.lib ws2_32.lib opengl32.lib gdi32.lib tcl84_d.lib tk84_d.lib itcl33_d.lib itk33_d.lib libbu_d.lib libbn_d.lib libsysv_d.lib librt_d.lib libwdb_d.lib libdm_d.lib libfb_d.lib libtclcad_d.lib libpkg_d.lib libpng_d.lib libz_d.lib /nologo /subsystem:console /debug /machine:I386 /out:"Debug/mged_d.exe" /pdbtype:sept /libpath:"../../../lib"
# Begin Special Build Tool
TargetPath=.\Debug\mged_d.exe
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "mged - Win32 Release"
# Name "mged - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;f90;for;f;fpp"
# Begin Source File

SOURCE=../../../src/mged/adc.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/anal.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/animedit.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/arb.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/arbs.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/attach.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/axes.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/bodyio.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/buttons.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/chgmodel.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/chgtree.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/chgview.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/cmd.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/color_scheme.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/columns.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/concat.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/dir.c
# End Source File
# Begin Source File

SOURCE="../../../src/mged/dm-generic.c"
# End Source File
# Begin Source File

SOURCE="../../../src/mged/dm-ogl.c"
# End Source File
# Begin Source File

SOURCE="../../../src/mged/dm-plot.c"
# End Source File
# Begin Source File

SOURCE="../../../src/mged/dm-ps.c"
# End Source File
# Begin Source File

SOURCE=../../../src/mged/dodraw.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/doevent.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/dozoom.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/edarb.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/edars.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/edpipe.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/edsol.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/facedef.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\mged\fbserv.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/ged.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/grid.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/history.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/inside.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/mater.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/menu.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/mover.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/muves.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/overlay.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/plot.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/polyif.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/predictor.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/qray.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/rect.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/red.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/rtif.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/scroll.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/set.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/share.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/solids_on_ray.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/tedit.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/titles.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/track.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/typein.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/update.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/usepen.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/utility1.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/utility2.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/vdraw.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/vers.c
# End Source File
# Begin Source File

SOURCE=../../../src/mged/vparse.c
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
