# Microsoft Developer Studio Project File - Name="libBLT" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libBLT - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libBLT.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libBLT.mak" CFG="libBLT - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libBLT - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libBLT - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libBLT - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBBLT_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../include" /I "../../../src/other/blt/src" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/bitmaps" /I "../../../src/other/libtk/xlib" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "NDEBUG" /D "CONSOLE" /D "_MBCS" /D "_USRDLL" /D TCL_THREADS=1 /D "_X86_" /D "__STDC__" /D "_MT" /D "IGNORE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib advapi32.lib user32.lib shell32.lib gdi32.lib comdlg32.lib winspool.lib imm32.lib comctl32.lib oldnames.lib tcl84.lib tk84.lib /nologo /dll /machine:I386 /out:"Release/BLT24.dll" /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Release
TargetPath=.\Release\BLT24.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\BLT24.lib ..\..\..\lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "libBLT - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBBLT_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../../../src/other/blt/src" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/bitmaps" /I "../../../src/other/libtk/xlib" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "_DEBUG" /D "CONSOLE" /D "_MBCS" /D "_USRDLL" /D TCL_THREADS=1 /D "_X86_" /D "__STDC__" /D "_MT" /D "IGNORE_CONFIG_H" /YX /FD /GZ /c
# SUBTRACT CPP /X
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib advapi32.lib user32.lib shell32.lib gdi32.lib comdlg32.lib winspool.lib imm32.lib comctl32.lib oldnames.lib tcl84_d.lib tk84_d.lib /nologo /dll /debug /machine:I386 /out:"Debug/BLT24_d.dll" /pdbtype:sept /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Debug
TargetPath=.\Debug\BLT24_d.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\BLT24_d.lib ..\..\..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "libBLT - Win32 Release"
# Name "libBLT - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;f90;for;f;fpp"
# Begin Source File

SOURCE=../../../src/other/blt/src/bltAlloc.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltArrayObj.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltBeep.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltBgexec.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltBind.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltBitmap.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltBusy.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltCanvEps.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltChain.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltColor.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltConfig.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltContainer.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltCutbuffer.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltDebug.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltDragdrop.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGraph.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrAxis.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrBar.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrElem.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrGrid.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrHairs.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrLegd.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrLine.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrMarker.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrMisc.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrPen.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltGrPs.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltHash.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltHierbox.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltHtext.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltImage.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltInit.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltList.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltNsUtil.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltObjConfig.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltParse.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltPool.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltPs.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltScrollbar.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltSpline.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltSwitch.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTable.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTabnotebook.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTabset.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTed.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltText.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTile.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTree.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTreeCmd.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTreeView.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTreeViewCmd.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTreeViewColumn.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTreeViewEdit.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltTreeViewStyle.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltUtil.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltVecCmd.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltVecMath.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltVecObjCmd.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltVector.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWatch.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWinDde.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWindow.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWinDraw.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWinImage.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWinMain.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWinop.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWinPipe.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWinPrnt.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/bltWinUtil.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/pure_api.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/tkButton.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/tkConsole.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/tkFrame.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/blt/src/tkScrollbar.c
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
