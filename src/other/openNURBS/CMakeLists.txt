# Note: License below is for CMake build logic of openNURBS.  For license of
# primary openNURBS source code, see license.txt file
#
# Copyright (c) 2010-2016 United States Government as represented by the U.S.
# Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. The name of the author may not be used to endorse or promote products
# derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

project(OPENNURBS)

# Latest OpenNURBS requires C++14
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate position independent code by default (i.e. -fPIC)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

# If we can, use OBJECT libraries to build both shared and static
# libraries from the same .o files, saving double compilation
include(CMakeDependentOption)
cmake_dependent_option(USE_OBJECT_LIBS "Use OBJECT libraries" ON "NOT MSVC" OFF)

set(BRLCAD_DEBUG ON)
if(BRLCAD_DEBUG)
  add_definitions(${DEBUG_CXX_FLAGS})
endif(BRLCAD_DEBUG)

# build shared libs by default
if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif(NOT DEFINED BUILD_SHARED_LIBS)

# build static libs by default
if(NOT DEFINED BUILD_STATIC_LIBS)
  set(BUILD_STATIC_LIBS ON)
endif(NOT DEFINED BUILD_STATIC_LIBS)

# Define destinations if not already set
if(NOT DEFINED BIN_DIR)
  set(BIN_DIR bin)
