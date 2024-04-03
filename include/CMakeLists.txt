add_subdirectory(analyze)
add_subdirectory(bu)
add_subdirectory(bn)
add_subdirectory(bg)
add_subdirectory(brep)
add_subdirectory(bv)
add_subdirectory(dm)
add_subdirectory(gcv)
add_subdirectory(ged)
add_subdirectory(icv)
add_subdirectory(nmg)
add_subdirectory(optical)
add_subdirectory(qtcad)
add_subdirectory(rt)
add_subdirectory(tclcad)

BRLCAD_MANAGE_FILES("analyze.h" ${INCLUDE_DIR}/brlcad REQUIRED libanalyze)
BRLCAD_MANAGE_FILES("bg.h" ${INCLUDE_DIR}/brlcad REQUIRED libbg)
BRLCAD_MANAGE_FILES("bio.h" ${INCLUDE_DIR}/brlcad)
BRLCAD_MANAGE_FILES("bn.h" ${INCLUDE_DIR}/brlcad REQUIRED libbn)
BRLCAD_MANAGE_FILES("bnetwork.h" ${INCLUDE_DIR}/brlcad)
BRLCAD_MANAGE_FILES("brep.h" ${INCLUDE_DIR}/brlcad REQUIRED libbrep)
BRLCAD_MANAGE_FILES("bresource.h" ${INCLUDE_DIR}/brlcad)
BRLCAD_MANAGE_FILES("brlcad.h" ${INCLUDE_DIR}/brlcad REQUIRED libwdb)
BRLCAD_MANAGE_FILES("bsocket.h" ${INCLUDE_DIR}/brlcad)
BRLCAD_MANAGE_FILES("bu.h" ${INCLUDE_DIR}/brlcad REQUIRED libbu)
BRLCAD_MANAGE_FILES("bv.h" ${INCLUDE_DIR}/brlcad REQUIRED libbv)
BRLCAD_MANAGE_FILES("common.h" ${INCLUDE_DIR}/brlcad)
BRLCAD_MANAGE_FILES("dm.h" ${INCLUDE_DIR}/brlcad REQUIRED libdm)
BRLCAD_MANAGE_FILES("fft.h" ${INCLUDE_DIR}/brlcad REQUIRED libfft)
BRLCAD_MANAGE_FILES("gcv.h" ${INCLUDE_DIR}/brlcad REQUIRED libgcv)
BRLCAD_MANAGE_FILES("ged.h" ${INCLUDE_DIR}/brlcad REQUIRED libged)
BRLCAD_MANAGE_FILES("icv.h" ${INCLUDE_DIR}/brlcad REQUIRED libicv)
BRLCAD_MANAGE_FILES("nmg.h" ${INCLUDE_DIR}/brlcad REQUIRED libnmg)
BRLCAD_MANAGE_FILES("optical.h" ${INCLUDE_DIR}/brlcad REQUIRED liboptical)
BRLCAD_MANAGE_FILES("pc.h" ${INCLUDE_DIR}/brlcad REQUIRED librt)
BRLCAD_MANAGE_FILES("photonmap.h" ${INCLUDE_DIR}/brlcad REQUIRED liboptical)
BRLCAD_MANAGE_FILES("pinttypes.h" ${INCLUDE_DIR}/brlcad)
BRLCAD_MANAGE_FILES("pkg.h" ${INCLUDE_DIR}/brlcad REQUIRED libpkg)
BRLCAD_MANAGE_FILES("pstdint.h" ${INCLUDE_DIR}/brlcad)
BRLCAD_MANAGE_FILES("raytrace.h" ${INCLUDE_DIR}/brlcad REQUIRED librt)
BRLCAD_MANAGE_FILES("tclcad.h" ${INCLUDE_DIR}/brlcad REQUIRED libtclcad)
BRLCAD_MANAGE_FILES("vmath.h" ${INCLUDE_DIR}/brlcad)
BRLCAD_MANAGE_FILES("wdb.h" ${INCLUDE_DIR}/brlcad REQUIRED libwdb)

BUILD_CFG_HDR(brlcad_ident.h.in ${INCLUDE_DIR}/brlcad)

# Define the amendment count if we have one
set(BRLCAD_VERSION_AMEND_STR "")
if (BRLCAD_VERSION_AMEND)
  set(BRLCAD_VERSION_AMEND_STR "#define BRLCAD_VERSION_AMEND ${BRLCAD_VERSION_AMEND}")
endif (BRLCAD_VERSION_AMEND)

# Generate the version header
BUILD_CFG_HDR(brlcad_version.h.in ${INCLUDE_DIR}/brlcad)

# FIXME: shouldn't exist, needs to be absorbed into brlcad_config.h
BUILD_CFG_HDR(config_win.h.in ${INCLUDE_DIR}/brlcad)

set(include_misc
  CMakeLists.txt
  brlcad_ident.h.in
  brlcad_version.h.in
  conf/BrlcadConfig.tmpl
  conf/MAJOR
  conf/MINOR
  conf/PATCH
  conf/make.vbs
  config_win.h.in
  RTree.h
  )
CMAKEFILES(${include_misc})

# Header checking target - this compiles the headers as isolated files to
# ensure they include what is needed to resolve any symbols defined in the
# public API.

# Note: don't know how to do this on Windows - skip with Visual Studio
if (DEFINED BRLCAD_HDR_CHECK AND NOT MSVC)

  add_custom_target(check-headers)
  set_target_properties(check-headers PROPERTIES FOLDER "BRL-CAD Header Checks")

  file(GLOB_RECURSE hdrfiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.h")

  foreach(hdrfile ${hdrfiles})

    if (NOT "${hdrfile}" STREQUAL "RtServerImpl.h")

      string(REPLACE "-" "_" STMPFILE "${hdrfile}")
      string(REPLACE "." "_" STMPFILE "${STMPFILE}")
      string(REPLACE "/" "_" STMPFILE "${STMPFILE}")

      add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${STMPFILE}.stamp
	COMMAND ${CMAKE_CXX_COMPILER} -I${CMAKE_SOURCE_DIR}/include -I${CMAKE_BINARY_DIR}/include
	-I${CMAKE_SOURCE_DIR}/src/other/openNURBS -I${CMAKE_SOURCE_DIR}/src/other/tcl/generic
	-fsyntax-only -Wall -Wextra -Wno-deprecated
	${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile}
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${STMPFILE}.stamp
	DEPENDS ${hdrfile}
	)
      add_custom_target(${STMPFILE}-check DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${STMPFILE}.stamp)
      set_target_properties(${STMPFILE}-check PROPERTIES FOLDER "BRL-CAD Header Checks")

      add_dependencies(check-headers ${STMPFILE}-check)

    endif (NOT "${hdrfile}" STREQUAL "RtServerImpl.h")

  endforeach(hdrfile ${hdrfiles})

endif (DEFINED BRLCAD_HDR_CHECK AND NOT MSVC)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
