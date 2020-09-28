set(opennurbs_DESCRIPTION "
Option for enabling and disabling compilation of the openNURBS library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(openNURBS OPENNURBS openNURBS opennurbs_DESCRIPTION ALIASES ENABLE_OPENNURBS FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_OPENNURBS_BUILD)

  set(OPENNURBS_MAJOR_VERSION 2012)
  set(OPENNURBS_MINOR_VERSION 10)
  set(OPENNURBS_PATCH_VERSION 245)
  set(OPENNURBS_VERSION ${OPENNURBS_MAJOR_VERSION}.${OPENNURBS_MINOR_VERSION}.${OPENNURBS_PATCH_VERSION})

  if (MSVC)
    set(OPENNURBS_BASENAME openNURBS)
    set(OPENNURBS_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(OPENNURBS_BASENAME libopenNURBS)
    set(OPENNURBS_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${OPENNURBS_VERSION})
  endif (MSVC)

  if (TARGET ZLIB_BLD)
    set(ZLIB_TARGET ZLIB_BLD)
  endif (TARGET ZLIB_BLD)

  ExternalProject_Add(OPENNURBS_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/openNURBS"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DZLIB_ROOT=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}> -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}>
    DEPENDS ${ZLIB_TARGET}
    )
  ExternalProject_Target(openNURBS OPENNURBS_BLD
    OUTPUT_FILE ${OPENNURBS_BASENAME}${OPENNURBS_SUFFIX}
    STATIC_OUTPUT_FILE ${OPENNURBS_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "${OPENNURBS_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${OPENNURBS_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${OPENNURBS_MAJOR_VERSION}"
    LINK_TARGET "${OPENNURBS_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "${OPENNURBS_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    RPATH
    )

  ExternalProject_ByProducts(OPENNURBS_BLD ${INCLUDE_DIR}
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

  list(APPEND BRLCAD_DEPS OPENNURBS_BLD)

endif (${CMAKE_PROJECT_NAME}_OPENNURBS_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

