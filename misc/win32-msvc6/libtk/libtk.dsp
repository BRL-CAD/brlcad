# Microsoft Developer Studio Project File - Name="libtk" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libtk - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libtk.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libtk.mak" CFG="libtk - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libtk - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libtk - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libtk - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBTK_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../include" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/bitmaps" /I "../../../src/other/libtk/xlib" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "USE_TCL_STUBS" /D TCL_THREADS=1 /D "BUILD_tk" /D "DOUBLE_BAR_TEAROFF" /D "IGNORE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /i "../../../include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib advapi32.lib user32.lib shell32.lib gdi32.lib comdlg32.lib winspool.lib imm32.lib comctl32.lib tclstub84.lib /nologo /dll /machine:I386 /nodefaultlib:"libcmt" /out:"Release/tk84.dll" /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Release
TargetPath=.\Release\tk84.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\tk84.lib ..\..\..\lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "libtk - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBTK_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../../../src/other/libtk/generic" /I "../../../src/other/libtk/win" /I "../../../src/other/libtk/bitmaps" /I "../../../src/other/libtk/xlib" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "USE_TCL_STUBS" /D TCL_THREADS=1 /D "BUILD_tk" /D "DOUBLE_BAR_TEAROFF" /D "IGNORE_CONFIG_H" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /i "../../../include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib advapi32.lib user32.lib shell32.lib gdi32.lib comdlg32.lib winspool.lib imm32.lib comctl32.lib tclstub84_d.lib /nologo /dll /debug /machine:I386 /nodefaultlib:"libcmt" /out:"Debug/tk84_d.dll" /pdbtype:sept /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Debug
TargetPath=.\Debug\tk84_d.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\tk84_d.lib ..\..\..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "libtk - Win32 Release"
# Name "libtk - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;f90;for;f;fpp"
# Begin Source File

SOURCE=../../../src/other/libtk/win/stubs.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tk3d.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkArgv.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkAtom.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkBind.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkBitmap.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkButton.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvArc.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvas.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvBmap.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvImg.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvLine.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvPoly.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvPs.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvText.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvUtil.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCanvWind.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkClipboard.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCmds.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkColor.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkConfig.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkConsole.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkCursor.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkEntry.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkError.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkEvent.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkFileFilter.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkFocus.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkFont.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkFrame.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkGC.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkGeometry.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkGet.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkGrab.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkGrid.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkImage.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkImgBmap.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkImgGIF.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkImgPhoto.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkImgPPM.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkImgUtil.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkListbox.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkMacWinMenu.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkMain.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkMenu.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkMenubutton.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkMenuDraw.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkMessage.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkObj.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkOldConfig.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkOption.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkPack.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkPanedWindow.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkPlace.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkPointer.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkRectOval.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkScale.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkScrollbar.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkSelect.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkStubInit.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkStubLib.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkStyle.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkText.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkTextBTree.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkTextDisp.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkTextImage.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkTextIndex.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkTextMark.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkTextTag.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkTextWind.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkTrig.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkUndo.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/unix/tkUnixMenubu.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/unix/tkUnixScale.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkUtil.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkVisual.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWin32Dll.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWin3d.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinButton.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinClipboard.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinColor.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinConfig.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinCursor.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinDialog.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/generic/tkWindow.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinDraw.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinEmbed.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinFont.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinImage.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinInit.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinKey.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinMenu.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinPixmap.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinPointer.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinRegion.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinScrlbr.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinSend.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinWindow.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinWm.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/win/tkWinX.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/xlib/xcolors.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/xlib/xdraw.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/xlib/xgc.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/xlib/ximage.c
# End Source File
# Begin Source File

SOURCE=../../../src/other/libtk/xlib/xutil.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\buttons.bmp
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor00.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor02.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor04.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor06.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor08.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor0a.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor0c.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor0e.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor10.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor12.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor14.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor16.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor18.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor1a.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor1c.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor1e.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor20.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor22.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor24.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor26.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor28.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor2a.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor2c.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor2e.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor30.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor32.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor34.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor36.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor38.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor3a.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor3c.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor3e.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor40.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor42.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor44.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor46.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor48.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor4a.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor4c.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor4e.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor50.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor52.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor54.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor56.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor58.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor5a.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor5c.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor5e.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor60.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor62.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor64.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor66.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor68.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor6a.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor6c.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor6e.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor70.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor72.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor74.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor76.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor78.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor7a.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor7c.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor7e.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor80.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor82.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor84.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor86.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor88.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor8a.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor8c.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor8e.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor90.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor92.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor94.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor96.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\cursor98.cur
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\tk.ico
# End Source File
# Begin Source File

SOURCE=..\..\..\src\other\libtk\win\rc\tk.rc
# End Source File
# End Group
# End Target
# End Project
