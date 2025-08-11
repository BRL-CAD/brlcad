set(
  NIRT_DBG_HDRS
  ${BRLCAD_SOURCE_DIR}/include/bu/debug.h
  ${BRLCAD_SOURCE_DIR}/include/nmg/debug.h
  ${BRLCAD_SOURCE_DIR}/include/rt/debug.h
  ${BRLCAD_SOURCE_DIR}/include/analyze/debug.h
)
set(DEBUG_CMD_C "${CMAKE_CURRENT_BINARY_DIR}/debug_cmd.c")
set(DEBUG_CMD_HDRS "${CMAKE_CURRENT_BINARY_DIR}/nirt_dbg_hdrs.cmake")

file(REMOVE "${DEBUG_CMD_HDRS}")
foreach(hdr ${NIRT_DBG_HDRS})
  file(APPEND "${DEBUG_CMD_HDRS}" "${hdr}\n")
endforeach(hdr ${NIRT_DBG_HDRS})

add_custom_command(
  OUTPUT ${DEBUG_CMD_C}
  COMMAND debug2c ${DEBUG_CMD_HDRS} ${DEBUG_CMD_C}
  DEPENDS debug2c
  COMMENT "[DEBUG2C] Scanning for NIRT debugging variables"
)
set_source_files_properties(nirt/nirt.cpp PROPERTIES OBJECT_DEPENDS ${DEBUG_CMD_C})
distclean(${DEBUG_CMD_C})
distclean(${DEBUG_CMD_HDRS})

set(
  LIBANALYZE_SOURCES
  api.c
  centroid.c
  check_options.c
  density.cpp
  globals.c
  inside.c
  mass.c
  moments.c
  nirt/nirt.cpp
  nirt/opts.cpp
  nirt/diff.cpp
  obj_to_pnts.cpp
  overlaps.c
  polygonizer.c
  raydiff.c
  #find_subtracted_shapes.cpp
  surf_area.c
  util.cpp
  volume.c
  voxels.c
  MeshHealing/MeshConversion_brlcad.cpp
  MeshHealing/MeshConversion.cpp
  MeshHealing/Zipper.cpp
  MeshHealing/Geometry.cpp
  MeshHealing/Stitch.cpp
  heal_mesh.cpp
  GridGeneration/rectangular_grid.c
)

set(
  libanalyze_ignored_files
  MeshHealing/DCEL.h
  MeshHealing/Geometry.h
  MeshHealing/MeshConversion.h
  MeshHealing/MeshConversion_brlcad.h
  MeshHealing/Stitch.h
  MeshHealing/Zipper.h
  nirt/nirt.h
)
cmakefiles(${libanalyze_ignored_files})

# Note - libanalyze_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
brlcad_addlib(libanalyze "${LIBANALYZE_SOURCES}" "${libanalyze_deps}" "" "${CMAKE_CURRENT_BINARY_DIR}")
set_target_properties(libanalyze PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

cmakefiles(
  CMakeLists.txt
  analyze_private.h
  find_subtracted_shapes.cpp
  wnsurface.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
