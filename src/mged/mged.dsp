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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../h" /I "../libtk8.4/xlib" /I "../libtk8.4/generic" /I "../libtk8.4/win" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "DM_X" /D "DM_OGL" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 opengl32.lib gdi32.lib user32.lib ws2_32.lib advapi32.lib libtcl84.lib libtk84.lib libsysv.lib libdm.lib libwdb.lib libbu.lib libbn.lib libfb.lib librt.lib libtermio.lib libitcl321.lib libitk321.lib /nologo /subsystem:console /machine:I386 /libpath:"../lib"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
TargetPath=.\Release\mged.exe
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\bin
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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../h" /I "../libtk8.4/xlib" /I "../libtk8.4/generic" /I "../libtk8.4/win" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "DM_X" /D "DM_OGL" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 opengl32.lib gdi32.lib user32.lib ws2_32.lib advapi32.lib libtcl84d.lib libtk84d.lib libsysv_d.lib libdm_d.lib libwdb_d.lib libbu_d.lib libbn_d.lib libfb_d.lib librt_d.lib libtermio_d.lib libitcl321d.lib libitk321d.lib /nologo /subsystem:console /debug /machine:I386 /out:"Debug/mged_d.exe" /pdbtype:sept /libpath:"../lib"
# Begin Special Build Tool
TargetPath=.\Debug\mged_d.exe
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\bin
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "mged - Win32 Release"
# Name "mged - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\adc.c
# End Source File
# Begin Source File

SOURCE=.\anal.c
# End Source File
# Begin Source File

SOURCE=.\animedit.c
# End Source File
# Begin Source File

SOURCE=.\arbs.c
# End Source File
# Begin Source File

SOURCE=.\attach.c
# End Source File
# Begin Source File

SOURCE=.\axes.c
# End Source File
# Begin Source File

SOURCE=.\bodyio.c
# End Source File
# Begin Source File

SOURCE=.\buttons.c
# End Source File
# Begin Source File

SOURCE=.\chgmodel.c
# End Source File
# Begin Source File

SOURCE=.\chgtree.c
# End Source File
# Begin Source File

SOURCE=.\chgview.c
# End Source File
# Begin Source File

SOURCE=.\cmd.c
# End Source File
# Begin Source File

SOURCE=.\color_scheme.c
# End Source File
# Begin Source File

SOURCE=.\columns.c
# End Source File
# Begin Source File

SOURCE=.\concat.c
# End Source File
# Begin Source File

SOURCE=.\dir.c
# End Source File
# Begin Source File

SOURCE=".\dm-generic.c"
# End Source File
# Begin Source File

SOURCE=".\dm-ogl.c"
# End Source File
# Begin Source File

SOURCE=".\dm-plot.c"
# End Source File
# Begin Source File

SOURCE=".\dm-ps.c"
# End Source File
# Begin Source File

SOURCE=.\dodraw.c
# End Source File
# Begin Source File

SOURCE=.\doevent.c
# End Source File
# Begin Source File

SOURCE=.\dozoom.c
# End Source File
# Begin Source File

SOURCE=.\edarb.c
# End Source File
# Begin Source File

SOURCE=.\edars.c
# End Source File
# Begin Source File

SOURCE=.\edpipe.c
# End Source File
# Begin Source File

SOURCE=.\edsol.c
# End Source File
# Begin Source File

SOURCE=.\facedef.c
# End Source File
# Begin Source File

SOURCE=.\fbserv_win32.c
# End Source File
# Begin Source File

SOURCE=.\ged.c
# End Source File
# Begin Source File

SOURCE=.\grid.c
# End Source File
# Begin Source File

SOURCE=.\history.c
# End Source File
# Begin Source File

SOURCE=.\inside.c
# End Source File
# Begin Source File

SOURCE=.\mater.c
# End Source File
# Begin Source File

SOURCE=.\menu.c
# End Source File
# Begin Source File

SOURCE=.\mover.c
# End Source File
# Begin Source File

SOURCE=.\muves.c
# End Source File
# Begin Source File

SOURCE=.\overlay.c
# End Source File
# Begin Source File

SOURCE=.\plot.c
# End Source File
# Begin Source File

SOURCE=.\polyif.c
# End Source File
# Begin Source File

SOURCE=.\predictor.c
# End Source File
# Begin Source File

SOURCE=.\qray.c
# End Source File
# Begin Source File

SOURCE=.\rect.c
# End Source File
# Begin Source File

SOURCE=.\red.c
# End Source File
# Begin Source File

SOURCE=.\rtif.c
# End Source File
# Begin Source File

SOURCE=.\scroll.c
# End Source File
# Begin Source File

SOURCE=.\set.c
# End Source File
# Begin Source File

SOURCE=.\share.c
# End Source File
# Begin Source File

SOURCE=.\solids_on_ray.c
# End Source File
# Begin Source File

SOURCE=.\tedit.c
# End Source File
# Begin Source File

SOURCE=.\titles.c
# End Source File
# Begin Source File

SOURCE=.\track.c
# End Source File
# Begin Source File

SOURCE=.\typein.c
# End Source File
# Begin Source File

SOURCE=.\update.c
# End Source File
# Begin Source File

SOURCE=.\usepen.c
# End Source File
# Begin Source File

SOURCE=.\utility1.c
# End Source File
# Begin Source File

SOURCE=.\utility2.c
# End Source File
# Begin Source File

SOURCE=.\vdraw.c
# End Source File
# Begin Source File

SOURCE=.\vers.c
# End Source File
# Begin Source File

SOURCE=.\vparse.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\icon2.ico
# End Source File
# Begin Source File

SOURCE=.\script.rc
# End Source File
# End Group
# End Target
# End Project
