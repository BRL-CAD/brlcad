# Microsoft Developer Studio Project File - Name="tgf2g" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT**

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=tgf2g - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "tgf2g.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "tgf2g.mak" CFG="tgf2g - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "tgf2g - Win32 Release" (based on  "Win32 (x86) Application")
!MESSAGE "tgf2g - Win32 Release (brlcad.dll)" (based on  "Win32 (x86) Application")
!MESSAGE "tgf2g - Win32 Debug" (based on  "Win32 (x86) Application")
!MESSAGE "tgf2g - Win32 Debug (brlcad.dll)" (based on  "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "tgf2g"
# PROP Scc_LocalPath "."
CPP=cl.exe
F90=df.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "tgf2g - Win32 Release"

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
# ADD BASE F90 /compile_only /nologo /warn:nofileopt /winapp
# ADD F90 /compile_only /nologo /warn:nofileopt /winapp
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\include" /I "..\tcl\generic" /I "..\openNURBS" /I "..\libz" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib ws2_32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib rpcrt4.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib ws2_32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib rpcrt4.lib libbn.lib libbu.lib libregex.lib librt.lib libsysv.lib libwdb.lib opennurbs.lib libz.lib /nologo /subsystem:console /machine:I386 /out:"Release/tgf-g.exe" /libpath:"..\..\..\misc\win32-msvc\Release"

!ELSEIF  "$(CFG)" == "tgf2g - Win32 Release (brlcad.dll)"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseDll"
# PROP BASE Intermediate_Dir "ReleaseDll"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseDll"
# PROP Intermediate_Dir "ReleaseDll"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE F90 /compile_only /nologo /warn:nofileopt /winapp
# ADD F90 /compile_only /nologo /warn:nofileopt /winapp
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\include" /I "..\tcl\generic" /I "..\openNURBS" /I "..\libz" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "BRLCAD_DLL" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:console /machine:I386
# ADD LINK32 brlcad.lib /nologo /subsystem:console /machine:I386 /out:"ReleaseDll/tgf-g.exe" /libpath:"..\..\..\misc\win32-msvc\ReleaseMt"

!ELSEIF  "$(CFG)" == "tgf2g - Win32 Debug"

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
# ADD BASE F90 /check:bounds /compile_only /dbglibs /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt /winapp
# ADD F90 /browser /check:bounds /compile_only /dbglibs /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt /winapp
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\include" /I "..\tcl\generic" /I "..\openNURBS" /I "..\libz" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FR /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib ws2_32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib rpcrt4.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib ws2_32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib rpcrt4.lib libbn.lib libbu.lib libregex.lib librt.lib libsysv.lib libwdb.lib opennurbs.lib libz.lib /nologo /subsystem:console /debug /machine:I386 /out:"Debug/tgf-g.exe" /pdbtype:sept /libpath:"..\..\..\misc\win32-msvc\Debug"

!ELSEIF  "$(CFG)" == "tgf2g - Win32 Debug (brlcad.dll)"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DebugDll"
# PROP BASE Intermediate_Dir "DebugDll"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugDll"
# PROP Intermediate_Dir "DebugDll"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE F90 /check:bounds /compile_only /dbglibs /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt /winapp
# ADD F90 /browser /check:bounds /compile_only /dbglibs /debug:full /nologo /traceback /warn:argument_checking /warn:nofileopt /winapp
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\..\..\include" /I "..\tcl\generic" /I "..\openNURBS" /I "..\libz" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "BRLCAD_DLL" /D "HAVE_CONFIG_H" /D "BRLCADBUILD" /FR /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 brlcad.lib /nologo /subsystem:console /debug /machine:I386 /out:"DebugDll/tgf-g.exe" /pdbtype:sept /libpath:"..\..\..\misc\win32-msvc\DebugMt"

!ENDIF 

# Begin Target

# Name "tgf2g - Win32 Release"
# Name "tgf2g - Win32 Release (brlcad.dll)"
# Name "tgf2g - Win32 Debug"
# Name "tgf2g - Win32 Debug (brlcad.dll)"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\read_dra.cpp
# End Source File
# Begin Source File

SOURCE=.\regtab.cpp
# End Source File
# Begin Source File

SOURCE=".\tgf-g.cpp"
# End Source File
# Begin Source File

SOURCE=.\write_brl.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\glob.h
# End Source File
# Begin Source File

SOURCE=.\read_dra.h
# End Source File
# Begin Source File

SOURCE=.\regtab.h
# End Source File
# Begin Source File

SOURCE=.\write_brl.h
# End Source File
# End Group
# Begin Group "Tools"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\misc\win32-msvc\Dll\TclDummies.c

!IF  "$(CFG)" == "tgf2g - Win32 Release"

!ELSEIF  "$(CFG)" == "tgf2g - Win32 Release (brlcad.dll)"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "tgf2g - Win32 Debug"

!ELSEIF  "$(CFG)" == "tgf2g - Win32 Debug (brlcad.dll)"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
