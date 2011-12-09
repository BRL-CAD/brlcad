# build shared libs by default
IF(NOT DEFINED BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS ON)
ENDIF(NOT DEFINED BUILD_SHARED_LIBS)

# build static libs by default
IF(NOT DEFINED BUILD_STATIC_LIBS)
	set(BUILD_STATIC_LIBS ON)
ENDIF(NOT DEFINED BUILD_STATIC_LIBS)

set(OPENNURBS_SOURCES
    opennurbs_3dm_attributes.cpp
    opennurbs_3dm_properties.cpp
    opennurbs_3dm_settings.cpp
    opennurbs_annotation.cpp
    opennurbs_annotation2.cpp
    opennurbs_arc.cpp
    opennurbs_arccurve.cpp
    opennurbs_archive.cpp
    opennurbs_array.cpp
    opennurbs_base32.cpp
    opennurbs_base64.cpp
    opennurbs_basic.cpp
    opennurbs_beam.cpp
    opennurbs_bezier.cpp
    opennurbs_beziervolume.cpp
    opennurbs_brep_region.cpp
    opennurbs_bitmap.cpp
    opennurbs_box.cpp
    opennurbs_bounding_box.cpp
    opennurbs_brep.cpp
    opennurbs_brep_changesrf.cpp
    opennurbs_brep_extrude.cpp
    opennurbs_brep_io.cpp
    opennurbs_brep_isvalid.cpp
    opennurbs_brep_kinky.cpp
    opennurbs_brep_tools.cpp
    opennurbs_brep_v2valid.cpp
    opennurbs_circle.cpp
    opennurbs_color.cpp
    opennurbs_cone.cpp
    opennurbs_crc.cpp
    opennurbs_curve.cpp
    opennurbs_curveonsurface.cpp
    opennurbs_curveproxy.cpp
    opennurbs_cylinder.cpp
    opennurbs_defines.cpp
    opennurbs_detail.cpp
    opennurbs_dimstyle.cpp
    opennurbs_ellipse.cpp
    opennurbs_error.cpp
    opennurbs_error_message.cpp
    opennurbs_evaluate_nurbs.cpp
    opennurbs_extensions.cpp
    opennurbs_font.cpp
    opennurbs_geometry.cpp
    opennurbs_group.cpp
    opennurbs_hatch.cpp
    opennurbs_instance.cpp
    opennurbs_intersect.cpp
    opennurbs_knot.cpp
    opennurbs_layer.cpp
    opennurbs_light.cpp
    opennurbs_line.cpp
    opennurbs_linecurve.cpp
    opennurbs_linetype.cpp
    opennurbs_lookup.cpp
    opennurbs_massprop.cpp
    opennurbs_material.cpp
    opennurbs_math.cpp
    opennurbs_matrix.cpp
    opennurbs_memory.c
    opennurbs_memory_util.c
    opennurbs_mesh.cpp
    opennurbs_mesh_ngon.cpp
    opennurbs_mesh_tools.cpp
    opennurbs_morph.cpp
    opennurbs_nurbscurve.cpp
    opennurbs_nurbssurface.cpp
    opennurbs_nurbsvolume.cpp
    opennurbs_object.cpp
    opennurbs_object_history.cpp
    opennurbs_objref.cpp
    opennurbs_offsetsurface.cpp
    opennurbs_optimize.cpp
    opennurbs_plane.cpp
    opennurbs_planesurface.cpp
    opennurbs_pluginlist.cpp
    opennurbs_point.cpp
    opennurbs_pointcloud.cpp
    opennurbs_pointgeometry.cpp
    opennurbs_pointgrid.cpp
    opennurbs_polycurve.cpp
    opennurbs_polyedgecurve.cpp
    opennurbs_polyline.cpp
    opennurbs_polylinecurve.cpp
    opennurbs_quaternion.cpp
    opennurbs_rand.cpp
    opennurbs_revsurface.cpp
    opennurbs_rtree.cpp
    opennurbs_sort.cpp
    opennurbs_sphere.cpp
    opennurbs_string.cpp
    opennurbs_sum.cpp
    opennurbs_sumsurface.cpp
    opennurbs_surface.cpp
    opennurbs_surfaceproxy.cpp
    opennurbs_textlog.cpp
    opennurbs_torus.cpp
    opennurbs_userdata.cpp
    opennurbs_uuid.cpp
    opennurbs_viewport.cpp
    opennurbs_workspace.cpp
    opennurbs_wstring.cpp
    opennurbs_x.cpp
    opennurbs_xform.cpp
    opennurbs_zlib.cpp
)

