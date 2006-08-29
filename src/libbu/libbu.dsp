# Microsoft Developer Studio Project File - Name="libbu" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libbu - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libbu.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libbu.mak" CFG="libbu - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libbu - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libbu - Win32 Release Multithreaded" (based on "Win32 (x86) Static Library")
!MESSAGE "libbu - Win32 Release Multithreaded DLL" (basierend auf "Win32 (x86) Static Library")
!MESSAGE "libbu - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "libbu - Win32 Debug Multithreaded" (based on "Win32 (x86) Static Library")
!MESSAGE "libbu - Win32 Debug Multithreaded DLL" (bbased on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "libbu"
# PROP Scc_LocalPath "."
CPP=cl.exe
F90=df.exe
RSC=rc.exe

!IF  "$(CFG)" == "libbu - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbu - Win32 Release Multithreaded"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbu - Win32 Release Multithreaded DLL"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbu - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbu - Win32 Debug Multithreaded"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libbu - Win32 Debug Multithreaded DLL"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /GZ /c
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

# Name "libbu - Win32 Release"
# Name "libbu - Win32 Release Multithreaded"
# Name "libbu - Win32 Release Multithreaded DLL"
# Name "libbu - Win32 Debug"
# Name "libbu - Win32 Debug Multithreaded"
# Name "libbu - Win32 Debug Multithreaded DLL"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\association.c
# End Source File
# Begin Source File

SOURCE=.\avs.c
# End Source File
# Begin Source File

SOURCE=.\badmagic.c
# End Source File
# Begin Source File

SOURCE=.\bitv.c
# End Source File
# Begin Source File

SOURCE=.\bomb.c
# End Source File
# Begin Source File

SOURCE=.\brlcad_path.c
# End Source File
# Begin Source File

SOURCE=.\bu_fgets.c
# End Source File
# Begin Source File

SOURCE=.\bu_tcl.c
# End Source File
# Begin Source File

SOURCE=.\cmd.c
# End Source File
# Begin Source File

SOURCE=.\cmdhist.c
# End Source File
# Begin Source File

SOURCE=.\cmdhist_obj.c
# End Source File
# Begin Source File

SOURCE=.\color.c
# End Source File
# Begin Source File

SOURCE=.\convert.c
# End Source File
# Begin Source File

SOURCE=.\fopen_uniq.c
# End Source File
# Begin Source File

SOURCE=.\getopt.c
# End Source File
# Begin Source File

SOURCE=.\hash.c
# End Source File
# Begin Source File

SOURCE=.\hist.c
# End Source File
# Begin Source File

SOURCE=.\hook.c
# End Source File
# Begin Source File

SOURCE=.\htond.c
# End Source File
# Begin Source File

SOURCE=.\htonf.c
# End Source File
# Begin Source File

SOURCE=.\ispar.c
# End Source File
# Begin Source File

SOURCE=.\lex.c
# End Source File
# Begin Source File

SOURCE=.\linebuf.c
# End Source File
# Begin Source File

SOURCE=.\list.c
# End Source File
# Begin Source File

SOURCE=.\log.c
# End Source File
# Begin Source File

SOURCE=.\magic.c
# End Source File
# Begin Source File

SOURCE=.\malloc.c
# End Source File
# Begin Source File

SOURCE=.\mappedfile.c
# End Source File
# Begin Source File

SOURCE=.\mread.c
# End Source File
# Begin Source File

SOURCE=.\mro.c
# End Source File
# Begin Source File

SOURCE=.\observer.c
# End Source File
# Begin Source File

SOURCE=.\parallel.c
# End Source File
# Begin Source File

SOURCE=.\parse.c
# End Source File
# Begin Source File

SOURCE=.\printb.c
# End Source File
# Begin Source File

SOURCE=.\ptbl.c
# End Source File
# Begin Source File

SOURCE=.\rb_create.c
# End Source File
# Begin Source File

SOURCE=.\rb_delete.c
# End Source File
# Begin Source File

SOURCE=.\rb_diag.c
# End Source File
# Begin Source File

SOURCE=.\rb_extreme.c
# End Source File
# Begin Source File

SOURCE=.\rb_free.c
# End Source File
# Begin Source File

SOURCE=.\rb_insert.c
# End Source File
# Begin Source File

SOURCE=.\rb_order_stats.c
# End Source File
# Begin Source File

SOURCE=.\rb_rotate.c
# End Source File
# Begin Source File

SOURCE=.\rb_search.c
# End Source File
# Begin Source File

SOURCE=.\rb_walk.c
# End Source File
# Begin Source File

SOURCE=.\semaphore.c
# End Source File
# Begin Source File

SOURCE=.\stat.c
# End Source File
# Begin Source File

SOURCE=.\units.c
# End Source File
# Begin Source File

SOURCE=.\vers.c
# End Source File
# Begin Source File

SOURCE=.\vfont.c
# End Source File
# Begin Source File

SOURCE=.\vls.c
# End Source File
# Begin Source File

SOURCE=.\whereis.c
# End Source File
# Begin Source File

SOURCE=.\which.c
# End Source File
# Begin Source File

SOURCE=.\xdr.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Source File

SOURCE=..\..\configure.ac

!IF  "$(CFG)" == "libbu - Win32 Release"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs bu_version The BRL-CAD Utility Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "libbu - Win32 Release Multithreaded"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs bu_version The BRL-CAD Utility Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "libbu - Win32 Release Multithreaded DLL"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs bu_version The BRL-CAD Utility Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "libbu - Win32 Debug"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs bu_version The BRL-CAD Utility Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "libbu - Win32 Debug Multithreaded"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs bu_version The BRL-CAD Utility Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "libbu - Win32 Debug Multithreaded DLL"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs bu_version The BRL-CAD Utility Library > $(ProjDir)\vers.c

# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
