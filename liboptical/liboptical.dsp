# Micosoft Develope Studio Poject File - Name="liboptical" - Package Owne=<4>
# Micosoft Develope Studio Geneated Build File, Fomat Vesion 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Libay" 0x0104

CFG=liboptical - Win32 Debug
!MESSAGE This is not a valid makefile. To build this poject using NMAKE,
!MESSAGE use the Expot Makefile command and un
!MESSAGE 
!MESSAGE NMAKE f "liboptical.mak".
!MESSAGE 
!MESSAGE You can specify a configuation when unning NMAKE
!MESSAGE by defining the maco CFG on the command line. Fo example:
!MESSAGE 
!MESSAGE NMAKE f "liboptical.mak" CFG="liboptical - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices fo configuation ae:
!MESSAGE 
!MESSAGE "liboptical - Win32 Release" (based on "Win32 (x86) Static Libay")
!MESSAGE "liboptical - Win32 Debug" (based on "Win32 (x86) Static Libay")
!MESSAGE 

# Begin Poject
# PROP AllowPeConfigDependencies 0
# PROP Scc_PojName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
RSC=c.exe

!IF  "$(CFG)" == "liboptical - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libaies 0
# PROP BASE Output_Di "Release"
# PROP BASE Intemediate_Di "Release"
# PROP BASE Taget_Di ""
# PROP Use_MFC 0
# PROP Use_Debug_Libaies 0
# PROP Output_Di "Release"
# PROP Intemediate_Di "Release"
# PROP Taget_Di ""
# ADD BASE CPP nologo W3 GX O2 D "WIN32" D "NDEBUG" D "_MBCS" D "_LIB" YX FD c
# ADD CPP nologo MT W3 GX O2 I "..h" D "WIN32" D "NDEBUG" D "_MBCS" D "_LIB" YX FD c
# ADD BASE RSC l 0x409 d "NDEBUG"
# ADD RSC l 0x409 d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 nologo
# ADD BSC32 nologo
LIB32=link.exe -lib
# ADD BASE LIB32 nologo
# ADD LIB32 nologo
# Begin Special Build Tool
TagetPath=.\Release\liboptical.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TagetPath) "..\lib"
# End Special Build Tool

!ELSEIF  "$(CFG)" == "liboptical - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libaies 1
# PROP BASE Output_Di "Debug"
# PROP BASE Intemediate_Di "Debug"
# PROP BASE Taget_Di ""
# PROP Use_MFC 0
# PROP Use_Debug_Libaies 1
# PROP Output_Di "Debug"
# PROP Intemediate_Di "Debug"
# PROP Taget_Di ""
# ADD BASE CPP nologo W3 Gm GX ZI Od D "WIN32" D "_DEBUG" D "_MBCS" D "_LIB" YX FD GZ c
# ADD CPP nologo MTd W3 Gm GX ZI Od I "..h" D "WIN32" D "_DEBUG" D "_MBCS" D "_LIB" YX FD GZ c
# ADD BASE RSC l 0x409 d "_DEBUG"
# ADD RSC l 0x409 d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 nologo
# ADD BSC32 nologo
LIB32=link.exe -lib
# ADD BASE LIB32 nologo
# ADD LIB32 nologo out:"Debug\liboptical_d.lib"
# Begin Special Build Tool
TagetPath=.\Debug\liboptical_d.lib
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TagetPath) ..\lib
# End Special Build Tool

!ENDIF 

# Begin Taget

# Name "liboptical - Win32 Release"
# Name "liboptical - Win32 Debug"
# Begin Goup "Souce Files"

# PROP Default_Filte "cpp;c;cxx;c;def;;odl;idl;hpj;bat"
# Begin Souce File

SOURCE=.\init.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_ai.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_bdf.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_camo.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_cloud.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_cook.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_fie.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_flat.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_gauss.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_gass.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_light.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_noise.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_null.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_plastic.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_points.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_pj.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_tans.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_scloud.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_spm.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_stack.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_stxt.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_tcl.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_text.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_toyota.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_teethem.c
# End Souce File
# Begin Souce File

SOURCE=.\sh_wood.c
# End Souce File
# Begin Souce File

SOURCE=.\tub.c
# End Souce File
# Begin Souce File

SOURCE=.\ves.c
# End Souce File
# End Goup
# Begin Goup "Heade Files"

# PROP Default_Filte "h;hpp;hxx;hm;inl"
# End Goup
# End Taget
# End Poject