SET(OPENNURBS_HEADERS
	opennurbs.h
	opennurbs_3dm.h
	opennurbs_3dm_attributes.h
	opennurbs_3dm_properties.h
	opennurbs_3dm_settings.h
	opennurbs_annotation.h
	opennurbs_annotation2.h
	opennurbs_arc.h
	opennurbs_arccurve.h
	opennurbs_archive.h
	opennurbs_array.h
	opennurbs_array_defs.h
	opennurbs_base32.h
	opennurbs_base64.h
	opennurbs_beam.h
	opennurbs_bezier.h
	opennurbs_bitmap.h
	opennurbs_bounding_box.h
	opennurbs_box.h
	opennurbs_brep.h
	opennurbs_circle.h
	opennurbs_color.h
	opennurbs_cone.h
	opennurbs_crashtest.h
	opennurbs_crc.h
	opennurbs_curve.h
	opennurbs_curveonsurface.h
	opennurbs_curveproxy.h
	opennurbs_cylinder.h
	opennurbs_defines.h
	opennurbs_detail.h
	opennurbs_dimstyle.h
	opennurbs_ellipse.h
	opennurbs_error.h
	opennurbs_evaluate_nurbs.h
	opennurbs_extensions.h
	opennurbs_font.h
	opennurbs_fpoint.h
	opennurbs_geometry.h
	opennurbs_gl.h
	opennurbs_group.h
	opennurbs_hatch.h
	opennurbs_instance.h
	opennurbs_intersect.h
	opennurbs_knot.h
	opennurbs_layer.h
	opennurbs_light.h
	opennurbs_line.h
	opennurbs_linecurve.h
	opennurbs_linestyle.h
	opennurbs_linetype.h
	opennurbs_lookup.h
	opennurbs_mapchan.h
	opennurbs_massprop.h
	opennurbs_material.h
	opennurbs_math.h
	opennurbs_matrix.h
	opennurbs_memory.h
	opennurbs_mesh.h
	opennurbs_nurbscurve.h
	opennurbs_nurbssurface.h
	opennurbs_object.h
	opennurbs_object_history.h
	opennurbs_objref.h
	opennurbs_offsetsurface.h
	opennurbs_optimize.h
	opennurbs_plane.h
	opennurbs_planesurface.h
	opennurbs_pluginlist.h
	opennurbs_point.h
	opennurbs_pointcloud.h
	opennurbs_pointgeometry.h
	opennurbs_pointgrid.h
	opennurbs_polycurve.h
	opennurbs_polyedgecurve.h
	opennurbs_polyline.h
	opennurbs_polylinecurve.h
	opennurbs_quaternion.h
	opennurbs_rand.h
	opennurbs_rendering.h
	opennurbs_revsurface.h
	opennurbs_rtree.h
	opennurbs_sphere.h
	opennurbs_staticlib_linking_pragmas.h
	opennurbs_string.h
	opennurbs_sumsurface.h
	opennurbs_surface.h
	opennurbs_surfaceproxy.h
	opennurbs_system.h
	opennurbs_textlog.h
	opennurbs_texture.h
	opennurbs_texture_mapping.h
	opennurbs_torus.h
	opennurbs_userdata.h
	opennurbs_uuid.h
	opennurbs_version.h
	opennurbs_viewport.h
	opennurbs_workspace.h
	opennurbs_x.h
	opennurbs_xform.h
	opennurbs_zlib.h
	)

SET(ON_DLL_HEADERS
	opennurbs_dll_resource.h
)

INCLUDE(CheckIncludeFiles)
CHECK_INCLUDE_FILE(emmintrin.h HAVE_EMMINTRIN_H)
CHECK_INCLUDE_FILE(ieeefp.h HAVE_IEEEFP_H)
CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
if(HAVE_EMMINTRIN_H)
	add_definitions(-DHAVE_EMMINTRIN_H)
endif(HAVE_EMMINTRIN_H)
if(HAVE_IEEEFP_H)
	add_definitions(-DHAVE_IEEEFP_H)
endif(HAVE_IEEEFP_H)
if(HAVE_SYS_STAT_H)
	add_definitions(-DHAVE_SYS_STAT_H)
endif(HAVE_SYS_STAT_H)

IF(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY AND ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_ZLIB AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
    set(OPENNURBS_LINKLIBRARIES
	zlib-static
    )
ELSE(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY AND ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_ZLIB AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
    set(OPENNURBS_LINKLIBRARIES
	${ZLIB_LIBRARY}
    )
ENDIF(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY AND ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_ZLIB AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)

# Ideally, we should only need ZLIB here
include_directories(
	${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/include
    ${ZLIB_INCLUDE_DIR}
)

if (MSVC)
   add_definitions(
      -DON_COMPILING_OPENNURBS
       -DON_DLL_EXPORTS
   )
	SET(OPENNURBS_HEADERS "${OPENNURBS_HEADERS};${ON_DLL_HEADERS}")
else (MSVC)
   add_definitions(
       -DON_COMPILING_OPENNURBS
   )
endif(MSVC)


if(MSVC)
    add_definitions(
	-DUNICODE
    )

    set(OPENNURBS_LINKLIBRARIES
	${OPENNURBS_LINKLIBRARIES}
	Rpcrt4
    )
endif(MSVC)

IF(BUILD_SHARED_LIBS)
  add_library(openNURBS SHARED ${OPENNURBS_SOURCES})
  target_link_libraries(openNURBS ${OPENNURBS_LINKLIBRARIES})
  set_property(TARGET openNURBS PROPERTY LINK_INTERFACE_LIBRARIES "")
  SET_TARGET_PROPERTIES(openNURBS PROPERTIES VERSION "2010.04.95")
  install(TARGETS openNURBS DESTINATION ${LIB_DIR})
ENDIF(BUILD_SHARED_LIBS)
IF(NOT MSVC)
   IF(BUILD_STATIC_LIBS)
	add_library(openNURBS-static STATIC ${OPENNURBS_SOURCES})
	target_link_libraries(openNURBS-static ${OPENNURBS_LINKLIBRARIES})
	IF(NOT WIN32)
		SET_TARGET_PROPERTIES(openNURBS-static PROPERTIES OUTPUT_NAME "openNURBS")
	ENDIF(NOT WIN32)
	install(TARGETS openNURBS-static ARCHIVE DESTINATION ${LIB_DIR})
   ENDIF(BUILD_STATIC_LIBS)
ENDIF(NOT MSVC)

install(FILES ${OPENNURBS_HEADERS} DESTINATION include/openNURBS)
