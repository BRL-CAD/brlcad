cmake_minimum_required(VERSION 2.8)

set(BRLCAD_BUILD_ROOT "$ENV{HOME}/build")
set(BRLCAD_SOURCE_ROOT "$ENV{HOME}/brlcad")
set(OPENNURBS_SOURCE_ROOT "${BRLCAD_SOURCE_ROOT}/src/other/openNURBS")

list(APPEND CMAKE_MODULE_PATH ${BRLCAD_SOURCE_ROOT}/misc/CMake)

find_package(LEMON)
find_package(RE2C)
find_package(PERPLEX)

include(${BRLCAD_SOURCE_ROOT}/misc/CMake/PERPLEX_Util.cmake)
include(${BRLCAD_SOURCE_ROOT}/misc/CMake/LEMON_Util.cmake)
include(${BRLCAD_SOURCE_ROOT}/misc/CMake/RE2C_Util.cmake)

include_directories(
    ${BRLCAD_SOURCE_ROOT}/include
    ${BRLCAD_SOURCE_ROOT}/src/other/tcl/generic
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

link_directories(${BRLCAD_BUILD_ROOT}/lib)

set(ON_sources
examples_linking_pragmas.h
opennurbs_3dm_attributes.cpp
opennurbs_3dm_attributes.h
opennurbs_3dm.h
opennurbs_3dm_properties.cpp
opennurbs_3dm_properties.h
opennurbs_3dm_settings.cpp
opennurbs_3dm_settings.h
opennurbs_annotation2.cpp
opennurbs_annotation2.h
opennurbs_annotation.cpp
opennurbs_annotation.h
opennurbs_arc.cpp
opennurbs_arccurve.cpp
opennurbs_arccurve.h
opennurbs_arc.h
opennurbs_archive.cpp
opennurbs_archive.h
opennurbs_array.cpp
opennurbs_array_defs.h
opennurbs_array.h
opennurbs_base32.cpp
opennurbs_base32.h
opennurbs_base64.cpp
opennurbs_base64.h
opennurbs_basic.cpp
opennurbs_beam.cpp
opennurbs_beam.h
opennurbs_bezier.cpp
opennurbs_bezier.h
opennurbs_beziervolume.cpp
opennurbs_bitmap.cpp
opennurbs_bitmap.h
opennurbs_bounding_box.cpp
opennurbs_bounding_box.h
opennurbs_box.cpp
opennurbs_box.h
opennurbs_brep_changesrf.cpp
opennurbs_brep.cpp
opennurbs_brep_extrude.cpp
opennurbs_brep.h
opennurbs_brep_io.cpp
opennurbs_brep_isvalid.cpp
opennurbs_brep_kinky.cpp
opennurbs_brep_region.cpp
opennurbs_brep_tools.cpp
opennurbs_brep_v2valid.cpp
opennurbs_circle.cpp
opennurbs_circle.h
opennurbs_color.cpp
opennurbs_color.h
opennurbs_compress.cpp
opennurbs_compress.h
opennurbs_cone.cpp
opennurbs_cone.h
opennurbs_crc.cpp
opennurbs_crc.h
opennurbs_curve.cpp
opennurbs_curve.h
opennurbs_curveonsurface.cpp
opennurbs_curveonsurface.h
opennurbs_curveproxy.cpp
opennurbs_curveproxy.h
opennurbs_cylinder.cpp
opennurbs_cylinder.h
opennurbs_defines.cpp
opennurbs_defines.h
opennurbs_detail.cpp
opennurbs_detail.h
opennurbs_dimstyle.cpp
opennurbs_dimstyle.h
opennurbs_dll.cpp
opennurbs_dll_resource.h
opennurbs_ellipse.cpp
opennurbs_ellipse.h
opennurbs_embedded_file.cpp
opennurbs_error.cpp
opennurbs_error.h
opennurbs_error_message.cpp
opennurbs_evaluate_nurbs.cpp
opennurbs_evaluate_nurbs.h
opennurbs_extensions.cpp
opennurbs_extensions.h
opennurbs_font.cpp
opennurbs_font.h
opennurbs_fpoint.h
opennurbs_fsp.cpp
opennurbs_fsp_defs.h
opennurbs_fsp.h
opennurbs_geometry.cpp
opennurbs_geometry.h
opennurbs_gl.cpp
opennurbs_gl.h
opennurbs_group.cpp
opennurbs_group.h
opennurbs.h
opennurbs_hatch.cpp
opennurbs_hatch.h
opennurbs_hsort_template.h
# this one has commented-out doc comments that the converter chokes on
#opennurbs_instance.cpp
opennurbs_instance.h
opennurbs_intersect.cpp
opennurbs_intersect.h
opennurbs_knot.cpp
opennurbs_knot.h
opennurbs_layer.cpp
opennurbs_layer.h
opennurbs_light.cpp
opennurbs_light.h
opennurbs_line.cpp
opennurbs_linecurve.cpp
opennurbs_linecurve.h
opennurbs_line.h
opennurbs_linestyle.h
opennurbs_linetype.cpp
opennurbs_linetype.h
opennurbs_lookup.cpp
opennurbs_lookup.h
opennurbs_mapchan.h
opennurbs_massprop.cpp
opennurbs_massprop.h
opennurbs_material.cpp
opennurbs_material.h
opennurbs_math.cpp
opennurbs_math.h
opennurbs_matrix.cpp
opennurbs_matrix.h
opennurbs_memory.h
opennurbs_mesh.cpp
opennurbs_mesh.h
opennurbs_mesh_ngon.cpp
opennurbs_mesh_tools.cpp
opennurbs_morph.cpp
opennurbs_nurbscurve.cpp
opennurbs_nurbscurve.h
opennurbs_nurbssurface.cpp
opennurbs_nurbssurface.h
opennurbs_nurbsvolume.cpp
opennurbs_object.cpp
opennurbs_object.h
opennurbs_object_history.cpp
opennurbs_object_history.h
opennurbs_objref.cpp
opennurbs_objref.h
opennurbs_offsetsurface.cpp
opennurbs_offsetsurface.h
opennurbs_optimize.cpp
opennurbs_optimize.h
opennurbs_plane.cpp
opennurbs_plane.h
opennurbs_planesurface.cpp
opennurbs_planesurface.h
opennurbs_pluginlist.cpp
opennurbs_pluginlist.h
opennurbs_pointcloud.cpp
opennurbs_pointcloud.h
opennurbs_point.cpp
opennurbs_pointgeometry.cpp
opennurbs_pointgeometry.h
opennurbs_pointgrid.cpp
opennurbs_pointgrid.h
opennurbs_point.h
opennurbs_polycurve.cpp
opennurbs_polycurve.h
opennurbs_polyedgecurve.cpp
opennurbs_polyedgecurve.h
opennurbs_polyline.cpp
opennurbs_polylinecurve.cpp
opennurbs_polylinecurve.h
opennurbs_polyline.h
opennurbs_precompiledheader.cpp
opennurbs_qsort_template.h
opennurbs_rand.cpp
opennurbs_rand.h
opennurbs_rendering.h
opennurbs_revsurface.cpp
opennurbs_revsurface.h
opennurbs_rtree.cpp
opennurbs_rtree.h
opennurbs_sort.cpp
opennurbs_sphere.cpp
opennurbs_sphere.h
opennurbs_string.cpp
opennurbs_string.h
opennurbs_sum.cpp
opennurbs_sumsurface.cpp
opennurbs_sumsurface.h
opennurbs_surface.cpp
opennurbs_surface.h
opennurbs_surfaceproxy.cpp
opennurbs_surfaceproxy.h
opennurbs_system.h
opennurbs_textlog.cpp
opennurbs_textlog.h
opennurbs_texture.h
opennurbs_texture_mapping.h
opennurbs_torus.cpp
opennurbs_torus.h
opennurbs_unicode.cpp
opennurbs_unicode.h
opennurbs_userdata.cpp
opennurbs_userdata.h
opennurbs_uuid.cpp
opennurbs_uuid.h
opennurbs_version.h
opennurbs_viewport.cpp
opennurbs_viewport.h
opennurbs_workspace.cpp
opennurbs_workspace.h
opennurbs_wstring.cpp
opennurbs_x.cpp
opennurbs_xform.cpp
opennurbs_xform.h
opennurbs_x.h
opennurbs_zlib.cpp
opennurbs_zlib.h
opennurbs_zlib_memory.cpp
)

set(DOC_CONVERTER dom2dox)
set(CONVERSION_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(DOC_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc")

PERPLEX_TARGET(${DOC_CONVERTER}_scanner ${DOC_CONVERTER}_scanner.perplex
    ${CMAKE_CURRENT_BINARY_DIR}/${DOC_CONVERTER}_scanner.c
    ${CMAKE_CURRENT_BINARY_DIR}/${DOC_CONVERTER}_scanner.h)
LEMON_TARGET(${DOC_CONVERTER}_parser ${DOC_CONVERTER}_parser.lemon
    ${CMAKE_CURRENT_BINARY_DIR}/${DOC_CONVERTER}_parser.c
    ${CMAKE_CURRENT_BINARY_DIR}/${DOC_CONVERTER}_parser.h)
ADD_PERPLEX_LEMON_DEPENDENCY(${DOC_CONVERTER}_scanner ${DOC_CONVERTER}_parser)

add_executable(${DOC_CONVERTER} ${DOC_CONVERTER}.c ${DOC_CONVERTER}_scanner.c ${DOC_CONVERTER}_parser.c)
target_link_libraries(${DOC_CONVERTER} bu)

foreach(ON_source ${ON_sources})
    add_custom_command(OUTPUT "${ON_source}" DEPENDS ${DOC_CONVERTER}
	COMMAND cmake ARGS -E make_directory "${CONVERSION_DIR}"
	COMMAND ${DOC_CONVERTER} ARGS "${OPENNURBS_SOURCE_ROOT}/${ON_source}" "${CONVERSION_DIR}/${ON_source}")
endforeach(ON_source)

add_custom_target(ON_DOC ALL DEPENDS ${ON_sources}
    COMMAND cmake -E make_directory "${DOC_OUTPUT_DIR}"
    COMMAND cmake -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile" "${DOC_OUTPUT_DIR}/Doxyfile"
    COMMAND cmake -E chdir "${DOC_OUTPUT_DIR}" doxygen Doxyfile
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
