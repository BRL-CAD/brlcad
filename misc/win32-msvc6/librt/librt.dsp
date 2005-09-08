# Microsoft Developer Studio Project File - Name="librt" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

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
!MESSAGE "librt - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "librt - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
F90=df.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "librt - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBRT_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../include" /I "../../../src/librt" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RT_EXPORT_DLL" /D "DB5_EXPORT_DLL" /D TCL_THREADS=1 /D "__win32" /D "BRLCAD_DLL" /D "NEW_TOPS_BEHAVIOR" /D "IMPORT_FASTGEN4_SECTION" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib advapi32.lib tcl84.lib libbu.lib libbn.lib libsysv.lib /nologo /dll /machine:I386 /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Release
TargetPath=.\Release\librt.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\librt.lib ..\..\..\lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "librt - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBRT_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../../../src/librt" /I "../../../src/other/libtcl/generic" /I "../../../src/other/libtcl/win" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "RT_EXPORT_DLL" /D TCL_THREADS=1 /D "__win32" /D "BRLCAD_DEBUG" /D "BRLCAD_DLL" /D "NEW_TOPS_BEHAVIOR" /D "IMPORT_FASTGEN4_SECTION" /D "HAVE_CONFIG_H" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib advapi32.lib tcl84_d.lib libbu_d.lib libbn_d.lib libsysv_d.lib /nologo /dll /debug /machine:I386 /out:"Debug/librt_d.dll" /pdbtype:sept /libpath:"../../../lib"
# Begin Special Build Tool
TargetDir=.\Debug
TargetPath=.\Debug\librt_d.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(TargetPath) ..\..\..\bin	copy $(TargetDir)\librt_d.lib ..\..\..\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "librt - Win32 Release"
# Name "librt - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;f90;for;f;fpp"
# Begin Source File

SOURCE=../../../src/librt/bezier_2d_isect.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/bigE.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/binary_obj.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/bomb.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/bool.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/bundle.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/cmd.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/comb.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/cut.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db5_alloc.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db5_bin.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db5_comb.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db5_io.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db5_scan.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db5_types.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_alloc.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_anim.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_comb.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_io.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_lookup.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_match.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_open.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_path.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_scan.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_tree.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/db_walk.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/dg_obj.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/dir.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/fortray.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_arb.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_arbn.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_ars.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_bot.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_cline.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_dsp.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_ebm.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_ehy.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_ell.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_epa.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_eto.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_extrude.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_grip.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_half.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_hf.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_nmg.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_nurb.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_part.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_pg.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_pipe.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_rec.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_rhc.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_rpc.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_sketch.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_sph.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_submodel.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_superell.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_tgc.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_torus.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/g_vol.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/global.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/htbl.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/importFg4Section.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/many.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/mater.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/memalloc.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/mkbundle.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nirt.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_bool.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_ck.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_class.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_eval.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_extrude.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_fcut.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_fuse.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_index.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_info.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_inter.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_manif.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_mesh.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_misc.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_mk.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_mod.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_plot.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_pr.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_pt_fu.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_rt_isect.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_rt_segs.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_tri.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nmg_visit.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_basis.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_bezier.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_bound.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_c2.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_copy.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_diff.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_eval.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_flat.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_interp.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_knot.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_norm.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_plot.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_poly.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_ray.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_refine.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_reverse.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_solve.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_split.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_tess.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_trim.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_trim_util.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_util.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/nurb_xsplit.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/oslo_calc.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/oslo_map.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/parse.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/pr.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/prep.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/qray.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/regionfix.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/roots.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/rt_dspline.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/shoot.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/spectrum.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/storage.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/table.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/tcl.c
# End Source File
# Begin Source File

SOURCE="../../../src/librt/timer-nt.c"
# End Source File
# Begin Source File

SOURCE=../../../src/librt/track.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/tree.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/vdraw.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/vers.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/view_obj.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/vlist.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/wdb.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/wdb_comb_std.c
# End Source File
# Begin Source File

SOURCE=../../../src/librt/wdb_obj.c
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
