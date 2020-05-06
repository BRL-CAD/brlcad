# Include directories needed by libbrep users
set(BREP_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/openNURBS
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${P2T_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIRS}
  )

# locally used but not needed by users of the library
set(BREP_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REGEX_INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/src/other/Eigen
  )

BRLCAD_LIB_INCLUDE_DIRS(brep BREP_INCLUDE_DIRS BREP_LOCAL_INCLUDE_DIRS)

#############################################################################
# BRL-CAD's copy of openNURBS diverges somewhat from upstream - we build our
# version directly into libbrep to ensure BRL-CAD has the expected
# functionality.
#############################################################################

# these are files no longer in upstream openNURBS but retained for reference
set(OPENNURBS_REF
  opennurbs_basic.cpp
  opennurbs_brep_changesrf.cpp
  opennurbs_brep_kinky.cpp
  opennurbs_x.cpp
  )

set(OPENNURBS_C_SOURCES
  openNURBS/opennurbs_memory.c
  openNURBS/opennurbs_memory_util.c
  )

set(OPENNURBS_CXX_SOURCES
  openNURBS/opennurbs_3dm_attributes.cpp
  openNURBS/opennurbs_3dm_properties.cpp
  openNURBS/opennurbs_3dm_settings.cpp
  openNURBS/opennurbs_annotation.cpp
  openNURBS/opennurbs_annotation2.cpp
  openNURBS/opennurbs_arc.cpp
  openNURBS/opennurbs_arccurve.cpp
  openNURBS/opennurbs_archive.cpp
  openNURBS/opennurbs_array.cpp
  openNURBS/opennurbs_base32.cpp
  openNURBS/opennurbs_base64.cpp
  openNURBS/opennurbs_beam.cpp
  openNURBS/opennurbs_bezier.cpp
  openNURBS/opennurbs_beziervolume.cpp
  openNURBS/opennurbs_bitmap.cpp
  openNURBS/opennurbs_bounding_box.cpp
  openNURBS/opennurbs_box.cpp
  openNURBS/opennurbs_brep.cpp
  openNURBS/opennurbs_brep_extrude.cpp
  openNURBS/opennurbs_brep_io.cpp
  openNURBS/opennurbs_brep_isvalid.cpp
  openNURBS/opennurbs_brep_region.cpp
  openNURBS/opennurbs_brep_tools.cpp
  openNURBS/opennurbs_brep_v2valid.cpp
  openNURBS/opennurbs_circle.cpp
  openNURBS/opennurbs_color.cpp
  openNURBS/opennurbs_compress.cpp
  openNURBS/opennurbs_cone.cpp
  openNURBS/opennurbs_crc.cpp
  openNURBS/opennurbs_curve.cpp
  openNURBS/opennurbs_curveonsurface.cpp
  openNURBS/opennurbs_curveproxy.cpp
  openNURBS/opennurbs_cylinder.cpp
  openNURBS/opennurbs_defines.cpp
  openNURBS/opennurbs_detail.cpp
  openNURBS/opennurbs_dimstyle.cpp
  openNURBS/opennurbs_dll.cpp
  openNURBS/opennurbs_ellipse.cpp
  openNURBS/opennurbs_embedded_file.cpp
  openNURBS/opennurbs_error.cpp
  openNURBS/opennurbs_error_message.cpp
  openNURBS/opennurbs_evaluate_nurbs.cpp
  openNURBS/opennurbs_extensions.cpp
  openNURBS/opennurbs_font.cpp
  openNURBS/opennurbs_fsp.cpp
  openNURBS/opennurbs_geometry.cpp
  openNURBS/opennurbs_group.cpp
  openNURBS/opennurbs_hatch.cpp
  openNURBS/opennurbs_instance.cpp
  openNURBS/opennurbs_intersect.cpp
  openNURBS/opennurbs_knot.cpp
  openNURBS/opennurbs_layer.cpp
  openNURBS/opennurbs_light.cpp
  openNURBS/opennurbs_line.cpp
  openNURBS/opennurbs_linecurve.cpp
  openNURBS/opennurbs_linetype.cpp
  openNURBS/opennurbs_lookup.cpp
  openNURBS/opennurbs_material.cpp
  openNURBS/opennurbs_math.cpp
  openNURBS/opennurbs_massprop.cpp
  openNURBS/opennurbs_matrix.cpp
  openNURBS/opennurbs_mesh.cpp
  openNURBS/opennurbs_mesh_ngon.cpp
  openNURBS/opennurbs_mesh_tools.cpp
  openNURBS/opennurbs_morph.cpp
  openNURBS/opennurbs_nurbscurve.cpp
  openNURBS/opennurbs_nurbssurface.cpp
  openNURBS/opennurbs_nurbsvolume.cpp
  openNURBS/opennurbs_object.cpp
  openNURBS/opennurbs_object_history.cpp
  openNURBS/opennurbs_objref.cpp
  openNURBS/opennurbs_offsetsurface.cpp
  openNURBS/opennurbs_optimize.cpp
  openNURBS/opennurbs_plane.cpp
  openNURBS/opennurbs_planesurface.cpp
  openNURBS/opennurbs_pluginlist.cpp
  openNURBS/opennurbs_point.cpp
  openNURBS/opennurbs_pointcloud.cpp
  openNURBS/opennurbs_pointgeometry.cpp
  openNURBS/opennurbs_pointgrid.cpp
  openNURBS/opennurbs_polycurve.cpp
  openNURBS/opennurbs_polyedgecurve.cpp
  openNURBS/opennurbs_polyline.cpp
  openNURBS/opennurbs_polylinecurve.cpp
  openNURBS/opennurbs_rand.cpp
  openNURBS/opennurbs_revsurface.cpp
  openNURBS/opennurbs_rtree.cpp
  openNURBS/opennurbs_sort.cpp
  openNURBS/opennurbs_sphere.cpp
  openNURBS/opennurbs_string.cpp
  openNURBS/opennurbs_sum.cpp
  openNURBS/opennurbs_sumsurface.cpp
  openNURBS/opennurbs_surface.cpp
  openNURBS/opennurbs_surfaceproxy.cpp
  openNURBS/opennurbs_textlog.cpp
  openNURBS/opennurbs_torus.cpp
  openNURBS/opennurbs_unicode.cpp
  openNURBS/opennurbs_userdata.cpp
  openNURBS/opennurbs_uuid.cpp
  openNURBS/opennurbs_viewport.cpp
  openNURBS/opennurbs_workspace.cpp
  openNURBS/opennurbs_wstring.cpp
  openNURBS/opennurbs_xform.cpp
  openNURBS/opennurbs_zlib.cpp
  )

