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
!MESSAGE "libbu - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libbu - Win32 Release"

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

!ELSEIF  "$(CFG)" == "libbu - Win32 Debug"

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
# ADD LIB32 /nologo /out:"Debug\libbu_d.lib"
# Begin Custom Build
TargetPath=.\Debug\libbu_d.lib
TargetName=libbu_d
InputPath=.\Debug\libbu_d.lib
SOURCE="$(InputPath)"

"$(TargetName).lib" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetPath) "..\lib"

# End Custom Build

!ENDIF 

# Begin Target

# Name "libbu - Win32 Release"
# Name "libbu - Win32 Debug"
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

SOURCE=.\memset.c
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

SOURCE=.\xdr.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
