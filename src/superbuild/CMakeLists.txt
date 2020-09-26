# If targets are built, we want to group them in build tools that support
# doing so.
function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# Debugging flag for ExternalProject tools
set(EXTPROJ_VERBOSE 0)

# Load some CMake macros to handle the special case of third party libraries.
include(ThirdParty)

# Superbuilds use ExternalProject_Add, and BRL-CAD defines some additional
# targets for managing them.
include(ExternalProject)
include(ExternalProject_Target)

set(BRLCAD_DEPS)

###############################################################################
#                 zlib compression/decompression library
###############################################################################

set (zlib_DESCRIPTION "
Option for enabling and disabling compilation of the zlib library
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(libz ZLIB zlib zlib_DESCRIPTION ALIASES ENABLE_ZLIB ENABLE_LIBZ)

if (${CMAKE_PROJECT_NAME}_ZLIB_BUILD)

  set(ZLIB_VERSION 1.2.11)

  set(Z_PREFIX_STR "brl_")
  add_definitions(-DZ_PREFIX)
  add_definitions(-DZ_PREFIX_STR=${Z_PREFIX_STR})
  set(Z_PREFIX_STR "${Z_PREFIX_STR}" CACHE STRING "prefix for zlib functions" FORCE)

  ExternalProject_Add(ZLIB_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libz"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DZ_PREFIX_STR=${Z_PREFIX_STR}
    )
  if (NOT MSVC)
    ExternalProject_Target(zlib ZLIB_BLD
      OUTPUT_FILE libz_brl${CMAKE_SHARED_LIBRARY_SUFFIX}.${ZLIB_VERSION}
      STATIC_OUTPUT_FILE libz${CMAKE_STATIC_LIBRARY_SUFFIX}
      SYMLINKS "libz_brl${CMAKE_SHARED_LIBRARY_SUFFIX};libz_brl${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
      LINK_TARGET "libz_brl${CMAKE_SHARED_LIBRARY_SUFFIX}"
      RPATH
      )
    set(ZLIB_BASENAME libz_brl)
  else (NOT MSVC)
    ExternalProject_Target(zlib ZLIB_BLD
      OUTPUT_FILE zlib${CMAKE_SHARED_LIBRARY_SUFFIX}
      STATIC_OUTPUT_FILE z_brl${CMAKE_STATIC_LIBRARY_SUFFIX}
      )
    set(ZLIB_BASENAME z_brl)
  endif (NOT MSVC)

  ExternalProject_ByProducts(ZLIB_BLD ${INCLUDE_DIR}
    zconf.h
    zlib.h
    )

  list(APPEND BRLCAD_DEPS ZLIB_BLD)

  set(ZLIB_LIBRARIES zlib CACHE STRING "Building bundled zlib" FORCE)
  set(ZLIB_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers." FORCE)

  SetTargetFolder(ZLIB_BLD "Third Party Libraries")
  SetTargetFolder(zlib "Third Party Libraries")

else (${CMAKE_PROJECT_NAME}_ZLIB_BUILD)

  set(Z_PREFIX_STR "" CACHE STRING "clear prefix for zlib functions" FORCE)
  set(Z_PREFIX_STR)
  set(ZLIB_LIBRARIES ${ZLIB_LIBRARY_RELEASE} CACHE STRING "ZLIB_LIBRARIES" FORCE)
  set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}" CACHE STRING "ZLIB include directory" FORCE)

endif (${CMAKE_PROJECT_NAME}_ZLIB_BUILD)

###############################################################################
#           openNURBS - Non-Uniform Rational B-Spline support
###############################################################################

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

  # Because we use a custom name for zlib, we need to conditionally use a
  # generator expression to set its location for FindZLIB.  So far, haven't
  # found a less verbose way to make this work...
  if (TARGET zlib)
    ExternalProject_Add(OPENNURBS_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/openNURBS"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
      -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
      -DZLIB_ROOT=${CMAKE_BINARY_DIR} -DZLIB_LIBRARY=$<TARGET_LINKER_FILE_DIR:zlib>/${ZLIB_BASENAME}$<TARGET_LINKER_FILE_SUFFIX:zlib>
      DEPENDS ${ZLIB_TARGET}
      )
  else (TARGET zlib)
    ExternalProject_Add(OPENNURBS_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/openNURBS"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
      -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
      DEPENDS ${ZLIB_TARGET}
      )
  endif (TARGET zlib)

  ExternalProject_Target(openNURBS OPENNURBS_BLD
    OUTPUT_FILE libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX}.${OPENNURBS_VERSION}
    STATIC_OUTPUT_FILE libopenNURBS${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX};libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX}.${OPENNURBS_MAJOR_VERSION}"
    LINK_TARGET "libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "libopenNURBS${CMAKE_STATIC_LIBRARY_SUFFIX}"
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

###############################################################################
#                    Primary BRL-CAD Compilation
###############################################################################

# We've built everything we needed to build - now we can build BRL-CAD itself
ExternalProject_Add(BRLCAD_BLD
  SOURCE_DIR "${BRLCAD_SOURCE_DIR}"
  PREFIX ${BRLCAD_BINARY_DIR}/brlcad
  BINARY_DIR ${BRLCAD_BINARY_DIR}/brlcad-build
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
  -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
  -DSUBBUILD=ON
  DEPENDS ${BRLCAD_DEPS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