# The openNURBS sources don't tolerate a number of our warning flags
set(ON_FLAGS
  no-class-memaccess
  no-deprecated-declarations
  no-extra
  no-float-equal
  no-implicit-fallthrough
  no-misleading-indentation
  no-shadow
  no-unused-but-set-parameter
  no-unused-but-set-variable
  no-unused-parameter
  )
set(A_ON_FLAGS)
foreach(onflag ${ON_FLAGS})
  string(TOUPPER "${onflag}" UONFLAG)
  string(REPLACE "-" "_" HFLAG ${UONFLAG})
  CHECK_CXX_COMPILER_FLAG("-W${onflag}" ${HFLAG})
  if (${HFLAG})
    set(A_ON_FLAGS ${A_ON_FLAGS} ${onflag})
  endif (${HFLAG})
endforeach(onflag ${ON_FLAGS})

foreach(onfile ${OPENNURBS_CXX_SOURCES})
  foreach(onflag ${A_ON_FLAGS})
    set_property(SOURCE ${onfile} APPEND_STRING PROPERTY COMPILE_FLAGS " -W${onflag}")
  endforeach(onflag ${A_ON_FLAGS})
endforeach(onfile ${OPENNURBS_CXX_SOURCES})


set(LIBBREP_SOURCES
  ${OPENNURBS_REF}
  ${OPENNURBS_C_SOURCES}
  ${OPENNURBS_CXX_SOURCES}
  BBNode.cpp
  BRNode.cpp
  PullbackCurve.cpp
  Subcurve.cpp
  Subsurface.cpp
  boolean.cpp
  cdt/cdt.cpp
  cdt/fast.cpp
  cdt/surf.cpp
  cdt/util.cpp
  cdt/validate.cpp
  cdt/edge.cpp
  cdt/mesh.cpp
  cdt/ovlps_simple.cpp
  cdt/tri_isect.cpp
  intersect.cpp
  tools/tools.cpp
  opennurbs_ext.cpp
  px_event.cpp
  shape_recognition/shape_recognition.cpp
  shape_recognition/cone.cpp
  shape_recognition/cylinder.cpp
  shape_recognition/pipeline.cpp
  shape_recognition/planar.cpp
  shape_recognition/sphere.cpp
  shape_recognition/util.cpp
  ssx_event.cpp
  tools/util.cpp
  #opennurbs_fit.cpp
  )

