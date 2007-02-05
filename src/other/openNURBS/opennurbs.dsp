# Microsoft Developer Studio Project File - Name="opennurbs" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=opennurbs - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "opennurbs.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "opennurbs.mak" CFG="opennurbs - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "opennurbs - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "opennurbs - Win32 Release Multithreaded" (based on "Win32 (x86) Static Library")
!MESSAGE "opennurbs - Win32 Release Multithreaded DLL" (basierend auf "Win32 (x86) Static Library")
!MESSAGE "opennurbs - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "opennurbs - Win32 Debug Multithreaded" (based on "Win32 (x86) Static Library")
!MESSAGE "opennurbs - Win32 Debug Multithreaded DLL" (bbased on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "opennurbs"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "opennurbs - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\misc\win32-msvc\Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../../../include" /I "../libz" /D "WIN32" /D "NDEBUG" /D "UNICODE" /D "_LIB" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "opennurbs - Win32 Release Multithreaded"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\misc\win32-msvc\ReleaseMt"
# PROP Intermediate_Dir "ReleaseMt"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../../include" /I "../libz" /D "WIN32" /D "NDEBUG" /D "UNICODE" /D "_LIB" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "opennurbs - Win32 Release Multithreaded DLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\misc\win32-msvc\ReleaseMtDll"
# PROP Intermediate_Dir "ReleaseMtDll"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../../include" /I "../libz" /D "WIN32" /D "NDEBUG" /D "UNICODE" /D "_LIB" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "opennurbs - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\misc\win32-msvc\Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../libz" /D "WIN32" /D "_DEBUG" /D "UNICODE" /D "_LIB" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "opennurbs - Win32 Debug Multithreaded"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\misc\win32-msvc\DebugMt"
# PROP Intermediate_Dir "DebugMt"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../libz" /D "WIN32" /D "_DEBUG" /D "UNICODE" /D "_LIB" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "opennurbs - Win32 Debug Multithreaded DLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\misc\win32-msvc\DebugMtDll"
# PROP Intermediate_Dir "DebugMtDll"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../libz" /D "WIN32" /D "_DEBUG" /D "UNICODE" /D "_LIB" /FD /GZ /c
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

# Name "opennurbs - Win32 Release"
# Name "opennurbs - Win32 Release Multithreaded"
# Name "opennurbs - Win32 Release Multithreaded DLL"
# Name "opennurbs - Win32 Debug"
# Name "opennurbs - Win32 Debug Multithreaded"
# Name "opennurbs - Win32 Debug Multithreaded DLL"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\opennurbs_3dm_attributes.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_3dm_properties.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_3dm_settings.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_annotation.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_annotation2.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_arc.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_arccurve.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_archive.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_array.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_basic.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_bezier.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_beziervolume.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_bitmap.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_bounding_box.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_brep.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_brep_changesrf.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_brep_extrude.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_brep_io.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_brep_isvalid.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_brep_kinky.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_brep_tools.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_brep_v2valid.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_circle.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_color.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_cone.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_crc.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_curve.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_curveonsurface.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_curveproxy.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_cylinder.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_defines.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_detail.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_dimstyle.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_ellipse.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_error.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_error_message.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_evaluate_nurbs.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_extensions.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_font.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_geometry.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_group.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_hatch.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_instance.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_intersect.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_knot.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_layer.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_light.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_line.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_linecurve.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_linetype.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_massprop.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_material.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_math.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_matrix.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_memory.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\opennurbs_memory_new.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_memory_util.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\opennurbs_mesh.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_mesh_tools.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_morph.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_nurbscurve.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_nurbssurface.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_nurbsvolume.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_object.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_object_history.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_objref.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_offsetsurface.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_optimize.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_plane.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_planesurface.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_pluginlist.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_point.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_pointcloud.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_pointgeometry.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_pointgrid.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_polycurve.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_polyline.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_polylinecurve.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_precompiledheader.cpp
# ADD CPP /Yc"opennurbs.h"
# End Source File
# Begin Source File

SOURCE=.\opennurbs_revsurface.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_sphere.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_string.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_sum.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_sumsurface.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_surface.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_surfaceproxy.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_textlog.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_torus.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_userdata.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_uuid.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_viewport.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_workspace.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_wstring.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_xform.cpp
# End Source File
# Begin Source File

SOURCE=.\opennurbs_zlib.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\opennurbs.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_3dm.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_3dm_attributes.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_3dm_properties.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_3dm_settings.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_annotation.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_annotation2.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_arc.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_arccurve.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_archive.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_array.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_array_defs.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_bezier.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_bezier_volume.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_bitmap.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_bounding_box.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_brep.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_circle.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_color.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_cone.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_crc.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_curve.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_curveonsurface.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_curveproxy.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_cylinder.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_defines.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_detail.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_dimstyle.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_dll_resource.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_ellipse.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_error.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_evaluate_nurbs.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_extensions.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_font.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_fpoint.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_geometry.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_ginfinity.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_gl.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_group.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_hatch.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_instance.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_intersect.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_knot.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_layer.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_light.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_line.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_linecurve.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_linestyle.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_linetype.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_mapchan.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_massprop.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_material.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_math.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_matrix.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_memory.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_mesh.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_nurbscurve.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_nurbssurface.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_object.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_object_history.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_objref.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_offsetsurface.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_optimize.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_plane.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_planesurface.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_pluginlist.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_point.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_pointcloud.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_pointgeometry.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_pointgrid.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_polycurve.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_polyline.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_polylinecurve.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_rendering.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_revsurface.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_sphere.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_string.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_sumsurface.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_surface.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_surfaceproxy.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_system.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_temp.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_textlog.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_texture.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_texture_mapping.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_torus.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_userdata.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_uuid.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_version.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_viewport.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_workspace.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_x.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_xform.h
# End Source File
# Begin Source File

SOURCE=.\opennurbs_zlib.h
# End Source File
# End Group
# End Target
# End Project
