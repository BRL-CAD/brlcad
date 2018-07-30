# Include directories needed by libanalyze users
set(ANALYZE_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(analyze ANALYZE_INCLUDE_DIRS "")

set(LIBANALYZE_SOURCES
  api.c
  centroid.c
  check_options.c
  density.c
  inside.c
  mass.c
  moments.c
  nirt.cxx
  outer_pnts.c
  overlaps.c
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
  overlaps/analyze_overlaps.c
  GridGeneration/rectangular_grid.c
  )

set(libanalyze_ignored_files
  MeshHealing/DCEL.h
  MeshHealing/Geometry.h
  MeshHealing/MeshConversion.h
  MeshHealing/MeshConversion_brlcad.h
  MeshHealing/Stitch.h
  MeshHealing/Zipper.h
)
CMAKEFILES(${libanalyze_ignored_files})

BRLCAD_ADDLIB(libanalyze "${LIBANALYZE_SOURCES}" "librt;libbu")
set_target_properties(libanalyze PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

CMAKEFILES(analyze_private.h find_subtracted_shapes.cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