endif(NOT DEFINED BIN_DIR)
if(NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif(NOT DEFINED LIB_DIR)

# Passed to OpenNURBS build to turn on/off any extentions/modifications to the base code
# specific to BRLCAD. Initially used for unwrapping UV points in a closed surface to
# allowing for trims that extend over a closed seam.
# It's an expert's setting at the moment.
option(BRLCAD_ENABLE_OPENNURBS_EXTENSIONS "Enables BRLCAD related extensions to OpenNURBS" ON)
mark_as_advanced(BRLCAD_ENABLE_OPENNURBS_EXTENSIONS)

# these are files no longer in upstream openNURBS but retained for reference
set(OPENNURBS_REF
  #opennurbs_basic.cpp
  #opennurbs_brep_changesrf.cpp
  #opennurbs_brep_kinky.cpp
  opennurbs_x.cpp
  )

# these are files we turn off because we don't use them and they incur some
# cost to enable (e.g., they require OpenGL)
set(OPENNURBS_OFF
  opennurbs_gl.cpp
  opennurbs_gl.h
  opennurbs_precompiledheader.cpp
  opennurbs_zlib_memory.cpp
  )

set(OPENNURBS_SOURCES
	${OPENNURBS_REF}
	opennurbs_3dm_attributes.cpp
	opennurbs_3dm_properties.cpp
	opennurbs_3dm_settings.cpp
	opennurbs_annotationbase.cpp
	opennurbs_apple_nsfont.cpp
	opennurbs_arc.cpp
	opennurbs_arccurve.cpp
	opennurbs_archive.cpp
	opennurbs_archive_manifest.cpp
	opennurbs_array.cpp
	opennurbs_base32.cpp
	opennurbs_base64.cpp
	opennurbs_beam.cpp
	opennurbs_bezier.cpp
	opennurbs_beziervolume.cpp
	opennurbs_bitmap.cpp
	opennurbs_bounding_box.cpp
	opennurbs_box.cpp
	opennurbs_brep.cpp
	opennurbs_brep_extrude.cpp
	opennurbs_brep_io.cpp
	opennurbs_brep_isvalid.cpp
	opennurbs_brep_region.cpp
	opennurbs_brep_tools.cpp
	opennurbs_brep_v2valid.cpp
	opennurbs_calculator.cpp
	opennurbs_circle.cpp
	opennurbs_color.cpp
	opennurbs_compress.cpp
	opennurbs_compstat.cpp
	opennurbs_cone.cpp
	opennurbs_convex_poly.cpp
	opennurbs_crc.cpp
	opennurbs_curve.cpp
	opennurbs_curveonsurface.cpp
	opennurbs_curveproxy.cpp
	opennurbs_cylinder.cpp
	opennurbs_date.cpp
	opennurbs_defines.cpp
	opennurbs_detail.cpp
	opennurbs_dimension.cpp
	opennurbs_dimensionformat.cpp
	opennurbs_dimensionstyle.cpp
	opennurbs_dll.cpp
	opennurbs_ellipse.cpp
	opennurbs_embedded_file.cpp
	opennurbs_error.cpp
	opennurbs_error_message.cpp
	opennurbs_evaluate_nurbs.cpp
	opennurbs_extensions.cpp
	opennurbs_file_utilities.cpp
	opennurbs_font.cpp
	opennurbs_freetype.cpp
	opennurbs_fsp.cpp
	opennurbs_function_list.cpp
	opennurbs_geometry.cpp
	opennurbs_glyph_outline.cpp
	opennurbs_group.cpp
	opennurbs_hash_table.cpp
	opennurbs_hatch.cpp
	opennurbs_instance.cpp
	opennurbs_internal_V2_annotation.cpp
	opennurbs_internal_V5_annotation.cpp
	opennurbs_internal_V5_dimstyle.cpp
	opennurbs_internal_Vx_annotation.cpp
	opennurbs_intersect.cpp
	opennurbs_ipoint.cpp
	opennurbs_knot.cpp
	opennurbs_layer.cpp
	opennurbs_leader.cpp
	opennurbs_light.cpp
	opennurbs_line.cpp
	opennurbs_linecurve.cpp
	opennurbs_linetype.cpp
	opennurbs_locale.cpp
	opennurbs_lock.cpp
	opennurbs_lookup.cpp
	opennurbs_material.cpp
	opennurbs_math.cpp
	opennurbs_matrix.cpp
	opennurbs_md5.cpp
	opennurbs_memory_util.cpp
	opennurbs_mesh.cpp
	opennurbs_mesh_ngon.cpp
	opennurbs_mesh_tools.cpp
	opennurbs_mesh_topology.cpp
	opennurbs_model_component.cpp
	opennurbs_model_geometry.cpp
	opennurbs_morph.cpp
	opennurbs_nurbscurve.cpp
	opennurbs_nurbssurface.cpp
	opennurbs_nurbsvolume.cpp
	opennurbs_object.cpp
	opennurbs_object_history.cpp
	opennurbs_objref.cpp
	opennurbs_offsetsurface.cpp
	opennurbs_optimize.cpp
	opennurbs_parse_angle.cpp
	opennurbs_parse_length.cpp
	opennurbs_parse_number.cpp
	opennurbs_parse_point.cpp
	opennurbs_parse_settings.cpp
	opennurbs_photogrammetry.cpp
	opennurbs_plane.cpp
	opennurbs_planesurface.cpp
	opennurbs_pluginlist.cpp
	opennurbs_pointcloud.cpp
	opennurbs_point.cpp
	opennurbs_pointgeometry.cpp
	opennurbs_pointgrid.cpp
	opennurbs_polycurve.cpp
	opennurbs_polyedgecurve.cpp
	opennurbs_polyline.cpp
	opennurbs_polylinecurve.cpp
	opennurbs_progress_reporter.cpp
	opennurbs_public_memory.cpp
	opennurbs_quaternion.cpp
	opennurbs_rand.cpp
	opennurbs_revsurface.cpp
	opennurbs_rtree.cpp
	opennurbs_sha1.cpp
	opennurbs_sleeplock.cpp
	opennurbs_sort.cpp
	opennurbs_sphere.cpp
	opennurbs_statics.cpp
	opennurbs_std_string_format.cpp
	opennurbs_std_string_utf.cpp
	opennurbs_string_compare.cpp
	opennurbs_string.cpp
	opennurbs_string_format.cpp
	opennurbs_string_scan.cpp
	opennurbs_string_values.cpp
	opennurbs_subd_archive.cpp
	opennurbs_subd_copy.cpp
	opennurbs_subd.cpp
	opennurbs_subd_data.cpp
	opennurbs_subd_eval.cpp
	opennurbs_subd_fragment.cpp
	opennurbs_subd_frommesh.cpp
	opennurbs_subd_heap.cpp
	opennurbs_subd_iter.cpp
	opennurbs_subd_limit.cpp
	opennurbs_subd_matrix.cpp
	opennurbs_subd_mesh.cpp
	opennurbs_subd_ref.cpp
	opennurbs_subd_ring.cpp
	opennurbs_subd_sector.cpp
	opennurbs_subd_texture.cpp
	opennurbs_sum.cpp
	opennurbs_sumsurface.cpp
	opennurbs_surface.cpp
	opennurbs_surfaceproxy.cpp
	opennurbs_symmetry.cpp
	opennurbs_terminator.cpp
	opennurbs_testclass.cpp
	opennurbs_textcontext.cpp
	opennurbs_text.cpp
	opennurbs_textdraw.cpp
	opennurbs_textglyph.cpp
	opennurbs_textiterator.cpp
	opennurbs_textlog.cpp
	opennurbs_textobject.cpp
	opennurbs_textrun.cpp
	opennurbs_text_style.cpp
	opennurbs_topology.cpp
	opennurbs_torus.cpp
	#opennurbs_unicode_cp932.cpp
	#opennurbs_unicode_cp949.cpp
	opennurbs_unicode.cpp
	opennurbs_unicode_cpsb.cpp
	opennurbs_units.cpp
	opennurbs_userdata.cpp
	opennurbs_userdata_obsolete.cpp
	opennurbs_uuid.cpp
	opennurbs_version.cpp
	opennurbs_version_number.cpp
	opennurbs_viewport.cpp
	opennurbs_win_dwrite.cpp
	opennurbs_workspace.cpp
	opennurbs_wstring.cpp
	opennurbs_xform.cpp
	opennurbs_zlib.cpp
	)

set(OPENNURBS_HEADERS
	opennurbs_3dm_attributes.h
	opennurbs_3dm.h
	opennurbs_3dm_properties.h
	opennurbs_3dm_settings.h
	opennurbs_annotationbase.h
	opennurbs_apple_nsfont.h
	opennurbs_arccurve.h
	opennurbs_arc.h
	opennurbs_archive.h
	opennurbs_array_defs.h
	opennurbs_array.h
	opennurbs_atomic_op.h
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
	opennurbs_compress.h
	opennurbs_compstat.h
	opennurbs_cone.h
	opennurbs_convex_poly.h
	opennurbs_cpp_base.h
	opennurbs_crc.h
	opennurbs_curve.h
	opennurbs_curveonsurface.h
	opennurbs_curveproxy.h
	opennurbs_cylinder.h
	opennurbs_date.h
	opennurbs_defines.h
	opennurbs_detail.h
	opennurbs_dimensionformat.h
	opennurbs_dimension.h
	opennurbs_dimensionstyle.h
	opennurbs_dll_resource.h
	opennurbs_ellipse.h
	opennurbs_error.h
	opennurbs_evaluate_nurbs.h
	opennurbs_extensions.h
	opennurbs_file_utilities.h
	opennurbs_font.h
	opennurbs_fpoint.h
	opennurbs_freetype.h
	opennurbs_freetype_include.h
	opennurbs_fsp_defs.h
	opennurbs_fsp.h
	opennurbs_function_list.h
	opennurbs_geometry.h
	opennurbs_gl.h
	opennurbs_group.h
	opennurbs.h
	opennurbs_hash_table.h
	opennurbs_hatch.h
	opennurbs_hsort_template.h
	opennurbs_input_libsdir.h
	opennurbs_instance.h
	opennurbs_internal_defines.h
	opennurbs_internal_glyph.h
	opennurbs_internal_unicode_cp.h
	opennurbs_internal_V2_annotation.h
	opennurbs_internal_V5_annotation.h
	opennurbs_internal_V5_dimstyle.h
	opennurbs_intersect.h
	opennurbs_ipoint.h
	opennurbs_knot.h
	opennurbs_layer.h
	opennurbs_leader.h
	opennurbs_light.h
	opennurbs_linecurve.h
	opennurbs_line.h
	opennurbs_linestyle.h
	opennurbs_linetype.h
	opennurbs_locale.h
	opennurbs_lock.h
	opennurbs_lookup.h
	opennurbs_mapchan.h
	opennurbs_material.h
	opennurbs_math.h
	opennurbs_matrix.h
	opennurbs_md5.h
	opennurbs_memory.h
	opennurbs_mesh.h
	opennurbs_model_component.h
	opennurbs_model_geometry.h
	opennurbs_nurbscurve.h
	opennurbs_nurbssurface.h
	opennurbs_object.h
	opennurbs_object_history.h
	opennurbs_objref.h
	opennurbs_offsetsurface.h
	opennurbs_optimize.h
	opennurbs_parse.h
	opennurbs_photogrammetry.h
	opennurbs_plane.h
	opennurbs_planesurface.h
	opennurbs_pluginlist.h
	opennurbs_pointcloud.h
	opennurbs_pointgeometry.h
	opennurbs_pointgrid.h
	opennurbs_point.h
	opennurbs_polycurve.h
	opennurbs_polyedgecurve.h
	opennurbs_polylinecurve.h
	opennurbs_polyline.h
	opennurbs_private_wrap_defs.h
	opennurbs_private_wrap.h
	opennurbs_progress_reporter.h
	opennurbs_public_examples.h
	opennurbs_public.h
	opennurbs_public_version.h
	opennurbs_qsort_template.h
	opennurbs_quacksort_template.h
	opennurbs_quaternion.h
	opennurbs_rand.h
	opennurbs_rendering.h
	opennurbs_revsurface.h
	opennurbs_rtree.h
	opennurbs_sha1.h
	opennurbs_sleeplock.h
	opennurbs_sphere.h
	opennurbs_std_string.h
	opennurbs_string.h
	opennurbs_string_value.h
	opennurbs_subd_data.h
	opennurbs_subd.h
	opennurbs_sumsurface.h
	opennurbs_surface.h
	opennurbs_surfaceproxy.h
	opennurbs_symmetry.h
	opennurbs_system_compiler.h
	opennurbs_system.h
	opennurbs_system_runtime.h
	opennurbs_terminator.h
	opennurbs_testclass.h
	opennurbs_textcontext.h
	opennurbs_textdraw.h
	opennurbs_textglyph.h
	opennurbs_text.h
	opennurbs_textiterator.h
	opennurbs_textlog.h
	opennurbs_textobject.h
	opennurbs_textrun.h
	opennurbs_text_style.h
	opennurbs_texture.h
	opennurbs_texture_mapping.h
	opennurbs_topology.h
	opennurbs_torus.h
	opennurbs_unicode.h
	opennurbs_userdata.h
	opennurbs_uuid.h
	opennurbs_version.h
	opennurbs_version_number.h
	opennurbs_viewport.h
	opennurbs_windows_targetver.h
	opennurbs_win_dwrite.h
	opennurbs_wip.h
	opennurbs_workspace.h
	opennurbs_x.h
	opennurbs_xform.h
	opennurbs_zlib.h
	)

include(CheckIncludeFiles)
check_include_files(emmintrin.h HAVE_EMMINTRIN_H)
check_include_files(ieeefp.h HAVE_IEEEFP_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
if(HAVE_EMMINTRIN_H)
  add_definitions(-DHAVE_EMMINTRIN_H)
endif(HAVE_EMMINTRIN_H)
if(HAVE_IEEEFP_H)
  add_definitions(-DHAVE_IEEEFP_H)
endif(HAVE_IEEEFP_H)
if(HAVE_SYS_STAT_H)
  add_definitions(-DHAVE_SYS_STAT_H)
endif(HAVE_SYS_STAT_H)

if(BRLCAD_ENABLE_OPENNURBS_EXTENSIONS)
  add_definitions(-DBRLCAD_FEATURE_EXTEND_UV_OVER_CLOSED_SEAMS)
endif(BRLCAD_ENABLE_OPENNURBS_EXTENSIONS)

find_package(ZLIB REQUIRED)

set(OPENNURBS_LINKLIBRARIES ${ZLIB_LIBRARIES})

# Ideally, we should only need ZLIB here
include_directories(
  ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/include
  ${ZLIB_INCLUDE_DIRS}
  )

add_definitions(-DON_COMPILING_OPENNURBS)
if (MSVC)
  add_definitions(-DUNICODE)
  set(OPENNURBS_LINKLIBRARIES ${OPENNURBS_LINKLIBRARIES} Rpcrt4)
endif(MSVC)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-O3 HAVE_O3)
  if(HAVE_O3)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
  endif(HAVE_O3)
endif("${CMAKE_BUILD_TYPE}" STREQUAL "Release")

# Based on the options, define libraries as either stand-alone builds
# or dependencies on OBJECT library builds
if(NOT USE_OBJECT_LIBS)
 
  if(BUILD_SHARED_LIBS)
    add_library(openNURBS SHARED ${OPENNURBS_SOURCES})
    if(MSVC)
      set_property(TARGET openNURBS APPEND PROPERTY COMPILE_DEFINITIONS "OPENNURBS_EXPORTS")
    endif(MSVC)
  endif(BUILD_SHARED_LIBS)

  if(BUILD_STATIC_LIBS)
    add_library(openNURBS-static STATIC ${OPENNURBS_SOURCES})
    if(TARGET zlib_stage)
      add_dependencies(openNURBS-static zlib_stage)
    endif(TARGET zlib_stage)
  endif(BUILD_STATIC_LIBS)

else(NOT USE_OBJECT_LIBS)

  add_library(openNURBS-obj OBJECT ${OPENNURBS_SOURCES})

  # For build ordering, if we are linking to another build target make sure
  # the object build depends on that target
  foreach(DEP ${OPENNURBS_LINKLIBRARIES})
    if(TARGET ${DEP})
      add_dependencies(openNURBS-obj ${DEP})
    endif(TARGET ${DEP})
  endforeach(DEP ${OPENNURBS_LINKLIBRARIES})

  if(BUILD_SHARED_LIBS)
    add_library(openNURBS SHARED $<TARGET_OBJECTS:openNURBS-obj>)
    if(MSVC)
      set_property(TARGET openNURBS APPEND PROPERTY COMPILE_DEFINITIONS "OPENNURBS_EXPORTS")
    endif(MSVC)
  endif(BUILD_SHARED_LIBS)
  if(BUILD_STATIC_LIBS)
    add_library(openNURBS-static STATIC $<TARGET_OBJECTS:openNURBS-obj>)
    if(TARGET zlib_stage)
      add_dependencies(openNURBS-static zlib_stage)
    endif(TARGET zlib_stage)
  endif(BUILD_STATIC_LIBS)
endif(NOT USE_OBJECT_LIBS)


if(BUILD_SHARED_LIBS)
  if(OPENNURBS_LINKLIBRARIES)
    target_link_libraries(openNURBS ${OPENNURBS_LINKLIBRARIES})
  endif(OPENNURBS_LINKLIBRARIES)
  set_target_properties(openNURBS PROPERTIES VERSION "2012.10.245")
  install(TARGETS openNURBS
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_SHARED_LIBS)

if(BUILD_STATIC_LIBS)
  if(MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(openNURBS-static PROPERTIES PREFIX "lib")
  else(MSVC)
    set_target_properties(openNURBS-static PROPERTIES OUTPUT_NAME "openNURBS")
  endif(MSVC)
  install(TARGETS openNURBS-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_STATIC_LIBS)

install(FILES ${OPENNURBS_HEADERS} DESTINATION include/openNURBS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

