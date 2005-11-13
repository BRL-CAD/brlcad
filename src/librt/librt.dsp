# Microsoft Developer Studio Project File - Name="librt" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=librt - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "librt.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "librt.mak" CFG="librt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "librt - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "librt - Win32 Release Multithreaded" (based on "Win32 (x86) Static Library")
!MESSAGE "librt - Win32 Release Multithreaded DLL" (basierend auf "Win32 (x86) Static Library")
!MESSAGE "librt - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "librt - Win32 Debug Multithreaded" (based on "Win32 (x86) Static Library")
!MESSAGE "librt - Win32 Debug Multithreaded DLL" (bbased on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "librt"
# PROP Scc_LocalPath "."
CPP=cl.exe
F90=df.exe
RSC=rc.exe

!IF  "$(CFG)" == "librt - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "librt - Win32 Release Multithreaded"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "librt - Win32 Release Multithreaded DLL"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "librt - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "librt - Win32 Debug Multithreaded"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "librt - Win32 Debug Multithreaded DLL"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
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

# Name "librt - Win32 Release"
# Name "librt - Win32 Release Multithreaded"
# Name "librt - Win32 Release Multithreaded DLL"
# Name "librt - Win32 Debug"
# Name "librt - Win32 Debug Multithreaded"
# Name "librt - Win32 Debug Multithreaded DLL"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\bezier_2d_isect.c
# End Source File
# Begin Source File

SOURCE=.\bigE.c
# End Source File
# Begin Source File

SOURCE=.\binary_obj.c
# End Source File
# Begin Source File

SOURCE=.\bomb.c
# End Source File
# Begin Source File

SOURCE=.\bool.c
# End Source File
# Begin Source File

SOURCE=.\bundle.c
# End Source File
# Begin Source File

SOURCE=.\cmd.c
# End Source File
# Begin Source File

SOURCE=.\cut.c
# End Source File
# Begin Source File

SOURCE=.\db5_alloc.c
# End Source File
# Begin Source File

SOURCE=.\db5_bin.c
# End Source File
# Begin Source File

SOURCE=.\db5_comb.c
# End Source File
# Begin Source File

SOURCE=.\db5_io.c
# End Source File
# Begin Source File

SOURCE=.\db5_scan.c
# End Source File
# Begin Source File

SOURCE=.\db5_types.c
# End Source File
# Begin Source File

SOURCE=.\db_alloc.c
# End Source File
# Begin Source File

SOURCE=.\db_anim.c
# End Source File
# Begin Source File

SOURCE=.\db_comb.c
# End Source File
# Begin Source File

SOURCE=.\db_io.c
# End Source File
# Begin Source File

SOURCE=.\db_lookup.c
# End Source File
# Begin Source File

SOURCE=.\db_match.c
# End Source File
# Begin Source File

SOURCE=.\db_open.c
# End Source File
# Begin Source File

SOURCE=.\db_path.c
# End Source File
# Begin Source File

SOURCE=.\db_scan.c
# End Source File
# Begin Source File

SOURCE=.\db_tree.c
# End Source File
# Begin Source File

SOURCE=.\db_walk.c
# End Source File
# Begin Source File

SOURCE=.\dg_obj.c
# End Source File
# Begin Source File

SOURCE=.\dir.c
# End Source File
# Begin Source File

SOURCE=.\fortray.c
# End Source File
# Begin Source File

SOURCE=.\g_arb.c
# End Source File
# Begin Source File

SOURCE=.\g_arbn.c
# End Source File
# Begin Source File

SOURCE=.\g_ars.c
# End Source File
# Begin Source File

SOURCE=.\g_bot.c
# End Source File
# Begin Source File

SOURCE=.\g_cline.c
# End Source File
# Begin Source File

SOURCE=.\g_dsp.c
# End Source File
# Begin Source File

SOURCE=.\g_ebm.c
# End Source File
# Begin Source File

SOURCE=.\g_ehy.c
# End Source File
# Begin Source File

SOURCE=.\g_ell.c
# End Source File
# Begin Source File

SOURCE=.\g_epa.c
# End Source File
# Begin Source File

SOURCE=.\g_eto.c
# End Source File
# Begin Source File

SOURCE=.\g_extrude.c
# End Source File
# Begin Source File

SOURCE=.\g_grip.c
# End Source File
# Begin Source File

SOURCE=.\g_half.c
# End Source File
# Begin Source File

SOURCE=.\g_hf.c
# End Source File
# Begin Source File

SOURCE=.\g_nmg.c
# End Source File
# Begin Source File

SOURCE=.\g_nurb.c
# End Source File
# Begin Source File

SOURCE=.\g_part.c
# End Source File
# Begin Source File

SOURCE=.\g_pg.c
# End Source File
# Begin Source File

SOURCE=.\g_pipe.c
# End Source File
# Begin Source File

SOURCE=.\g_rec.c
# End Source File
# Begin Source File

SOURCE=.\g_rhc.c
# End Source File
# Begin Source File

SOURCE=.\g_rpc.c
# End Source File
# Begin Source File

SOURCE=.\g_sketch.c
# End Source File
# Begin Source File

SOURCE=.\g_sph.c
# End Source File
# Begin Source File

SOURCE=.\g_submodel.c
# End Source File
# Begin Source File

SOURCE=.\g_superell.c
# End Source File
# Begin Source File

SOURCE=.\g_tgc.c
# End Source File
# Begin Source File

SOURCE=.\g_torus.c
# End Source File
# Begin Source File

SOURCE=.\g_vol.c
# End Source File
# Begin Source File

SOURCE=.\global.c
# End Source File
# Begin Source File

SOURCE=.\htbl.c
# End Source File
# Begin Source File

SOURCE=.\importFg4Section.c
# End Source File
# Begin Source File

SOURCE=.\many.c
# End Source File
# Begin Source File

SOURCE=.\mater.c
# End Source File
# Begin Source File

SOURCE=.\memalloc.c
# End Source File
# Begin Source File

SOURCE=.\mkbundle.c
# End Source File
# Begin Source File

SOURCE=.\nirt.c
# End Source File
# Begin Source File

SOURCE=.\nmg_bool.c
# End Source File
# Begin Source File

SOURCE=.\nmg_ck.c
# End Source File
# Begin Source File

SOURCE=.\nmg_class.c
# End Source File
# Begin Source File

SOURCE=.\nmg_eval.c
# End Source File
# Begin Source File

SOURCE=.\nmg_extrude.c
# End Source File
# Begin Source File

SOURCE=.\nmg_fcut.c
# End Source File
# Begin Source File

SOURCE=.\nmg_fuse.c
# End Source File
# Begin Source File

SOURCE=.\nmg_index.c
# End Source File
# Begin Source File

SOURCE=.\nmg_info.c
# End Source File
# Begin Source File

SOURCE=.\nmg_inter.c
# End Source File
# Begin Source File

SOURCE=.\nmg_manif.c
# End Source File
# Begin Source File

SOURCE=.\nmg_mesh.c
# End Source File
# Begin Source File

SOURCE=.\nmg_misc.c
# End Source File
# Begin Source File

SOURCE=.\nmg_mk.c
# End Source File
# Begin Source File

SOURCE=.\nmg_mod.c
# End Source File
# Begin Source File

SOURCE=.\nmg_plot.c
# End Source File
# Begin Source File

SOURCE=.\nmg_pr.c
# End Source File
# Begin Source File

SOURCE=.\nmg_pt_fu.c
# End Source File
# Begin Source File

SOURCE=.\nmg_rt_isect.c
# End Source File
# Begin Source File

SOURCE=.\nmg_rt_segs.c
# End Source File
# Begin Source File

SOURCE=.\nmg_tri.c
# End Source File
# Begin Source File

SOURCE=.\nmg_visit.c
# End Source File
# Begin Source File

SOURCE=.\nurb_basis.c
# End Source File
# Begin Source File

SOURCE=.\nurb_bezier.c
# End Source File
# Begin Source File

SOURCE=.\nurb_bound.c
# End Source File
# Begin Source File

SOURCE=.\nurb_c2.c
# End Source File
# Begin Source File

SOURCE=.\nurb_copy.c
# End Source File
# Begin Source File

SOURCE=.\nurb_diff.c
# End Source File
# Begin Source File

SOURCE=.\nurb_eval.c
# End Source File
# Begin Source File

SOURCE=.\nurb_flat.c
# End Source File
# Begin Source File

SOURCE=.\nurb_interp.c
# End Source File
# Begin Source File

SOURCE=.\nurb_knot.c
# End Source File
# Begin Source File

SOURCE=.\nurb_norm.c
# End Source File
# Begin Source File

SOURCE=.\nurb_plot.c
# End Source File
# Begin Source File

SOURCE=.\nurb_poly.c
# End Source File
# Begin Source File

SOURCE=.\nurb_ray.c
# End Source File
# Begin Source File

SOURCE=.\nurb_refine.c
# End Source File
# Begin Source File

SOURCE=.\nurb_reverse.c
# End Source File
# Begin Source File

SOURCE=.\nurb_solve.c
# End Source File
# Begin Source File

SOURCE=.\nurb_split.c
# End Source File
# Begin Source File

SOURCE=.\nurb_tess.c
# End Source File
# Begin Source File

SOURCE=.\nurb_trim.c
# End Source File
# Begin Source File

SOURCE=.\nurb_trim_util.c
# End Source File
# Begin Source File

SOURCE=.\nurb_util.c
# End Source File
# Begin Source File

SOURCE=.\nurb_xsplit.c
# End Source File
# Begin Source File

SOURCE=.\oslo_calc.c
# End Source File
# Begin Source File

SOURCE=.\oslo_map.c
# End Source File
# Begin Source File

SOURCE=.\pr.c
# End Source File
# Begin Source File

SOURCE=.\prep.c
# End Source File
# Begin Source File

SOURCE=.\qray.c
# End Source File
# Begin Source File

SOURCE=.\regionfix.c
# End Source File
# Begin Source File

SOURCE=.\roots.c
# End Source File
# Begin Source File

SOURCE=.\rt_dspline.c
# End Source File
# Begin Source File

SOURCE=.\shoot.c
# End Source File
# Begin Source File

SOURCE=.\spectrum.c
# End Source File
# Begin Source File

SOURCE=.\storage.c
# End Source File
# Begin Source File

SOURCE=.\table.c
# End Source File
# Begin Source File

SOURCE=.\tcl.c
# End Source File
# Begin Source File

SOURCE=".\timer-nt.c"
# End Source File
# Begin Source File

SOURCE=.\track.c
# End Source File
# Begin Source File

SOURCE=.\tree.c
# End Source File
# Begin Source File

SOURCE=.\vdraw.c
# End Source File
# Begin Source File

SOURCE=.\vers.c
# End Source File
# Begin Source File

SOURCE=.\view_obj.c
# End Source File
# Begin Source File

SOURCE=.\vlist.c
# End Source File
# Begin Source File

SOURCE=.\wdb.c
# End Source File
# Begin Source File

SOURCE=.\wdb_comb_std.c
# End Source File
# Begin Source File

SOURCE=.\wdb_obj.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Source File

SOURCE=..\..\configure.ac

!IF  "$(CFG)" == "librt - Win32 Release"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs rt_version The BRL-CAD Ray-Tracing Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "librt - Win32 Release Multithreaded"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs rt_version The BRL-CAD Ray-Tracing Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "librt - Win32 Release Multithreaded DLL"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs rt_version The BRL-CAD Ray-Tracing Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "librt - Win32 Debug"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs rt_version The BRL-CAD Ray-Tracing Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "librt - Win32 Debug Multithreaded"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs rt_version The BRL-CAD Ray-Tracing Library > $(ProjDir)\vers.c

# End Custom Build

!ELSEIF  "$(CFG)" == "librt - Win32 Debug Multithreaded DLL"

# Begin Custom Build
ProjDir=.
InputPath=..\..\configure.ac

"$(ProjDir)\vers.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cscript //nologo ..\..\misc\win32-msvc\vers.vbs rt_version The BRL-CAD Ray-Tracing Library > $(ProjDir)\vers.c

# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