set(CDT2_srcs
  cdt2/cdt.cpp
  cdt2/vert.cpp
  cdt2/edge.cpp
  cdt2/mesh.cpp
  cdt2/polygon.cpp
  )
if (ENABLE_CDT2)
  set(LIBBREP_SOURCES ${LIBBREP_SOURCES} ${CDT2_srcs})
endif (ENABLE_CDT2)

set(libbrep_ignored_files
  tools/tools.h
  brep_except.h
  cdt/cdt.h
  cdt/mesh.h
  cdt/omesh.cpp
  cdt/overt.cpp
  cdt/ovlps.cpp
  cdt/ovlps_grps.cpp
  cdt/RTree.h
  opennurbs_fit.h
  opennurbs_fit.cpp
  PullbackCurve.h
  shape_recognition/shape_recognition.h
  shape_recognition/torus.cpp
  surface_tree_queue_tests.patch
  ${CDT2_srcs}
  cdt2/cdt.h
  )
CMAKEFILES(${libbrep_ignored_files})

# Anyone who needs to import libbrep will also need to import openNURBS
if(CPP_DLL_DEFINES)
  set_property(GLOBAL APPEND PROPERTY libbrep_DLL_DEFINES "ON_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)

BRLCAD_ADDLIB(libbrep "${LIBBREP_SOURCES}" "libbg;libbn;libbu;${OPENNURBS_LIBRARIES};${P2T_LIBRARY};${ZLIB_LIBRARIES};${WINSOCK_LIB};${RPCRT_LIB};${STDCXX_LIBRARIES}")

set_target_properties(libbrep PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

set(OPENNURBS_HEADERS
  openNURBS/opennurbs.h
  openNURBS/opennurbs_3dm.h
  openNURBS/opennurbs_3dm_attributes.h
  openNURBS/opennurbs_3dm_properties.h
  openNURBS/opennurbs_3dm_settings.h
  openNURBS/opennurbs_annotation.h
  openNURBS/opennurbs_annotation2.h
  openNURBS/opennurbs_arc.h
  openNURBS/opennurbs_arccurve.h
  openNURBS/opennurbs_archive.h
  openNURBS/opennurbs_array.h
  openNURBS/opennurbs_array_defs.h
  openNURBS/opennurbs_base32.h
  openNURBS/opennurbs_base64.h
  openNURBS/opennurbs_beam.h
  openNURBS/opennurbs_bezier.h
  openNURBS/opennurbs_bitmap.h
  openNURBS/opennurbs_bounding_box.h
  openNURBS/opennurbs_box.h
  openNURBS/opennurbs_brep.h
  openNURBS/opennurbs_circle.h
  openNURBS/opennurbs_color.h
  openNURBS/opennurbs_compress.h
  openNURBS/opennurbs_cone.h
  openNURBS/opennurbs_crc.h
  openNURBS/opennurbs_curve.h
  openNURBS/opennurbs_curveonsurface.h
  openNURBS/opennurbs_curveproxy.h
  openNURBS/opennurbs_cylinder.h
  openNURBS/opennurbs_defines.h
  openNURBS/opennurbs_detail.h
  openNURBS/opennurbs_dimstyle.h
  openNURBS/opennurbs_dll_resource.h
  openNURBS/opennurbs_ellipse.h
  openNURBS/opennurbs_error.h
  openNURBS/opennurbs_evaluate_nurbs.h
  openNURBS/opennurbs_extensions.h
  openNURBS/opennurbs_font.h
  openNURBS/opennurbs_fpoint.h
  openNURBS/opennurbs_fsp.h
  openNURBS/opennurbs_fsp_defs.h
  openNURBS/opennurbs_geometry.h
  openNURBS/opennurbs_group.h
  openNURBS/opennurbs_hatch.h
  openNURBS/opennurbs_hsort_template.h
  openNURBS/opennurbs_instance.h
  openNURBS/opennurbs_intersect.h
  openNURBS/opennurbs_knot.h
  openNURBS/opennurbs_layer.h
  openNURBS/opennurbs_light.h
  openNURBS/opennurbs_line.h
  openNURBS/opennurbs_linecurve.h
  openNURBS/opennurbs_linestyle.h
  openNURBS/opennurbs_linetype.h
  openNURBS/opennurbs_lookup.h
  openNURBS/opennurbs_mapchan.h
  openNURBS/opennurbs_massprop.h
  openNURBS/opennurbs_material.h
  openNURBS/opennurbs_math.h
  openNURBS/opennurbs_matrix.h
  openNURBS/opennurbs_memory.h
  openNURBS/opennurbs_mesh.h
  openNURBS/opennurbs_nurbscurve.h
  openNURBS/opennurbs_nurbssurface.h
  openNURBS/opennurbs_object.h
  openNURBS/opennurbs_object_history.h
  openNURBS/opennurbs_objref.h
  openNURBS/opennurbs_offsetsurface.h
  openNURBS/opennurbs_optimize.h
  openNURBS/opennurbs_plane.h
  openNURBS/opennurbs_planesurface.h
  openNURBS/opennurbs_pluginlist.h
  openNURBS/opennurbs_point.h
  openNURBS/opennurbs_pointcloud.h
  openNURBS/opennurbs_pointgeometry.h
  openNURBS/opennurbs_pointgrid.h
  openNURBS/opennurbs_polycurve.h
  openNURBS/opennurbs_polyedgecurve.h
  openNURBS/opennurbs_polyline.h
  openNURBS/opennurbs_polylinecurve.h
  openNURBS/opennurbs_qsort_template.h
  openNURBS/opennurbs_rand.h
  openNURBS/opennurbs_rendering.h
  openNURBS/opennurbs_revsurface.h
  openNURBS/opennurbs_rtree.h
  openNURBS/opennurbs_sphere.h
  openNURBS/opennurbs_string.h
  openNURBS/opennurbs_sumsurface.h
  openNURBS/opennurbs_surface.h
  openNURBS/opennurbs_surfaceproxy.h
  openNURBS/opennurbs_system.h
  openNURBS/opennurbs_textlog.h
  openNURBS/opennurbs_texture.h
  openNURBS/opennurbs_texture_mapping.h
  openNURBS/opennurbs_torus.h
  openNURBS/opennurbs_unicode.h
  openNURBS/opennurbs_userdata.h
  openNURBS/opennurbs_uuid.h
  openNURBS/opennurbs_version.h
  openNURBS/opennurbs_viewport.h
  openNURBS/opennurbs_workspace.h
  openNURBS/opennurbs_x.h
  openNURBS/opennurbs_xform.h
  openNURBS/opennurbs_zlib.h
  )

BRLCAD_MANAGE_FILES(OPENNURBS_HEADERS ${INCLUDE_DIR}/brlcad/openNURBS)

# these are files we turn off because we don't use them and they incur some
# cost to enable (e.g., they require OpenGL)
set(OPENNURBS_OFF
  openNURBS/opennurbs_gl.cpp
  openNURBS/opennurbs_gl.h
  openNURBS/opennurbs_precompiledheader.cpp
  openNURBS/opennurbs_zlib_memory.cpp
  )

CMAKEFILES(
  CMakeLists.txt
  ${OPENNURBS_OFF}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
